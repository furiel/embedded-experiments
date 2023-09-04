CC:=avr-gcc
OBJCOPY:=avr-objcopy
AVRDUDE:=avrdude
MCU=atmega32u4
PROGRAMMER=avr109
USBDEV:=/dev/ttyACM0

all: blink.hex

flash-%: % reset
	$(AVRDUDE) -p $(MCU) -c $(PROGRAMMER) -U flash:w:$<:i -D -b57600 -P $(USBDEV) -v

%.bin: %.c
	$(CC) -Wall -g -Os -mmcu=$(MCU) -o $@ $<

%.hex: %.bin
	$(OBJCOPY) -j .text -j .data -O ihex $< $@

.PHONY = reset
