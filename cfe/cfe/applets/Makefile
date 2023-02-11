
CFG_MLONG64=0
MIPS_BARE_APP=1
TOOLS=../../../broadcom_2004d_341/i686-pc-linux-gnu/bin/
CFE_DIRS =  /home/javad/workspace/dsl_mips/cfe-bcm63xx/cfe/cfe/lib/ \
 ../include /home/javad/workspace/dsl_mips/cfe-bcm63xx/broadcom_2004d_341/i686-pc-linux-gnu/sb1-elf/include/sb1250-include/ \
 ../arch/mips/cpu/sb1250/include ../lib ../verif ../api \
 ../../../shared/opensource/include/bcm963xx/ \
 ../../cfe/arch/mips/board/bcm63xx_ram/include/ \
 ../../cfe/arch/mips/cpu/bcmcore/include \
 ../../cfe/arch/mips/common/include \
 ../../cfe/main/

CFE_INCLUDES =  $(patsubst %,-I%,$(subst :, ,$(CFE_DIRS)))

VPATH = $(CFE_DIRS)

INCLUDES =  -I. $(CFE_INCLUDES)
COMFLAGS = -g -c  -ffreestanding
CFLAGS = $(INCLUDES) $(COMFLAGS) -Os -static -Wall -Werror -Wstrict-prototypes -Wmissing-prototypes -G 0 -DCFE_API_ALL -mips32 -D_MIPSREGS32_ -mabi=32 -D_BCM96338_

ifeq ($(MIPS_BARE_APP),1)
CFLAGS += -DMIPS_BARE_APP=1
endif
GCC = $(TOOLS)sb1-elf-gcc
GLD = $(TOOLS)sb1-elf-ld
OBJCOPY = $(TOOLS)sb1-elf-objcopy

# ifeq (${CFG_MLONG64},1)
# CFLAGS += -mlong64 -D__long64
# endif

LDFLAGS =  --script cfe_applet.lds -lcfe

ifeq (CFG_LITTLE,1)
CFLAGS += -EL
LDFLAGS += -EL
else
CFLAGS += -EB
LDFLAGS += -EB
endif


%.o : %.c
	$(GCC) $(CFLAGS) -o $@ $<

%.o : %.S
	$(GCC) $(CFLAGS) -o $@ $<


LIBOBJS = cfe_api.o \
			lib_string.o \
			lib_printf.o \
			# cfe_console.o \
			# cfe_background.o \
			# env_subr.o \
			# nvram_subr.o \
			# lib_malloc.o \
			# lib_queue.o \
			# lib_string2.o \

all : test vapitest
	@ echo done

test : test.o minicrt0.o $(LIBOBJS) 
	$(GLD) -o test.elfraw -Map test.mapraw $(LDFLAGS) minicrt0.o test.o $(LIBOBJS) 
	$(GLD) -o test.elf -Ttext 0x80001000 -Map test.map $(LDFLAGS) minicrt0.o test.o $(LIBOBJS) 
	$(OBJCOPY) -O binary test.elfraw test
	$(OBJCOPY) -O srec test.elfraw test.srec

download : download.o minicrt0.o $(LIBOBJS) 
	$(GLD) -o download.elf  -Map download.map $(LDFLAGS) minicrt0.o download.o $(LIBOBJS) 
	$(OBJCOPY) --strip-unneeded download.elf s.download.elf
	gzip --best -c s.download.elf > download.elf.gz
	$(GLD) -Tdownload.lds -bbinary -o download.data download.elf.gz

vapitest : vapitest.o 
	$(GLD) -o vapitest -Ttext 0x80020000 -Map vapitest.map vapitest.o
	$(OBJCOPY) -O srec vapitest vapitest.srec

clean :
	rm -f *.o *~ *.map *.mapraw
	rm -f test test.elf test.srec test.elfraw
	rm -f *download.elf *.gz
	rm -f vapitest vapitest.srec
