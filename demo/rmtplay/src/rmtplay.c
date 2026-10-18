/* rmtplay.c - RMT player with a visualizer on the Commander X16, in the
 * style of the XEX/SAP export of Raster Music Tracker.  Port of RMTPLAY of
 * VERA_ATARI_PBI (vera-tests/rmt/rmtplay.c), which runs on an Atari with a
 * VERA card: here only the VERA screen and the VERA PSG are left.
 *
 *   NAME / AUTHOR / DATE of the song (tools/rmtinfo.py: text stored in the
 *   .rmt file, or RMT_NAME= RMT_AUTHOR= RMT_DATE= on the make line); a line
 *   longer than 40 characters scrolls left (coarse, one character at a time)
 *   after 5 seconds of play
 *   one volume bar per channel (4 for RMT4, 8 for RMT8)
 *   AUDF / AUDC of every channel and AUDCTL, in hex (the POKEY registers the
 *   player computes; psgrmt.s turns them into PSG voices)
 *   song line, row in the track, speed, play time; for a .sap/.xex song
 *   (RMT_LZSS, lzsplayr.s) the section of the LZSS stream (intro, loop) and
 *   the frame in it
 *
 *   SPACE  pause / play     R  restart the song     S  stereo on/off
 *   Q, ESC or RUN/STOP      stop and exit
 *
 * Screen: layer 1, 40x30 text (320x240) in 1bpp tiles with 256-colour
 * foreground (T256C), the upper / lower case X16 character set (VRAM
 * $1F000) copied to $01000
 * with the bar glyphs; the bar rows get a red..green colour each.  Map at
 * $00000, away from the KERNAL screen ($1B000): on exit the layer registers
 * and the palette entries are put back and the KERNAL screen shows again.
 *
 * Sound: rmtplayr.s or lzsplayr.s on a 300 Hz VIA1 timer IRQ (rmtx16.s:
 * exact 50 / 60 Hz song frames) + psgrmt.s.
 */

#include <cbm.h>
#include <conio.h>
#include <string.h>
#include "rmt.h"
#include "songinfo.h"

/* cc65 is much faster with static locals; nothing here is recursive */
#pragma static-locals (on)

/* ---- player state (rmtplayr.s, lzsplayr.s, song.s) -------------------- */

extern volatile unsigned char *rmt_p_song;          /* next song line */
#pragma zpsym ("rmt_p_song")
extern volatile unsigned char rmt_abeat;            /* row in the track */
extern volatile unsigned char rmt_maxtracklen;      /* rows per track */
extern volatile unsigned char rmt_speed;            /* frames per row */
extern volatile unsigned char rmt_chan_audf[RMT_TRACKS];
extern volatile unsigned char rmt_chan_audc[RMT_TRACKS];
extern volatile unsigned char rmt_audctl, rmt_audctl2;
extern const unsigned char rmt_song_end[];
/* LZSS player (lzsplayr.s, a .sap/.xex song): stream position */
extern volatile unsigned char lzs_section;          /* section of the next frame */
extern volatile unsigned char lzs_pos[3];           /* its frame in the section, 24 bit */

/* ---- VERA ----------------------------------------------------------------- */

#define VREG(n)         (*(volatile unsigned char *)(0x9F20 + (n)))
#define VERA_ADDR_L     VREG(0x00)
#define VERA_ADDR_M     VREG(0x01)
#define VERA_ADDR_H     VREG(0x02)
#define VERA_DATA0      VREG(0x03)
#define VERA_DATA1      VREG(0x04)
#define VERA_CTRL       VREG(0x05)
#define VERA_DC_VIDEO   VREG(0x09)
#define VERA_DC_HSCALE  VREG(0x0A)
#define VERA_DC_VSCALE  VREG(0x0B)
#define VERA_DC_BORDER  VREG(0x0C)
#define VERA_L1_FIRST   0x14            /* CONFIG MAPBASE TILEBASE HSCROLL VSCROLL */
#define VERA_L1_COUNT   7

