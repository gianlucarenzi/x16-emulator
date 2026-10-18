; vera.inc - symbols of the VERA card, shared by every libvera module on
; every host (Atari XL/XE PBI, C64 expansion port, Commander X16).
; All symbol names are padded to 24 characters so the '=' falls at column 25.
;
; Only the base of the 32 registers depends on the host:
;   Atari  $D100 (PBI device space; the registers answer whether or not the
;          $D1FF latch selects the card: the latch only maps its ROM)
;   C64    $DE00 (IO1) or $DF00 (IO2, -D VERA_BASE=$DF00)
;   X16    $9F20
; ca65 -t atari / -t c64 / -t cx16 defines __ATARI__ / __C64__ / __CX16__.

.ifndef VERA_BASE
.ifdef __ATARI__
VERA_BASE               = $D100                     ; PBI (atari800: -verax16)
.elseif .defined(__CX16__)
VERA_BASE               = $9F20                     ; on the board
.else
VERA_BASE               = $DE00                     ; IO1 (VICE: -vera -verabase 0xde00)
.endif
.endif

; ============================================================================
; VERA registers (at VERA_BASE + offset)
; ============================================================================

VERA_ADDR_L             = VERA_BASE+$00              ; VRAM address bits  7:0 (active port)
VERA_ADDR_M             = VERA_BASE+$01              ; VRAM address bits 15:8
VERA_ADDR_H             = VERA_BASE+$02              ; bit[0]=A16, bits[7:4]=increment
VERA_DATA0              = VERA_BASE+$03              ; VRAM data port 0
VERA_DATA1              = VERA_BASE+$04              ; VRAM data port 1
VERA_CTRL_REG           = VERA_BASE+$05              ; ADDRSEL(0) DCSEL(1) RESET(7)
VERA_CTRL               = VERA_CTRL_REG             ; alias
VERA_IEN                = VERA_BASE+$06              ; interrupt enable register
VERA_ISR                = VERA_BASE+$07              ; interrupt status (write 1 to clear)

; ============================================================================
; VERA display composer — DCSEL=0 (output mode, scaling, border)
; ============================================================================

VERA_DC_VIDEO           = VERA_BASE+$09              ; output mode, layer/sprite enable
VERA_DC_HSCALE          = VERA_BASE+$0A              ; horizontal scale (128 = 1:1)
VERA_DC_VSCALE          = VERA_BASE+$0B              ; vertical scale   (128 = 1:1)
VERA_DC_BORDER          = VERA_BASE+$0C              ; border colour index

; ============================================================================
; VERA display composer — DCSEL=1 (active area clipping)
; ============================================================================

VERA_DC_HSTART          = VERA_BASE+$09              ; active area start column (/4)
VERA_DC_HSTOP           = VERA_BASE+$0A              ; active area stop  column (/4)
VERA_DC_VSTART          = VERA_BASE+$0B              ; active area start row    (/2)
VERA_DC_VSTOP           = VERA_BASE+$0C              ; active area stop  row    (/2)

; ============================================================================
; VERA layer 0 registers
; ============================================================================

VERA_L0_CONFIG          = VERA_BASE+$0D              ; map mode and colour depth
VERA_L0_MAPBASE         = VERA_BASE+$0E              ; tilemap base (VRAM addr / 512) — unused in bitmap mode
VERA_L0_TILEBASE        = VERA_BASE+$0F               ; bitmap base (VRAM addr/2048, bits7:2) + bitmap width (bit0)
VERA_L0_HSCR_L          = VERA_BASE+$10              ; horizontal scroll low  byte
VERA_L0_HSCR_H          = VERA_BASE+$11              ; horizontal scroll high byte
VERA_L0_VSCR_L          = VERA_BASE+$12              ; vertical scroll low  byte
VERA_L0_VSCR_H          = VERA_BASE+$13              ; vertical scroll high byte

; ============================================================================
; VERA layer 1 registers
; ============================================================================

