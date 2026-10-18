; ============================================================================
; rmtx16.s - the RMT player on the Commander X16: VIA1 timer IRQ + VERA PSG
;
; The Atari RMT routine (rmtplayr.s, or lzsplayr.s for a .sap/.xex song,
; built with -D RMT_SHADOW -D RMT_VERA) writes its POKEY registers to
; pokey_shadow (RAM: the X16 has no POKEY) and leaves the channel state for
; psgrmt.s, which turns it into VERA PSG voices.  This replaces rmtvbi.s of
; the Atari version (VERA_ATARI_PBI vera-tests/rmt).
;
; Tempo: an RMT song is written for the 50 Hz VBI of a PAL Atari or the
; 60 Hz of an NTSC one (rmt_rate).  The X16 frame is the VERA VSYNC, 59.52
; Hz: playing a 50 Hz song from it means skipping one VSYNC in six, and a
; song that changes its registers every frame (instrument envelopes,
; vibrato, songs converted frame by frame) then plays with a 17 ms jolt
; every 6 frames.  So the player runs from Timer 1 of VIA1, free running at
; 300 Hz: a song frame every 6 ticks at 50 Hz, every 5 at 60 Hz, exactly.
; The latch is CPU_HZ / 300 - 2 cycles (-D CPU_HZ=..., 8 MHz by default).
; The KERNAL uses Timer 1 only as an entropy source (it reads the counter),
; the serial bus uses Timer 2.
;
; The handler sits on the KERNAL IRQ vector ($0314).  The KERNAL handler
; does not look at the source (every IRQ is a VSYNC for it: keyboard, mouse,
; clock), so a timer IRQ ends here (ply/plx/pla/rti, as the KERNAL does:
; the RTI goes through __irq_ret, which restores the ROM bank) and only a
; VSYNC goes on to the KERNAL handler.  With instrument speed N the player
; is called N times in a row; psg_update then writes the PSG once.  The PSG
; pitch follows the Atari of the song (rmt_palnts: 1 PAL, 0 NTSC POKEY).
;
; psgrmt.s saves and restores CTRL, FX_CTRL and the port 0 address, so the
; main program can be in the middle of its own VRAM writes.
; ============================================================================

        .setcpu "65C02"

        .export _rmt_init, _rmt_irq_on, _rmt_irq_off
        .export _rmt_frames, _rmt_vframes, _rmt_ispeed, _rmt_rate
        .export _rmt_palnts, rmt_palnts := _rmt_palnts
        .export pokey_shadow, rmt_ioactive
        .import RASTERMUSICTRACKER
        .import psg_update, psg_silence

rmt_init    = RASTERMUSICTRACKER+0      ; X/Y = module, A = song line
rmt_play    = RASTERMUSICTRACKER+3
rmt_silence = RASTERMUSICTRACKER+9

CINV        = $0314                     ; KERNAL IRQ vector
VERA_ISR    = $9F27
VSYNC       = $01
VIA1_T1CL   = $9F04                     ; read: clears the Timer 1 flag
VIA1_T1LL   = $9F06
VIA1_T1LH   = $9F07
VIA1_ACR    = $9F0B
VIA1_IFR    = $9F0D
VIA1_IER    = $9F0E

.ifndef CPU_HZ
CPU_HZ      = 8000000
.endif
TICK_HZ     = 300                       ; a multiple of 50 and of 60
T1_LATCH    = CPU_HZ / TICK_HZ - 2      ; free running period: latch + 2

        .segment "DATA"

_rmt_rate:      .byte 50        ; song frames a second (50 PAL, 60 NTSC)
_rmt_palnts:    .byte 1         ; POKEY clock of the song: 1 PAL, 0 NTSC
rmt_ioactive:   .byte 0         ; (Atari SIO only: never set here)

        .segment "BSS"