#define VERA_INC1       0x10
#define VERA_INC2       0x20

#define VMAP            0x0000u         /* 64x32 map, 128 bytes per row */
#define VFONT           0x1000u         /* 256 glyphs of 8 bytes */
#define KFONT_BANK      1               /* the KERNAL character set: $1F000 */
#define KFONT           0xF000u
#define VPAL_BANK       1               /* palette at $1FA00 */
#define VPAL            0xFA00u
#define V_ROW_OFS       3               /* 24 rows centred in 30 */

#define L1_CONFIG_VAL   0x18            /* map 64x32, T256C, 1bpp tiles */
#define L1_MAPBASE_VAL  (VMAP >> 9)
#define L1_TILEBASE_VAL ((VFONT >> 11) << 2)   /* 8x8 tiles */

/* palette entries used (default palette 0 = black stays the background) */
#define COL_TEXT        0x21
#define COL_HEAD        0x22
#define COL_BAR         0x28            /* + bar row, 0 = top */
#define COL_FIRST       COL_TEXT
#define COL_LAST        (COL_BAR + BAR_ROWS - 1)

/* ---- screen layout ---------------------------------------------------- */

#define ROW_TITLE   0
#define ROW_NAME    2
#define ROW_AUTHOR  3
#define ROW_DATE    4
#define BAR_TOP     6       /* 8 rows of bars: 64 pixels, 4 per volume step */
#define BAR_ROWS    8
#define ROW_LABELS  (BAR_TOP + BAR_ROWS)
#define ROW_AUDF    16
#define ROW_AUDC    17
#define ROW_AUDCTL  18
#define ROW_POS     20
#define ROW_STATE   21
#define ROW_KEYS1   23

#if RMT_TRACKS > 4
#define BAR_W       3       /* 8 bars: 3 wide, 2 apart */
#define BAR_STEP    5
#define BAR_X0      1
#else
#define BAR_W       6       /* 4 bars: 6 wide, 3 apart */
#define BAR_STEP    9
#define BAR_X0      3
#endif

/* bar glyphs: screen codes $61-$68 (graphics of the upper / lower case
 * character set, unused here) */
#define GLYPH_BASE  0x60

/* VERA map address of each row: 128 bytes per row, the 24 rows centred in 30 */
#define VROW(y)     (VMAP + ((y) + V_ROW_OFS) * 128)
static const unsigned int vera_row[24] = {
    VROW(0), VROW(1), VROW(2), VROW(3), VROW(4), VROW(5), VROW(6), VROW(7), VROW(8), VROW(9), VROW(10), VROW(11),
    VROW(12), VROW(13), VROW(14), VROW(15), VROW(16), VROW(17), VROW(18), VROW(19), VROW(20), VROW(21), VROW(22), VROW(23)
};
static unsigned char bar_px[RMT_TRACKS];    /* height shown, with fall-off */
static unsigned char bar_shown[RMT_TRACKS]; /* height drawn on screen */
static unsigned char line_buf[40];
static unsigned char vera_saved[4 + VERA_L1_COUNT];
static unsigned char pal_saved[2 * (COL_LAST - COL_FIRST + 1)];

/* value shown in each field: a field is written again only when it changes
 * (VERA writes cost CPU time; $FFFF = not drawn yet) */
enum {
    F_AUDF = 0, F_AUDC = 8, F_CTL = 16, F_LINE = 18, F_LINES, F_ROW, F_ROWS,
    F_SPEED, F_SEC, F_STEREO, F_PAUSE, F_COUNT
};
static unsigned int shown[F_COUNT];

static unsigned char changed(unsigned char f, unsigned int v)
{
    if (shown[f] == v)
        return 0;
    shown[f] = v;
    return 1;
}
#ifdef RMT_LZSS
/* rmt_play calls of each section (sap2ca65.py) */
static const unsigned long sec_frames[RMT_INFO_SECTIONS] = RMT_INFO_FRAMES;
#else
static unsigned char song_shift;            /* bytes per song line: 1 << shift */
static const unsigned char *song_start;
static unsigned int song_lines;
#endif

