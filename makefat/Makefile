
LD = xc32-gcc
CC = ../origdev/pic32/compiler/pic32-tools/bin/pic32-gcc
CFLAG = -Os -minterlink-compressed -mprocessor=32MX250F128B -Wall -c -g
OBJ = 	NVMem.o	\
	aoutloader.o\
	cmpdata.o\
compiler.o\
debug.o\
editor.o\
error.o\
e.oeption.o\
ff.o\
ff_wrap.o\
filer.o\
flash.o\
float.o\
fu.otion.o\
globalvars.o\
keyinput.o\
library.o\
linker.o\
main.o\
memory.o\
mus.o.o\
operator.o\
run.o\
statement.o\
string.o\
test.o\
value.o\
varname.o\
xprintf.o\

machikaz: $(OBJ)
	$(LD) -o $@ $^  -mips16 -mprocessor=32MX250F128B -Os -T App_32MX170F256B.ld *.a -Wl,--defsym,_min_heap_size=0,--defsym=_min_stack_size=1000,--gc-sections,--report-mem -lm


.c.o:
	$(CC) $(CFLAGS) -c $<

NVMem.o	:NVMem.c
	$(CC) $^ $(CFLAG)

aoutloader.o:aoutloader.c
	$(CC) $^ -mips16 $(CFLAG)

cmpdata.o:cmpdata.c
	$(CC) $^ $(CFLAG)

compiler.o:compiler.c
	$(CC) $^ $(CFLAG)

debug.o:debug.c
	$(CC) $^ $(CFLAG)

editor.o:editor.c
	$(CC) -mips16 $^ $(CFLAG)

error.o:error.c
	$(CC) $^ $(CFLAG)

exception.o:exception.c
	$(CC) $^ $(CFLAG)

ff.o:ff.c
	$(CC) $^ $(CFLAG)

ff_wrap.o:ff_wrap.c
	$(CC) $^ $(CFLAG)

filer.o:filer.c
	$(CC) $^ $(CFLAG)

flash.o:flash.c
	$(CC) $^ $(CFLAG)

float.o:float.c
	$(CC) $^ $(CFLAG)

function.o:function.c
	$(CC) $^ $(CFLAG)

globalvars.o:globalvars.c
	$(CC) $^ $(CFLAG)

keyinput.o:keyinput.c
	$(CC) $^ $(CFLAG)

library.o:library.c
	$(CC) $^ $(CFLAG)

linker.o:linker.c
	$(CC) $^ $(CFLAG)

main.o:main.c
	$(CC) $^ $(CFLAG)

memory.o:memory.c
	$(CC) $^ $(CFLAG)

music.o:music.c
	$(CC) $^ $(CFLAG)

operator.o:operator.c
	$(CC) $^ $(CFLAG)

run.o:run.c
	$(CC) $^ $(CFLAG)

statement.o:statement.c
	$(CC) $^ $(CFLAG)

string.o:string.c
	$(CC) $^ $(CFLAG)

test.o:test.c
	$(CC) $^ $(CFLAG)

value.o:value.c
	$(CC) $^ $(CFLAG)

varname.o:varname.c
	$(CC) $^ $(CFLAG)

xprintf.o:xprintf.c
	$(CC) $^ $(CFLAG)




# hello: hello.o sayhi.c
#     gcc -o hello hello.o sayhi.o
# hello.o: hello.c
#     gcc -c hello.c
# sayhi.o: sayhi.c
#     gcc -c sayhi.c
