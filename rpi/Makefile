obj-m += relay_driver.o
obj-m += pir_driver.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	dtc -I dts -O dtb my_devices.dts -o my_devices.dtbo

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