/* ---- screen helpers --------------------------------------------------- */

/* cc65 strings are PETSCII on the X16: the screen code in the upper /
 * lower case character set (main() selects it), lower case letters at 1-26,
 * upper case at $41-$5A */
static unsigned char screen_code(unsigned char c)
{
    if (c >= 0x40 && c < 0x60)
        return c - 0x40;
    if (c >= 0xC0 && c < 0xE0)
        return c - 0x80;
    if (c >= 0x60 && c < 0x80)
        return c - 0x20;
    if (c >= 0x20 && c < 0x40)
        return c;
    return 0x20;
}

/* a macro, not a function: it is on the path of every screen write */
#define vera_seek(bank, addr, inc) do { unsigned int a_ = (addr); \
    VERA_ADDR_L = (unsigned char)a_; VERA_ADDR_M = (unsigned char)(a_ >> 8); VERA_ADDR_H = (inc) | (bank); } while (0)

/* Every screen write goes through these two: characters only, step 2 (the
 * colours of the map are set once). */
static void put_codes(unsigned char x, unsigned char y, const unsigned char *c, unsigned char n)
{
    unsigned char i;

    vera_seek(0, vera_row[y] + (x << 1), VERA_INC2);
    for (i = 0; i < n; i++)
        VERA_DATA0 = c[i];
}

static void fill_codes(unsigned char x, unsigned char y, unsigned char c, unsigned char n)
{
    unsigned char i;

    vera_seek(0, vera_row[y] + (x << 1), VERA_INC2);
    for (i = 0; i < n; i++)
        VERA_DATA0 = c;
}

static void put_text(unsigned char x, unsigned char y, const char *s, unsigned char inv)
{
    unsigned char n = 0;

    while (s[n] && x + n < 40) {
        line_buf[n] = screen_code((unsigned char)s[n]) | inv;
        n++;
    }
    put_codes(x, y, line_buf, n);
}

static void put_center(unsigned char y, const char *s)
{
    unsigned char n = (unsigned char)strlen(s);

    put_text(n < 40 ? (40 - n) / 2 : 0, y, s, 0);
}

/* the hex digits as screen codes */
static const unsigned char hexcode[16] = {
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46
};

static void put_hex(unsigned char x, unsigned char y, unsigned char v)
{
    vera_seek(0, vera_row[y] + (x << 1), VERA_INC2);
    VERA_DATA0 = hexcode[v >> 4];
    VERA_DATA0 = hexcode[v & 15];
}

/* two decimal digits of v (0..99): the tens by subtraction */
static void put_dec2(unsigned char x, unsigned char y, unsigned char v)
{
    unsigned char t = 0x30;     /* screen code of '0' */

    while (v >= 10) {
        v -= 10;
        ++t;
    }
    line_buf[0] = t;
    line_buf[1] = 0x30 + v;
    put_codes(x, y, line_buf, 2);
}

/* ---- VERA screen -------------------------------------------------------- */

static void vera_color(unsigned char idx, unsigned char r, unsigned char g, unsigned char b)
{
    vera_seek(VPAL_BANK, VPAL + idx * 2, VERA_INC1);
    VERA_DATA0 = (unsigned char)((g << 4) | b);
    VERA_DATA0 = r;
}

/* red at the top, yellow in the middle, green at the bottom (4-bit RGB) */
static const unsigned char bar_rgb[BAR_ROWS][3] = {
    { 15, 2, 2 }, { 15, 6, 0 }, { 15, 10, 0 }, { 15, 14, 0 },
    { 11, 15, 0 }, { 7, 14, 0 }, { 3, 13, 0 }, { 0, 12, 0 }
};