VERA_L1_CONFIG          = VERA_BASE+$14              ; map mode and colour depth
VERA_L1_MAPBASE         = VERA_BASE+$15              ; tilemap base (VRAM addr / 512)
VERA_L1_TILEBASE        = VERA_BASE+$16              ; glyph base   (VRAM addr / 512)
VERA_L1_HSCR_L          = VERA_BASE+$17              ; horizontal scroll low  byte
VERA_L1_HSCR_H          = VERA_BASE+$18              ; horizontal scroll high byte
VERA_L1_VSCR_L          = VERA_BASE+$19              ; vertical scroll low  byte
VERA_L1_VSCR_H          = VERA_BASE+$1A              ; vertical scroll high byte

; ============================================================================
; VERA control constants and bitmasks
; ============================================================================

VERA_REG_ARRAY          = VERA_BASE+$00              ; alias for register file base
VERA_ADDRSEL_CLEAR      = $FE                       ; AND mask: clear ADDRSEL bit in CTRL

VERA_INC0               = $00                       ; ADDR_H: no auto-increment
VERA_INC1               = $10                       ; ADDR_H: auto-increment by 1
VERA_INC2               = $20                       ; ADDR_H: auto-increment by 2
VERA_INC4               = $30                       ; ADDR_H: auto-increment by 4
VERA_INCN1              = $18                       ; ADDR_H: auto-decrement by 1 (step -1)

VERA_DCSEL0             = $00                       ; CTRL: select display composer bank 0
VERA_DCSEL1             = $02                       ; CTRL: select display composer bank 1
VERA_DCSEL2             = $04                       ; CTRL: select FX bank 2 (FX_CTRL … FX_MULT)
VERA_DCSEL3             = $06                       ; CTRL: select FX bank 3 (FX_X/Y_INCR)
VERA_DCSEL4             = $08                       ; CTRL: select FX bank 4 (FX_X/Y_POS integer)
VERA_DCSEL5             = $0A                       ; CTRL: select FX bank 5 (FX_X/Y_POS_S, poly fill)
VERA_DCSEL6             = $0C                       ; CTRL: select FX bank 6 (FX_CACHE)

VERA_VIDEO_VGA          = $01                       ; DC_VIDEO: VGA output mode (640×480)
VERA_LAYER0_EN          = $10                       ; DC_VIDEO: enable layer 0
VERA_LAYER1_EN          = $20                       ; DC_VIDEO: enable layer 1
VERA_MAP_128x64         = $60                       ; L1_CONFIG: 128-wide × 64-tall tilemap

VERA_L0_BITMAP_8BPP     = $07                       ; L0_CONFIG: Bitmap Mode(bit2)=1 | Color Depth(bits1:0)=3 (8bpp)
VERA_L0_TILEBASE_320    = $00                       ; L0_TILEBASE: base=$0:0000, bit0=0 -> 320px-wide bitmap
VERA_SCALE_320X240      = $40                       ; DC_HSCALE/DC_VSCALE: 2:1 (320x240 source -> 640x480 VGA out),
                                                     ; same convention already used for this project's 40x30 text mode

; ============================================================================
; VERA audio — PSG voice registers live in VRAM at $1F9C0-$1F9FF (16 voices,
; 4 bytes each): +0/+1 freq lo/hi, +2 pan+volume, +3 waveform+pulse width.
; ============================================================================

VERA_PSG_BASE           = $01F9C0                   ; PSG voice 0 register base in VRAM
VERA_PSG_VOICE_SIZE     = 4                         ; bytes per PSG voice

; ============================================================================
; FX coprocessor — register addresses at VERA_BASE+$09-$0C, muxed by CTRL[DCSEL]
; Usage pattern: sta VERA_CTRL_REG with the DCSEL value, then access the
; physical address.  All four addresses are shared across DCSEL banks 2-6.
; ============================================================================

