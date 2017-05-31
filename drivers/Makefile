OBJDIR = .
SRCDIR = .
INCLDIR = .
OUTDIR = .

## General Flags
CC = avr-gcc
LD = avr-gcc
MCU = atmega1284p
CFLAGS = -Wall -O0 -I $(INCLDIR)

## Options common to compile, link and assembly rules
COMMON = -mmcu=$(MCU)

## Compile options common for all C compilation units.
CFLAGS = $(COMMON)
CFLAGS += -Wall -gdwarf-2 -std=gnu99 -Wextra  -Werror -O0 -fsigned-char -funsigned-bitfields -fpack-struct -fshort-enums
# CFLAGS += -MD -MP -MT $(*F).o -MF dep/$(@F).d

## Objects that must be built in order to link
OBJECTS = $(OBJDIR)/drv.o


## Build both compiler and program
all: $(OBJECTS)

## Compile source files
$(OBJDIR)/%.o : $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c -o $(OBJDIR)/$*.o $< 

clean:
	-rm -rf $(OBJECTS)

