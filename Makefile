CC = ../machikania/origdev/pic32/compiler/pic32-tools/bin/pic32-gcc
CFLAGS          = -Os -Wall -DMIPS -DNO_ANNOTATIONS \
                  -DNO_PPACK -D_RETROBSD -D__SMALLER_C_SCHAR__ \
                  -D__SMALLER_C__ -D__SMALLER_C_32__ -DSTATIC \
                  -DNO_EXTRA_WARNS -DSYNTAX_STACK_MAX=3200 -mips16 -minterlink-compressed
LD = ../machikania/origdev/pic32/compiler/pic32-tools/bin/pic32-gcc

# For cross compile
#include $(TOPSRC)/cross.mk
#CFLAGS          = -Os -Wall -DMIPS -m32
#LDFLAGS         = -m32

ARCH            = mips

OBJS            = smlrc.o ff.o NVMem.o ff_wrap.o lb.o xprintf.o main.o exception.o ps2keyboard.o videoout.o textvideolib.o as.o ld.o umm_info.o umm_integrity.o umm_malloc.o umm_poison.o aoutloader.o keyinput.o function_table.o cc.o autocomplete.o\
 sd.o\
 flash.o 

CFLAGS 		+= -mprocessor=32MX250F128B  -g 

LDFLAGS 	= -mprocessor=32MX250F128B -g -Wl,--defsym,_min_heap_size=0,--defsym=_min_stack_size=32,--gc-sections,--report-mem,-Map=filename.map

SIZE = xc32-size
BIN2HEX = xc32-bin2hex

all:		smlrc

smlrc:          $(OBJS)
		${LD} ${LDFLAGS} -o $@.elf ${OBJS} ${LIBS} crt0.S crtn.S crti.S -T App_32MX170F256B.ld -nostartfiles -Os
		${SIZE} $@.elf
		${SIZE} *.o
		xc32-objdump smlrc.elf -S -l -s> $@.asm
#		${OBJDUMP} -S $@.elf > $@.dis
		${BIN2HEX} $@.elf
		cp smlrc.hex /media/*/*/

clean:
		rm -f $(OBJS) smlrc smlrc.dis smlrc.elf

###

smlrc.o: smlrc.c fp.c cgmips.c
		${CC} ${CFLAGS} smlrc.c -g -c -o $@ 

# doesn't stable when compile with optimize level `-O1`, `-Os`
as.o:as.c
		${CC} as.c -g -mips16 -c -o $@ 

aoutloader.o:aoutloader.c
		${CC} aoutloader.c -g -c -Os -o $@ 

main.o:main.c
		${CC} -mprocessor=32MX250F128B  -g  main.c -g -c -Os -o $@ 

flash.o: makefat/fatsrc/ flash.c
		./makefat/a.out ./makefat/fatsrc
		${CC} ${CFLAGS} flash.c -c -o $@

sd.o:sd.c
	$(CC) $(CFLAGS) sd.c -c -o $@

exception.o:exception.c
		${CC} -g -mprocessor=32MX250F128B exception.c -minterlink-compressed -O1 -c -o $@
