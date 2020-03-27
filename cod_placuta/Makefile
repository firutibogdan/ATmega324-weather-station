
all: project.hex

project.elf: project.c lcd.c usart.c dht22.c adc.c
	avr-g++ -mmcu=atmega324p -DF_CPU=16000000 -Wall -Os -o $@ $^

project.hex: project.elf
	avr-objcopy -j .text -j .data -O ihex project.elf project.hex
	avr-size project.elf


clean:
	rm -rf project.elf project.hex

.PHONY: all clean