static void vera_screen_on(void)
{
    unsigned int i;
    unsigned char c, r, b, attr;

    VERA_CTRL = 0;
    vera_saved[0] = VERA_DC_VIDEO;
    vera_saved[1] = VERA_DC_HSCALE;
    vera_saved[2] = VERA_DC_VSCALE;
    vera_saved[3] = VERA_DC_BORDER;
    for (c = 0; c < VERA_L1_COUNT; c++)
        vera_saved[4 + c] = VREG(VERA_L1_FIRST + c);
    vera_seek(VPAL_BANK, VPAL + COL_FIRST * 2, VERA_INC1);
    for (c = 0; c < sizeof pal_saved; c++)
        pal_saved[c] = VERA_DATA0;

    /* font: the KERNAL one (port 1 reads it, port 0 writes the copy), the
     * bar glyphs drawn over codes GLYPH_BASE + 1..8: the bottom h pixel
     * rows lit, as LED segments of 3 lit rows and 1 dark row (one segment
     * per volume step), 6 pixels wide so the bar columns stay apart */
    VERA_CTRL = 1;
    vera_seek(KFONT_BANK, KFONT, VERA_INC1);
    VERA_CTRL = 0;
    vera_seek(0, VFONT, VERA_INC1);
    for (i = 0; i < 2048; i++) {
        c = VERA_DATA1;
        if (i >= (GLYPH_BASE + 1) * 8 && i < (GLYPH_BASE + 9) * 8) {
            r = (unsigned char)(i & 7);
            b = 7 - r;      /* pixel row counted from the bottom */
            c = ((b < (i >> 3) - GLYPH_BASE) && (b & 3) != 3) ? 0xFC : 0x00;
        }
        VERA_DATA0 = c;
    }

    vera_color(COL_TEXT, 12, 12, 12);
    vera_color(COL_HEAD, 6, 10, 15);
    for (r = 0; r < BAR_ROWS; r++)
        vera_color(COL_BAR + r, bar_rgb[r][0], bar_rgb[r][1], bar_rgb[r][2]);

    /* map: blank, with the colour of each row */
    vera_seek(0, VMAP, VERA_INC1);
    for (r = 0; r < 32; r++) {
        attr = COL_TEXT;
        if (r == ROW_TITLE + V_ROW_OFS)
            attr = COL_HEAD;
        else if (r >= BAR_TOP + V_ROW_OFS && r < BAR_TOP + BAR_ROWS + V_ROW_OFS)
            attr = COL_BAR + (r - BAR_TOP - V_ROW_OFS);
        for (c = 0; c < 64; c++) {
            VERA_DATA0 = 0x20;
            VERA_DATA0 = attr;
        }
    }

    VREG(VERA_L1_FIRST + 0) = L1_CONFIG_VAL;
    VREG(VERA_L1_FIRST + 1) = L1_MAPBASE_VAL;
    VREG(VERA_L1_FIRST + 2) = L1_TILEBASE_VAL;
    for (c = 3; c < VERA_L1_COUNT; c++)
        VREG(VERA_L1_FIRST + c) = 0;        /* no scroll */
    VERA_DC_HSCALE = 64;                    /* 320x240 */
    VERA_DC_VSCALE = 64;
    VERA_DC_BORDER = 0;
    VERA_DC_VIDEO = (vera_saved[0] & 0x0F) | 0x20;   /* layer 1 only */
}

static void vera_screen_off(void)
{
    unsigned char c;

    VERA_CTRL = 0;
    VERA_DC_VIDEO = vera_saved[0];
    VERA_DC_HSCALE = vera_saved[1];
    VERA_DC_VSCALE = vera_saved[2];
    VERA_DC_BORDER = vera_saved[3];
    for (c = 0; c < VERA_L1_COUNT; c++)
        VREG(VERA_L1_FIRST + c) = vera_saved[4 + c];
    vera_seek(VPAL_BANK, VPAL + COL_FIRST * 2, VERA_INC1);
    for (c = 0; c < sizeof pal_saved; c++)
        VERA_DATA0 = pal_saved[c];
}

/* ---- static parts of the screen ---------------------------------------- */