pokey_shadow:   .res 16         ; the POKEY registers written by the player
_rmt_frames:    .res 2          ; song frames played
_rmt_vframes:   .res 1          ; VERA frames seen (the main loop waits on it)
_rmt_ispeed:    .res 1          ; player calls per song frame (1..4)
old_irq:        .res 2
on:             .res 1
acc:            .res 2          ; tempo accumulator: += rate every tick
calls:          .res 1

        .segment "CODE"

; ---------------------------------------------------------------------------
; unsigned char __fastcall__ rmt_init(const void *module)
; With the music stopped.  Returns the instrument speed (1..4).
_rmt_init:
        pha
        phx
        jsr _rmt_irq_off
        ply                             ; Y = hi, X = lo for the player
        plx
        lda #0                          ; from song line 0
        jsr rmt_init                    ; A = instrument speed
        cmp #1                          ; clamp to 1..4 (RMT format range)
        bcs @lo
        lda #1
@lo:    cmp #5
        bcc @hi
        lda #4
@hi:    sta _rmt_ispeed
        stz _rmt_frames
        stz _rmt_frames+1
        ldx #0
        rts

; ---------------------------------------------------------------------------
; void rmt_irq_on(void): the music plays from the VIA1 timer
_rmt_irq_on:
        lda on
        bne @done
        php
        sei
        lda CINV
        sta old_irq
        lda CINV+1
        sta old_irq+1
        lda #<irq
        sta CINV
        lda #>irq
        sta CINV+1
        stz acc
        stz acc+1
        lda VIA1_ACR
        ora #$40                        ; Timer 1 free running, no PB7
        and #$7F
        sta VIA1_ACR
        lda #<T1_LATCH
        sta VIA1_T1LL
        lda #>T1_LATCH
        sta VIA1_T1LH                   ; (latches only: the counter reloads
        lda VIA1_T1CL                   ;  at its next time out)
        lda #$C0                        ; Timer 1 IRQ on
        sta VIA1_IER
        lda #1
        sta on
        plp
@done:  rts

; void rmt_irq_off(void): music off, PSG voices silent; the song position is
; kept (rmt_irq_on goes on from there)
_rmt_irq_off:
        lda on
        beq @done
        php
        sei
        lda #$40                        ; Timer 1 IRQ off
        sta VIA1_IER
        lda VIA1_T1CL
        lda old_irq
        sta CINV
        lda old_irq+1
        sta CINV+1
        stz on
        plp
        jsr rmt_silence
        jmp psg_silence
@done:  rts

; ---------------------------------------------------------------------------
; KERNAL IRQ entry: A, X, Y pushed
irq:
        bit VIA1_IFR                    ; bit 6 = Timer 1 -> V
        bvc @kernal
        lda VIA1_T1CL                   ; acknowledge
        clc                             ; acc += rate: a song frame each
        lda acc                         ; time it reaches 300
        adc _rmt_rate
        sta acc
        bcc @c
        inc acc+1
@c:     lda acc+1                       ; acc >= TICK_HZ?
        cmp #>TICK_HZ
        bcc @tick_end
        bne @frame
        lda acc
        cmp #<TICK_HZ
        bcc @tick_end
@frame: sec
        lda acc
        sbc #<TICK_HZ
        sta acc
        lda acc+1
        sbc #>TICK_HZ
        sta acc+1
        lda _rmt_ispeed
        sta calls
@play:  jsr rmt_play                    ; writes pokey_shadow
        dec calls
        bne @play
        inc _rmt_frames
        bne @psg
        inc _rmt_frames+1
@psg:   jsr psg_update                  ; channel state -> VERA PSG voices
@tick_end:
        lda VERA_ISR                    ; a VSYNC too? the KERNAL takes it
        and #VSYNC
        bne @kernal
        ply
        plx
        pla
        rti
@kernal:
        lda VERA_ISR
        and #VSYNC
        beq @k
        inc _rmt_vframes
@k:     jmp (old_irq)                   ; VSYNC acknowledge, keyboard, clock
