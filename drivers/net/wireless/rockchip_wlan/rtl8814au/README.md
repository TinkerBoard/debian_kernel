<u>**rtl8814au for linux**</u>

rtl8814au linux kernel driver Wireless Dual-Band USB Adapter
also for rtl8813au devices.

<u>If one USB-ID is missing, please mail me.</u>  

**Building and install driver**

for building type  
`make`  

for load the driver  
`sudo insmod rtl8814au.ko`  

You need to install the needed fw with  
`sudo make installfw`  

**STATUS**  
Currently as educational driver,  
but driver is working fine in STA  

**USB3 Mode Issue**  
Realtek aka the chipdesigner does some **stupid** idea to switch into USB3 mode via special usb read/write command.  
  
On cold boot this device will appear as normal USB2 device.
Switching is tested, but currently not active yet !

**TODO**, will work on if I have enough time,  
currently <u>not</u>

- rip out firmware blob
- activate USB3 switch
- minor crosscompile check


Hans Ulli Kroll <ulli.kroll@googlemail.com>