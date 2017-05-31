OBJDIR = .
SRCDIR = .
INCLDIR = -I kernel -I drivers -I servers -I lib -I usr -I usr/lib 
OUTDIR = .

## General Flags
PROGRAM = avros
TARGET = $(PROGRAM).elf
CC = avr-gcc
LD = avr-gcc
MCU = atmega1284p
CFLAGS = -Wall -O0 $(INCLUDE)

## Options common to compile, link and assembly rules
COMMON = -mmcu=$(MCU)

## Compile options common for all C compilation units.
CFLAGS = $(COMMON)
CFLAGS += -Wall -gdwarf-2 -std=gnu99 -Wextra  -Werror -O0 -fsigned-char -funsigned-bitfields -fpack-struct -fshort-enums
# CFLAGS += -MD -MP -MT $(*F).o -MF dep/$(@F).d

## Assembly specific flags
ASMFLAGS = $(COMMON)
ASMFLAGS += $(CFLAGS)
ASMFLAGS += -x assembler-with-cpp -Wa,-gdwarf2

## Linker flags
LDFLAGS = $(COMMON)
LDFLAGS +=  -Wl,-Map=$(PROGRAM).map

## Intel Hex file production flags
HEX_FLASH_FLAGS = -R .eeprom -R .fuse -R .lock -R .signature

HEX_EEPROM_FLAGS = -j .eeprom
HEX_EEPROM_FLAGS += --set-section-flags=.eeprom="alloc,load"
HEX_EEPROM_FLAGS += --change-section-lma .eeprom=0 --no-change-warnings


## Objects that must be built in order to link
OBJECTS = $(OBJDIR)/main.o

LINKONLYOBJECTS = kernel/kernel.o           \
                  kernel/hal.o              \
                  drivers/drv.o             \
                  lib/queue.o               \
                  servers/es.o              \
                  servers/pm.o              \
                  servers/ts.o              \
                  servers/vfs.o             \
                  usr/apps.o                \
                  usr/init.o                \
                  usr/sh.o                  \
                  usr/lib/mstdlib.o         \


SUBDIRS = kernel drivers servers lib usr usr/lib
.PHONY : $(SUBDIRS)

## Build both compiler and program
all: $(TARGET) $(PROGRAM).hex $(PROGRAM).eep $(PROGRAM).lss size

$(SUBDIRS):
	$(MAKE) -C $@

## Compile source files
$(OBJDIR)/%.o : $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c -o $(OBJDIR)/$*.o $< 


##Link
$(TARGET): $(SUBDIRS) $(OBJECTS) 
	$(LD) $(LDFLAGS) $(OBJECTS) $(LINKONLYOBJECTS) -o $(TARGET)

%.hex: $(TARGET)
	avr-objcopy -O ihex $(HEX_FLASH_FLAGS)  $< $@

%.eep: $(TARGET)
	-avr-objcopy $(HEX_EEPROM_FLAGS) -O ihex $< $@ || exit 0

%.lss: $(TARGET)
	avr-objdump -h -S $< > $@

size: ${TARGET}
	@echo
	@avr-size -C --mcu=${MCU} ${TARGET}

## clean
clean:
	-rm -rf $(OBJECTS) $(OUTDIR)/$(TARGET) $(PROGRAM).hex $(PROGRAM).eep $(PROGRAM).lss $(PROGRAM).map
	for dir in $(SUBDIRS); do $(MAKE) clean -C $$dir; done