; DCSEL=2 — control and geometry setup
VERA_FX_CTRL            = VERA_BASE+$09              ; FX_CTRL:     Addr1 mode, cache flags, 4-bit, transparency
VERA_FX_TILEBASE        = VERA_BASE+$0A              ; FX_TILEBASE: tile-data base (addr>>9)[5:0], clip, 2-bit poly
VERA_FX_MAPBASE         = VERA_BASE+$0B              ; FX_MAPBASE:  tilemap base  (addr>>9)[5:0], map size[1:0]
VERA_FX_MULT            = VERA_BASE+$0C              ; FX_MULT:     multiplier / accumulator control

; DCSEL=3 — X and Y increments (6.9 fixed point, signed 15-bit + 32× flag)
VERA_FX_X_INCR_L        = VERA_BASE+$09              ; FX_X_INCR_L: X_Incr[-2:-9]
VERA_FX_X_INCR_H        = VERA_BASE+$0A              ; FX_X_INCR_H: [7]=32× flag, [6:0]=X_Incr[5:-1]
VERA_FX_Y_INCR_L        = VERA_BASE+$0B              ; FX_Y_INCR_L: Y_Incr[-2:-9]
VERA_FX_Y_INCR_H        = VERA_BASE+$0C              ; FX_Y_INCR_H: [7]=32× flag, [6:0]=Y_Incr[5:-1]

; DCSEL=4 — X and Y integer position (11.9 fixed point, integer part)
VERA_FX_X_POS_L         = VERA_BASE+$09              ; FX_X_POS_L: X_Pos[7:0]
VERA_FX_X_POS_H         = VERA_BASE+$0A              ; FX_X_POS_H: [7]=X_Pos[-9], [2:0]=X_Pos[10:8]
VERA_FX_Y_POS_L         = VERA_BASE+$0B              ; FX_Y_POS_L: Y_Pos[7:0]
VERA_FX_Y_POS_H         = VERA_BASE+$0C              ; FX_Y_POS_H: [7]=Y_Pos[-9], [2:0]=Y_Pos[10:8]

; DCSEL=5 — subpixel fraction and polygon fill length
VERA_FX_X_POS_S         = VERA_BASE+$09              ; FX_X_POS_S:     X_Pos[-1:-8] (subpixel fraction)
VERA_FX_Y_POS_S         = VERA_BASE+$0A              ; FX_Y_POS_S:     Y_Pos[-1:-8] (subpixel fraction)
VERA_FX_POLY_FILL_L     = VERA_BASE+$0B              ; FX_POLY_FILL_L: fill span low  (read-only)
VERA_FX_POLY_FILL_H     = VERA_BASE+$0C              ; FX_POLY_FILL_H: fill span high (read-only)

; DCSEL=6 — 32-bit write cache (and accumulator side-effects on read)
; Reading VERA_BASE+$09 (FX_ACCUM_RESET) resets fx_mult_accumulator, returns cache[7:0]
; Reading VERA_BASE+$0A (FX_ACCUM)       triggers accumulate step,    returns cache[15:8]
VERA_FX_CACHE_L         = VERA_BASE+$09              ; FX_CACHE_L / FX_ACCUM_RESET (DCSEL=6)
VERA_FX_CACHE_M         = VERA_BASE+$0A              ; FX_CACHE_M / FX_ACCUM       (DCSEL=6)
VERA_FX_CACHE_H         = VERA_BASE+$0B              ; FX_CACHE_H / mult_b[7:0]    (DCSEL=6)
VERA_FX_CACHE_U         = VERA_BASE+$0C              ; FX_CACHE_U / mult_b[15:8]   (DCSEL=6)

; ============================================================================
; FX_CTRL (DCSEL=2, VERA_BASE+$09) bit masks and Addr1 mode constants
; ============================================================================