#define SCROLL_WAIT     5       /* seconds of play before a long line scrolls */
#define SCROLL_SHIFT    3       /* one character every 8 frames */
#define SCROLL_GAP      40      /* blanks between the end and the start again:
                                 * a whole row, so the text leaves the screen
                                 * before it comes back in from the right */
#define SCROLL_SEEK     3       /* VERA writes of a new address: unchanged
                                 * gaps up to this long are written over */
#define BLANK           0x20    /* screen code of a space */

/* NAME, AUTHOR, DATE: on rows ROW_NAME, ROW_NAME + 1, ROW_NAME + 2 */
static const char *const info_text[3] = { RMT_INFO_NAME, RMT_INFO_AUTHOR, RMT_INFO_DATE };
static unsigned char info_len[3];
static unsigned char info_shown[3];         /* scroll offset drawn */
static unsigned char info_cells[3][40];     /* screen codes on the row */
/* screen codes of a long line (rmtinfo.py keeps 200 characters), as long as
 * the line, one byte for a line that fits on the screen */
#define INFO_CODES(t)   (sizeof(t) - 1 > 40 ? sizeof(t) - 1 : 1)
static unsigned char info_codes0[INFO_CODES(RMT_INFO_NAME)];
static unsigned char info_codes1[INFO_CODES(RMT_INFO_AUTHOR)];
static unsigned char info_codes2[INFO_CODES(RMT_INFO_DATE)];
static unsigned char *const info_codes[3] = { info_codes0, info_codes1, info_codes2 };

#if SCROLL_GAP < 40
#error "info_render() needs the blanks of a whole row"
#endif

/* The 40 cells of a long info line from scroll offset k: its screen codes
 * (draw_static), then the SCROLL_GAP blanks; with a gap of a whole row the
 * 40 cells are the text then blanks, or blanks then the text again. */
static void info_render(unsigned char i, unsigned char k, unsigned char *buf)
{
    const unsigned char *s = info_codes[i];
    unsigned char len = info_len[i], n;

    if (k < len) {
        n = len - k;
        if (n > 40)
            n = 40;
        memcpy(buf, s + k, n);
        memset(buf + n, BLANK, 40 - n);
    } else {
        n = len + SCROLL_GAP - k;   /* blanks before the text comes round again: 1..40 */
        memset(buf, BLANK, n);
        memcpy(buf + n, s, 40 - n);
    }
}

static void draw_static(void)
{
    unsigned char i, x;
    char title[41];

    memset(title, ' ', 40);
    title[40] = 0;
    memcpy(title + 1, "RMT PLAYER  VERA PSG  X16", 25);
#if RMT_TRACKS > 4
    memcpy(title + 31, "RMT8", 4);
#else
    memcpy(title + 31, "RMT4", 4);
#endif
    memcpy(title + 36, rmt_rate == 50 ? "50HZ" : "60HZ", 4);
    put_text(0, ROW_TITLE, title, 0x80);

    /* a line longer than the screen shows its first 40 characters until
     * draw_info() starts scrolling it */
    for (i = 0; i < 3; i++) {
        info_len[i] = (unsigned char)strlen(info_text[i]);
        if (info_len[i] <= 40) {
            put_center(ROW_NAME + i, info_text[i]);
            continue;
        }
        for (x = 0; x < info_len[i]; x++)
            info_codes[i][x] = screen_code((unsigned char)info_text[i][x]);
        info_shown[i] = 0;
        info_render(i, 0, info_cells[i]);
        put_codes(0, ROW_NAME + i, info_cells[i], 40);
    }

    for (i = 0; i < RMT_TRACKS; i++) {
        x = BAR_X0 + i * BAR_STEP + (BAR_W - 2) / 2;
#if RMT_TRACKS > 4
        line_buf[0] = screen_code(i < 4 ? 'L' : 'R');
        line_buf[1] = screen_code('1' + (i & 3));
        put_codes(x, ROW_LABELS, line_buf, 2);
#else
        line_buf[0] = screen_code('1' + i);
        put_codes(x + 1, ROW_LABELS, line_buf, 1);
#endif
    }

    put_text(0, ROW_AUDF, "AUDF", 0);
    put_text(0, ROW_AUDC, "AUDC", 0);
    put_text(0, ROW_AUDCTL, "AUDCTL", 0);
#ifdef RMT_LZSS
    put_text(0, ROW_POS, "SECT   /     FRAME       /", 0);
#else
    put_text(0, ROW_POS, "LINE   /      ROW   /     SPEED", 0);
#endif
    put_text(0, ROW_STATE, "TIME   :      OUT VERA PSG", 0);
    put_text(0, ROW_KEYS1, "SPC PAUSE  R RESTART  S STEREO  Q EXIT", 0);
}

