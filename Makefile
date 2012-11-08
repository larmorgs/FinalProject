CC=gcc

TARGETS=test \
test.o

all: $(TARGETS)

example:
	cp -f ./example.c ~/BeagleBoard/kernel/kernel/drivers/char
	export ARCH=arm
	export CROSS_COMPILE=arm-linux-gnueabi-
	cd ~/BeagleBoard/kernel/kernel; make modules -j9
	scp ~/BeagleBoard/kernel/kernel/drivers/char/example.ko root@192.168.7.2:/lib/modules/3.2.25+/kernel/drivers/char

clean: 
	clear
	rm -f $(TARGETS)

test: test.o
	$(CC) $^ -o $@

%.o: %.c
	$(CC) -c $< -o $@


