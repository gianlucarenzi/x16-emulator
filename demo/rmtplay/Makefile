# Makefile - RMTPLAY for the Commander X16: an RMT song on the VERA PSG with
# a visualizer (see README.md)
#
#   make                       build/RMTPLAY.PRG with music/gemx.rmt
#   make SONG=path/song.rmt    any RMT4 / RMT8 module
#   make SONG=path/song.sap    a SAP or XEX exported by RITMO / RMT (LZSS
#                              streams, lzsplayr.s; SUBSONG=n: another subtune)
#   make NAME=.. AUTHOR=.. DATE=..   song info instead of the text in the file
#   make MHZ=4                 the CPU clock the player timer is set for (8)
#   make run                   start it in x16emu (X16EMU=..., X16ROM=...: -rom)
#   make clean
#
# Needs cc65 (cl65) and python3.

CL65    ?= cl65
PYTHON  ?= python3
X16EMU  ?= x16emu
X16ROM  ?=
X16OPTS ?= -scale 2

SONG    ?= music/gemx.rmt
SUBSONG ?= 0
NAME    ?=
AUTHOR  ?=
DATE    ?=
MHZ     ?= 8

B   = build
PRG = $(B)/RMTPLAY.PRG

# a .sap or .xex exported by RITMO / RMT: the VU-Player LZSS streams
ifneq ($(filter .sap .SAP .xex .XEX,$(suffix $(SONG))),)
LZSS    = 1
PLAYER  = src/lzsplayr.s
TRACKS := $(shell $(PYTHON) tools/sap2ca65.py $(SONG) --tracks)
else
PLAYER  = src/rmtplayr.s
TRACKS := $(shell $(PYTHON) -c "d=open('$(SONG)','rb').read(10); print(8 if d[6:10]==b'RMT8' else 4)")
endif

ASM     = $(PLAYER) src/psgrmt.s src/rmtx16.s
DEFS    = --asm-define RMT_SHADOW --asm-define RMT_VERA --asm-define CPU_HZ=$(MHZ)000000 \
          --asm-define RMT_TRACKS=$(TRACKS) -DRMT_TRACKS=$(TRACKS) $(if $(LZSS),-DRMT_LZSS)

all: $(PRG)

# the song and its info are kept in a stamp file: changing them rebuilds
$(B)/song.cfg: FORCE
	@mkdir -p $(B)
	@printf '%s\n' '$(SONG)|$(SUBSONG)|$(TRACKS)|$(NAME)|$(AUTHOR)|$(DATE)|$(MHZ)' | cmp -s - $@ || \
		printf '%s\n' '$(SONG)|$(SUBSONG)|$(TRACKS)|$(NAME)|$(AUTHOR)|$(DATE)|$(MHZ)' > $@

$(B)/song.s: $(SONG) tools/rmt2ca65.py tools/sap2ca65.py $(B)/song.cfg
ifdef LZSS
	$(PYTHON) tools/sap2ca65.py $(SONG) $@ --subsong $(SUBSONG)
else
	$(PYTHON) tools/rmt2ca65.py $(SONG) $@
endif

$(B)/songinfo.h: $(SONG) tools/rmtinfo.py tools/sap2ca65.py $(B)/song.cfg
ifdef LZSS
	$(PYTHON) tools/sap2ca65.py $(SONG) /dev/null --subsong $(SUBSONG) --info $@ \
		--name "$(NAME)" --author "$(AUTHOR)" --date "$(DATE)"
else
	$(PYTHON) tools/rmtinfo.py $(SONG) $@ --name "$(NAME)" --author "$(AUTHOR)" --date "$(DATE)"
endif

$(B)/psgtab.s: tools/mkpsgtab.py
	@mkdir -p $(B)
	$(PYTHON) $< $@

$(PRG): src/rmtplay.c src/rmt.h $(ASM) src/rmt_feat.inc src/vera.inc src/rmtplay.cfg \
        $(B)/psgtab.s $(B)/song.s $(B)/songinfo.h $(B)/song.cfg
	$(CL65) -t cx16 -Osir -C src/rmtplay.cfg -I src -I $(B) --asm-include-dir src $(DEFS) \
		-m $(B)/rmtplay.map -o $@ src/rmtplay.c $(ASM) $(B)/psgtab.s $(B)/song.s

run: $(PRG)
	$(X16EMU) $(if $(X16ROM),-rom $(X16ROM)) $(X16OPTS) -prg $(PRG) -run

clean:
	rm -rf $(B)

.PHONY: all run clean FORCE