/* ---- dynamic parts ----------------------------------------------------- */

/* Coarse scroll of the info lines longer than 40 characters: after
 * SCROLL_WAIT seconds of play one character every 2^SCROLL_SHIFT song
 * frames.  Only the cells that change are written (info_cells[] keeps the
 * row); changed cells close together go out as one run. */
static unsigned int info_wait;              /* frames of play before the long lines scroll */
static unsigned char info_sub;              /* frames since the last scroll step */

static void info_reset(unsigned char fps)
{
    unsigned char i;

    info_wait = fps * SCROLL_WAIT;
    info_sub = 0;
    for (i = 0; i < 3; i++)
        if (info_len[i] > 40 && info_shown[i]) {
            info_shown[i] = 0;
            info_render(i, 0, info_cells[i]);
            put_codes(0, ROW_NAME + i, info_cells[i], 40);
        }
}

static void draw_info(unsigned char adv)    /* frames played since the last call */
{
    unsigned char i, k, n, x, start, end, period;
    unsigned char *old;

    if (info_wait) {
        if (info_wait > adv) {
            info_wait -= adv;
            return;
        }
        adv -= (unsigned char)info_wait;
        info_wait = 0;
    }
    info_sub += adv;
    n = info_sub >> SCROLL_SHIFT;
    if (!n)
        return;
    info_sub &= (1 << SCROLL_SHIFT) - 1;
    for (i = 0; i < 3; i++) {
        if (info_len[i] <= 40)
            continue;
        period = info_len[i] + SCROLL_GAP;
        k = info_shown[i] + n;
        while (k >= period)
            k -= period;
        info_shown[i] = k;
        info_render(i, k, line_buf);
        old = info_cells[i];
        x = 0;
        while (x < 40) {
            if (line_buf[x] == old[x]) {
                x++;
                continue;
            }
            /* run from the first changed cell to the last changed one
             * before a gap longer than SCROLL_SEEK */
            start = x;
            end = ++x;
            for (; x < 40; x++) {
                if (line_buf[x] != old[x])
                    end = x + 1;
                else if (x - end >= SCROLL_SEEK)
                    break;
            }
            put_codes(start, ROW_NAME + i, line_buf + start, end - start);
            memcpy(old + start, line_buf + start, end - start);
        }
    }
}

/* the left column of each bar */
static const unsigned char bar_x[RMT_TRACKS] = {
#if RMT_TRACKS > 4
    BAR_X0, BAR_X0 + BAR_STEP, BAR_X0 + 2 * BAR_STEP, BAR_X0 + 3 * BAR_STEP,
    BAR_X0 + 4 * BAR_STEP, BAR_X0 + 5 * BAR_STEP, BAR_X0 + 6 * BAR_STEP, BAR_X0 + 7 * BAR_STEP
#else
    BAR_X0, BAR_X0 + BAR_STEP, BAR_X0 + 2 * BAR_STEP, BAR_X0 + 3 * BAR_STEP
#endif
};

