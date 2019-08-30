KERN_DIR = /root/linux-xlnx
.PHONY:all
all:
	make -C $(KERN_DIR) M=`pwd` modules

.PHONY:clean
clean:
	make -C $(KERN_DIR) M=`pwd` modules clean

obj-m += ssd1306s.o
