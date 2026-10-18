#ifndef __RMT_H__
#define __RMT_H__

/* rmt.h - RMT music on the VERA PSG of the Commander X16 (rmtx16.s,
 * rmtplayr.s or lzsplayr.s, psgrmt.s) */

/* RMT module linked in the program (tools/rmt2ca65.py, tools/sap2ca65.py) */
extern const unsigned char rmt_song[];

/* Init the player on a module (music stopped); returns the instrument
 * speed (player calls per song frame, 1..4) */
unsigned char __fastcall__ rmt_init(const void *module);

/* Music on / off on the VIA1 timer IRQ (off: PSG silent, the position kept) */
void rmt_irq_on(void);
void rmt_irq_off(void);

extern volatile unsigned int  rmt_frames;     /* song frames played */
extern volatile unsigned char rmt_vframes;    /* VERA frames (VSYNC count) */
extern unsigned char          rmt_ispeed;     /* player calls per song frame */
/* song frames a second (50: PAL Atari song, the default; 60: NTSC) and the
 * POKEY clock of its pitch (1 PAL, 0 NTSC): set them before rmt_init */
extern unsigned char          rmt_rate;
extern unsigned char          rmt_palnts;

/* Tracks of the song: 4 (RMT4, mono SAP) or 8 (RMT8, stereo SAP); the
 * Makefile builds with -DRMT_TRACKS from the song file */
#ifndef RMT_TRACKS
#define RMT_TRACKS 4
#endif

/* VERA PSG output (psgrmt.s) */
void psg_init(void);
extern volatile unsigned char psg_enable;     /* 1 = voices follow the player */
extern volatile unsigned char psg_stereo;     /* 1 = stereo pan */
extern volatile unsigned char psg_hybrid;     /* (Atari: channels 1-4 on POKEY) */

#endif
