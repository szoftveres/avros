###############################################################################
# Makefile for the project os
###############################################################################

## General Flags
PROJECT = os
MCU = atmega1284p
TARGET = os.elf
CC = avr-gcc

CPP = avr-g++

## Options common to compile, link and assembly rules
COMMON = -mmcu=$(MCU)

## Compile options common for all C compilation units.
CFLAGS = $(COMMON)
CFLAGS += -Wall -gdwarf-2 -std=gnu99 -Wextra  -Werror -O0 -fsigned-char -funsigned-bitfields -fpack-struct -fshort-enums
CFLAGS += -MD -MP -MT $(*F).o -MF dep/$(@F).d 

## Assembly specific flags
ASMFLAGS = $(COMMON)
ASMFLAGS += $(CFLAGS)
ASMFLAGS += -x assembler-with-cpp -Wa,-gdwarf2

## Linker flags
LDFLAGS = $(COMMON)
LDFLAGS +=  -Wl,-Map=os.map


## Intel Hex file production flags
HEX_FLASH_FLAGS = -R .eeprom -R .fuse -R .lock -R .signature

HEX_EEPROM_FLAGS = -j .eeprom
HEX_EEPROM_FLAGS += --set-section-flags=.eeprom="alloc,load"
HEX_EEPROM_FLAGS += --change-section-lma .eeprom=0 --no-change-warnings


## Objects that must be built in order to link
OBJECTS = apps.o kernel.o main.o misc.o sh.o sys.o drv.o sema.o pm.o es.o init.o timer.o queue.o vfs.o 

## Objects explicitly added by the user
LINKONLYOBJECTS = 

## Build
all: $(TARGET) os.hex os.eep os.lss size

## Compile
apps.o: ../apps.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

kernel.o: ../kernel.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

main.o: ../main.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

misc.o: ../misc.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

sh.o: ../sh.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

sys.o: ../sys.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

drv.o: ../drv.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

sema.o: ../sema.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

pm.o: ../pm.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

es.o: ../es.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

init.o: ../init.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

timer.o: ../timer.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

queue.o: ../queue.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

vfs.o: ../vfs.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

##Link
$(TARGET): $(OBJECTS)
	 $(CC) $(LDFLAGS) $(OBJECTS) $(LINKONLYOBJECTS) $(LIBDIRS) $(LIBS) -o $(TARGET)

%.hex: $(TARGET)
	avr-objcopy -O ihex $(HEX_FLASH_FLAGS)  $< $@

%.eep: $(TARGET)
	-avr-objcopy $(HEX_EEPROM_FLAGS) -O ihex $< $@ || exit 0

%.lss: $(TARGET)
	avr-objdump -h -S $< > $@

size: ${TARGET}
	@echo
	@avr-size -C --mcu=${MCU} ${TARGET}

## Clean target
.PHONY: clean
clean:
	-rm -rf $(OBJECTS) os.elf dep/* os.hex os.eep os.lss os.map


## Other dependencies
-include $(shell mkdir dep 2>/dev/null) $(wildcard dep/*)

