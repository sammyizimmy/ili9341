```text
ili9341
========
A Raspberry pi framebuffer driver for the ili9341 lcd driver.
I am using the NHD-2.4-240320SF-CTXI#-T1 (http://www.newhavendisplay.com/nhd24240320sfctxit1-p-3975.html)


NHD-2.4-240320SF-CTXI#-T1 Datasheet (http://www.newhavendisplay.com/specs/NHD-2.4-240320SF-CTXI-T1.pdf)
ili9341 Datasheet (http://www.displayfuture.com/Display/datasheet/controller/ILI9341.pdf)

Installing:
   download the source
   run make
   sudo insmod ili9341.ko
this will create /dev/fb1.
Use the new framebuffer (https://github.com/notro/fbtft/wiki/Framebuffer-use)


The physical part of things.

R-PI Model B v2 GPIO
LCD GPIO p1 p2 GPIO LCD
      3v--* o--5v
      02--o o--5v
      03--o o--GND
DC----04--o o--14---RD
     GND--o o--15---WR
RESET-17--o o--18---DB10
CS----27--o o--GND
      22--o o--23---DB11
      3v--o o--24---DB12
IM0---10--o o--GND
DB8---09--o o--25---DB13
DB9---11--o o--08---DB14
     GND--o o--07---DB15
        p25 p26

Photos:
```
	![Desktop](/photos/X_running_screen.JPG)
	![Connector](/photos/connector.JPG)
	![Full View](/photos/full_view.JPG)
    
