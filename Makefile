CC=gcc

TARGETS=test \
test.o

all: $(TARGETS)

.PHONY: example spi-lpd8806 clean

example:
	cp -f ./example.c ~/BeagleBoard/kernel/kernel/drivers/char
	export ARCH=arm; \
	export CROSS_COMPILE=arm-linux-gnueabi-; \
	cd ~/BeagleBoard/kernel/kernel; \
	make modules -j9
	scp ~/BeagleBoard/kernel/kernel/drivers/char/example.ko root@192.168.7.2:/lib/modules/3.2.25+/kernel/drivers/char

spi-lpd8806:
	cp -f ./spi-lpd8806.c ~/BeagleBoard/kernel/kernel/drivers/spi
	export ARCH=arm; \
	export CROSS_COMPILE=arm-linux-gnueabi-; \
	cd ~/BeagleBoard/kernel/kernel; \
	make modules -j9
	scp ~/BeagleBoard/kernel/kernel/drivers/spi/spi-lpd8806.ko root@192.168.7.2:/lib/modules/3.2.25+/kernel/drivers/spi

clean: 
	clear
	rm -f $(TARGETS)

test: test.o
	$(CC) $^ -o $@

%.o: %.c
	$(CC) -c $< -o $@