VERA_FX_TRANSP          = $80                       ; bit 7: transparency — skip zero bytes/nibbles on write
VERA_FX_CACHE_WR_EN     = $40                       ; bit 6: write 32-bit cache to VRAM on each DATA write
VERA_FX_CACHE_FILL_EN   = $20                       ; bit 5: fill cache from DATA0/DATA1 reads
VERA_FX_ONE_BYTE_CACHE  = $10                       ; bit 4: one-byte cache cycling (vs full 4-byte)
VERA_FX_16BIT_HOP       = $08                       ; bit 3: 16-bit hop mode on address port 1
VERA_FX_4BIT_MODE       = $04                       ; bit 2: 4-bit (nibble) pixel mode
VERA_FX_ADDR1_NORMAL    = $00                       ; bits[1:0]=00: Addr1 normal (no FX helper)
VERA_FX_ADDR1_LINE      = $01                       ; bits[1:0]=01: Addr1 line-draw helper
VERA_FX_ADDR1_POLY      = $02                       ; bits[1:0]=10: Addr1 polygon-fill helper
VERA_FX_ADDR1_AFFINE    = $03                       ; bits[1:0]=11: Addr1 affine-transform helper

; Compat aliases for the driver
FX_CACHE_WR_EN          = VERA_FX_CACHE_WR_EN
FX_CACHE_FILL_EN        = VERA_FX_CACHE_FILL_EN

; ============================================================================
; FX_MULT (DCSEL=2, VERA_BASE+$0C) bit masks
; ============================================================================

VERA_FX_MULT_RESET_ACCUM = $80                      ; bit 7: reset accumulator to 0 (write-only trigger)
VERA_FX_MULT_ACCUM      = $40                       ; bit 6: trigger one accumulate step on write
VERA_FX_MULT_SUB_EN     = $20                       ; bit 5: subtract (vs add) in accumulate
VERA_FX_MULT_EN         = $10                       ; bit 4: use multiplier result for VRAM write

; ============================================================================
; VRAM layout — 128 KB address space; text display lives in bank 1
; ============================================================================

SCREEN_ADDR             = $01B000                   ; tilemap start in VRAM
CHARSET_ADDR            = $01F000                   ; character glyph data in VRAM

SCREEN_MAPBASE          = <(SCREEN_ADDR>>9)         ; L1_MAPBASE  register value (addr/512)
SCREEN_TILEBASE         = <(CHARSET_ADDR>>9)        ; L1_TILEBASE register value (addr/512)

SCREEN_ADDR_M           = >SCREEN_ADDR              ; tilemap VRAM address mid  byte
SCREEN_ADDR_BANK        = ^SCREEN_ADDR              ; tilemap VRAM address bank byte
VERA_SCREEN_BASE_M      = SCREEN_ADDR_M             ; alias used in vera_driver.s
VERA_ADDR_H_BASE        = (VERA_INC1|^SCREEN_ADDR)  ; ADDR_H preset for tilemap port
VERA_ADDR_H_BASE_N1     = (VERA_INCN1|^SCREEN_ADDR) ; ADDR_H preset with step -1

CHARSET_VRAM_L          = <CHARSET_ADDR             ; glyph VRAM address byte 0
CHARSET_VRAM_M          = >CHARSET_ADDR             ; glyph VRAM address byte 1
CHARSET_VRAM_H          = (VERA_INC1|^CHARSET_ADDR) ; glyph ADDR_H with increment

; ============================================================================
; Screen / viewport configuration — select at build time:
;   make SCREEN=80x60   →  80×60 viewport (8-pixel-tall tiles, 640×480 VGA)
;   make SCREEN=80x30   →  80×30 viewport (16-pixel-tall tiles, default)
;   make SCREEN=40x30   →  40×30 viewport (8-pixel tiles, 2× scale, 320×240)
;
; Passing MODE_40X30=1 activates the 320×240 path with 2x hardware scaling.
; Passing FONT_8X8=1 selects the 8-pixel tall character set (font8x8.bin).
; ============================================================================