/* only the rows between the old and the new top of a bar are written */
static void draw_bars(unsigned char paused)
{
    unsigned char i, r, last, h, g, target;

    for (i = 0; i < RMT_TRACKS; i++) {
        target = paused ? 0 : (unsigned char)((rmt_chan_audc[i] & 0x0F) << 2);
        if (target >= bar_px[i])
            bar_px[i] = target;
        else
            bar_px[i] -= (bar_px[i] - target > 2) ? 2 : bar_px[i] - target;

        h = bar_px[i];
        g = bar_shown[i];
        if (h == g)
            continue;       /* only the bars that moved */
        bar_shown[i] = h;
        if (g < h) {        /* r = 0 is the bottom row */
            r = g >> 3;
            last = (h - 1) >> 3;
        } else {
            r = h >> 3;
            last = (g - 1) >> 3;
        }
        for (; r <= last; r++) {
            g = r << 3;     /* pixels of the bar in row r */
            g = h > g ? h - g : 0;
            if (g > 8)
                g = 8;
            g = g ? g + GLYPH_BASE : BLANK;
            fill_codes(bar_x[i], BAR_TOP + BAR_ROWS - 1 - r, g, BAR_W);
        }
    }
}

static void draw_regs(void)
{
    unsigned char i, x;

    for (i = 0; i < RMT_TRACKS; i++) {
        x = 5 + i * 3;
#if RMT_TRACKS > 4
        if (i >= 4)
            x += 2;     /* gap between left and right POKEY */
#endif
        if (changed(F_AUDF + i, rmt_chan_audf[i]))
            put_hex(x, ROW_AUDF, rmt_chan_audf[i]);
        if (changed(F_AUDC + i, rmt_chan_audc[i]))
            put_hex(x, ROW_AUDC, rmt_chan_audc[i]);
    }
    if (changed(F_CTL, rmt_audctl))
        put_hex(7, ROW_AUDCTL, rmt_audctl);
#if RMT_TRACKS > 4
    if (changed(F_CTL + 1, rmt_audctl2))
        put_hex(10, ROW_AUDCTL, rmt_audctl2);
#endif
}

#ifdef RMT_LZSS
static void put_hex24(unsigned char x, unsigned char y, unsigned long v)
{
    put_hex(x, y, (unsigned char)(v >> 16));
    put_hex(x + 2, y, (unsigned char)(v >> 8));
    put_hex(x + 4, y, (unsigned char)v);
}

/* LZSS: section of the stream and frame in it, of the next frame */
static void draw_position(void)
{
    unsigned char sec, lo, mid, hi;

    /* the IRQ may change them between the reads */
    do {
        sec = lzs_section;
        lo = lzs_pos[0];
        mid = lzs_pos[1];
        hi = lzs_pos[2];
    } while (sec != lzs_section || lo != lzs_pos[0]);

    if (changed(F_LINE, sec)) {
        put_hex(5, ROW_POS, sec);
        put_hex24(26, ROW_POS, sec_frames[sec] - 1);
    }
    if (changed(F_LINES, RMT_INFO_SECTIONS - 1))
        put_hex(8, ROW_POS, RMT_INFO_SECTIONS - 1);
    if (changed(F_ROW, ((unsigned int)mid << 8) | lo) | changed(F_ROWS, hi))
        put_hex24(19, ROW_POS, ((unsigned long)hi << 16) | ((unsigned int)mid << 8) | lo);
}
#else
static void draw_position(void)
{
    const unsigned char *a, *b;
    unsigned int line;

    /* the IRQ may move the pointer between the two byte reads */
    do {
        a = (const unsigned char *)rmt_p_song;
        b = (const unsigned char *)rmt_p_song;
    } while (a != b);
    line = (unsigned int)(a - song_start) >> song_shift;
    line = line ? line - 1 : 0;     /* p_song already points to the next line */

    if (changed(F_LINE, (unsigned char)line))
        put_hex(5, ROW_POS, (unsigned char)line);
    if (changed(F_LINES, (unsigned char)(song_lines - 1)))
        put_hex(8, ROW_POS, (unsigned char)(song_lines - 1));
    if (changed(F_ROW, rmt_abeat))
        put_hex(18, ROW_POS, rmt_abeat);
    if (changed(F_ROWS, (unsigned char)(rmt_maxtracklen - 1)))
        put_hex(21, ROW_POS, (unsigned char)(rmt_maxtracklen - 1));
    if (changed(F_SPEED, rmt_speed))
        put_hex(32, ROW_POS, rmt_speed);
}
#endif

