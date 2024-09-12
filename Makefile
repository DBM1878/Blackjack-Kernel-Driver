obj-m += blackjack.o

KERNEL_SRC := /lib/modules/5.15.67+/build

all:
	make -C $(KERNEL_SRC) M=$(PWD) modules
clean:
	make -C $(KERNEL_SRC) M=$(PWD) clean
