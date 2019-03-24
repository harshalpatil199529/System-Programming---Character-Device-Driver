Assignment 5

Steps to test the code :

1) Compile driver module : $ make

2) Load module : $ sudo insmod char_driver.ko NUM_DEVICES=<num_devices>

3) Test driver :
	1) Compile userapp : $ make app
	2) Run userapp : $ sudo ./userapp <device_number>			 
		   
4) Unload module : $ sudo rmmod char_driver