.ifdef MODE_40X30
TILE_HEIGHT             = 8                         ; tile height in pixels
FONT_PAGES              = 4                         ; 128 chars ×  8 bytes = 1024 = 4 × 256
SCREEN_COLS             = 40                        ; visible columns (320 ÷ 8)
SCREEN_ROWS             = 30                        ; visible rows    (240 ÷ 8)
SCREEN_TILEBASE_REG     = <(CHARSET_ADDR>>9)        ; L1_TILEBASE: bit1=0 → 8px tile height
HSCALE_VAL              = $40                       ; horizontal scale (64 = 2:1 magnification)
VSCALE_VAL              = $40                       ; vertical scale   (64 = 2:1 magnification)
INPUT_LINE_ROWS         = 3                         ; max logical line length (3 physical rows)
.else
SCREEN_COLS             = 80                        ; visible columns (640 ÷ 8)
HSCALE_VAL              = $80                       ; horizontal scale (128 = 1:1)
VSCALE_VAL              = $80                       ; vertical scale   (128 = 1:1)
INPUT_LINE_ROWS         = 2                         ; max logical line length (2 physical rows)
.ifdef FONT_8X8
TILE_HEIGHT             = 8                         ; tile height in pixels
FONT_PAGES              = 4                         ; 128 chars ×  8 bytes = 1024 = 4 × 256
SCREEN_ROWS             = 60                        ; visible rows  (480 ÷  8)
SCREEN_TILEBASE_REG     = <(CHARSET_ADDR>>9)        ; L1_TILEBASE: bit1=0 → 8px tile height
.else
TILE_HEIGHT             = 16                        ; tile height in pixels
FONT_PAGES              = 8                         ; 128 chars × 16 bytes = 2048 = 8 × 256
SCREEN_ROWS             = 30                        ; visible rows  (480 ÷ 16)
SCREEN_TILEBASE_REG     = <(CHARSET_ADDR>>9) | 2    ; L1_TILEBASE: bit1=1 → 16px tile height
.endif
.endif

; ============================================================================
; Viewport and tilemap dimensions
; ============================================================================

MAP_COLS                = 128                                  ; tilemap width  in cells (256 bytes/row stride)
MAP_ROWS                = 64                                   ; tilemap height in cells
SCREEN_COLS_VIEW        = SCREEN_COLS                          ; current viewport width in characters
SCREEN_ROWS_VIEW        = SCREEN_ROWS                          ; current viewport height in characters
TEXT_COLOR              = $61                                  ; default text colour byte (fg/bg nibbles)
INPUT_LINE_MAX          = SCREEN_COLS_VIEW * INPUT_LINE_ROWS   ; max chars in one logical input line
BELL_COL                = 5                                    ; trigger bell click 5 chars before EOL of last row

; ============================================================================
; Registers past the layers
; ============================================================================

VERA_IRQLINE_L          = VERA_BASE+$08             ; LINE IRQ raster line bits 7:0 (write)
VERA_SCANLINE_L         = VERA_BASE+$08             ; current raster line bits 7:0 (read)
VERA_AUDIO_CTRL         = VERA_BASE+$1B             ; PCM: FIFO reset(7), 16 bit(5), stereo(4), volume(3:0)
VERA_AUDIO_RATE         = VERA_BASE+$1C             ; PCM sample rate (128 = 48828 Hz)
VERA_AUDIO_DATA         = VERA_BASE+$1D             ; PCM FIFO (write)
VERA_SPI_DATA           = VERA_BASE+$1E             ; SPI data
VERA_SPI_CTRL           = VERA_BASE+$1F             ; SPI busy(7), autotx(2), slow(1), select(0)

VERA_IRQ_VSYNC          = $01                       ; IEN/ISR: vertical sync
VERA_IRQ_LINE           = $02                       ; IEN/ISR: raster line
VERA_IRQ_SPRCOL         = $04                       ; IEN/ISR: sprite collision
VERA_IRQ_AFLOW          = $08                       ; IEN/ISR: PCM FIFO below 1/4 (level, not acknowledgeable)

VERA_DCSEL63            = $7E                       ; CTRL: DCSEL 63, the identity bytes at $09-$0C
VERA_ID_V               = 'V'                       ; DCSEL 63, $09: 'V'
VERA_ID_MAJOR_MIN       = 47                        ; DCSEL 63, $0A: major version of a VERA with FX