/* the play time: seconds counted from the song frames, minutes and seconds
 * kept apart (no divisions) */
static unsigned char time_frames, time_sec, time_min;

static void count_time(unsigned char elapsed, unsigned char fps)
{
    time_frames += elapsed;
    while (time_frames >= fps) {
        time_frames -= fps;
        if (++time_sec == 60) {
            time_sec = 0;
            if (++time_min == 100)
                time_min = 0;
        }
    }
}

static void draw_state(unsigned char paused)
{
    if (changed(F_SEC, ((unsigned int)time_min << 8) | time_sec)) {
        put_dec2(5, ROW_STATE, time_min);
        put_dec2(8, ROW_STATE, time_sec);
    }
    if (changed(F_STEREO, psg_stereo))
        put_text(27, ROW_STATE, psg_stereo ? "STEREO" : "      ", 0);
    if (changed(F_PAUSE, paused))
        put_text(34, ROW_STATE, paused ? "PAUSED" : "      ", 0x80 * paused);
}

/* ---- main --------------------------------------------------------------- */

int main(void)
{
    unsigned char paused = 0;
    unsigned char fps = rmt_rate;   /* song frames a second */
    unsigned char vlast, odd = 0, k, elapsed;
    unsigned int flast, f;
    unsigned char adv = 0;          /* frames played since the last draw_info() */

#ifndef RMT_LZSS
#if RMT_TRACKS > 4
    song_shift = 3;     /* 8 bytes per song line */
#else
    song_shift = 2;     /* 4 bytes per song line */
#endif
    song_start = (const unsigned char *)(rmt_song[14] | (rmt_song[15] << 8));
    song_lines = (unsigned int)(rmt_song_end - song_start) >> song_shift;
#endif

    memset(shown, 0xFF, sizeof shown);
    cbm_k_chrout(0x0E);     /* the KERNAL loads the upper / lower case set */
    vera_screen_on();
    draw_static();
    info_reset(fps);

    psg_init();
    psg_enable = 1;
    rmt_init(rmt_song);
    rmt_irq_on();

    vlast = rmt_vframes;
    flast = rmt_frames;
    for (;;) {
        /* once per VERA frame */
        while (rmt_vframes == vlast)
            ;
        vlast = rmt_vframes;
        f = rmt_frames;
        elapsed = (unsigned char)(f - flast);
        flast = f;
        if (!paused) {
            adv += elapsed;
            count_time(elapsed, fps);
        }

        /* the bars every frame, the registers and the text fields every other frame */
        draw_bars(paused);
        odd ^= 1;
        if (odd) {
            draw_regs();
        } else {
            draw_position();
            draw_state(paused);
            draw_info(adv);
            adv = 0;
        }

        if (!kbhit())
            continue;
        k = cgetc();
        if (k == 27 || k == 3 || k == 'q' || k == 'Q')
            break;
        if (k == ' ') {
            paused = !paused;
            if (paused)
                rmt_irq_off();      /* silences the PSG, keeps the position */
            else
                rmt_irq_on();
        }
        else if (k == 'r' || k == 'R') {
            rmt_irq_off();
            rmt_init(rmt_song);
            flast = rmt_frames;
            time_frames = time_sec = time_min = 0;
            adv = 0;
            info_reset(fps);
            paused = 0;
            rmt_irq_on();
        }
        else if (k == 's' || k == 'S')
            psg_stereo = (unsigned char)!psg_stereo;
    }

    rmt_irq_off();
    vera_screen_off();
    clrscr();
    cputs("music stopped.\r\n");
    return 0;
}
