/*
 * ili9342 LCD for Raspberry Pi Model B rev2
 */
 
#define BCM2708_PERI_BASE        0x20000000
#define GPIO_BASE                (BCM2708_PERI_BASE + 0x200000) /* GPIO controller */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/backlight.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>


#define BLOCKSIZE (4*1024)

// GPIO setup macros. Always use INP_GPIO(x) before using OUT_GPIO(x) or SET_GPIO_ALT(x,y)
#define INP_GPIO(g) *(gpio+((g)/10)) &= ~(7<<(((g)%10)*3))
#define OUT_GPIO(g) *(gpio+((g)/10)) |=  (1<<(((g)%10)*3))
#define SET_GPIO_ALT(g,a) *(gpio+(((g)/10))) |= (((a)<=3?(a)+4:(a)==4?3:2)<<(((g)%10)*3))

#define GPIO_SET *(gpio+7)  // sets   bits which are 1 ignores bits which are 0
#define GPIO_CLR *(gpio+10) // clears bits which are 1 ignores bits which are 0

#define DATA0 9
#define DATA1 11
#define DATA2 18
#define DATA3 23
#define DATA4 24
#define DATA5 25
#define DATA6 8
#define DATA7 7

#define DC 4
#define CS 27
#define RD 14
#define RW 15
#define IM0 10
#define RESET 17

#define ORIENTATION 0 //0=LANDSCAPE 1=PORTRAIT  

#define DISPLAY_WIDTH   320
#define DISPLAY_HEIGHT  240

#define DISPLAY_BPP     16



volatile unsigned *gpio;


// Set to output
static void gpio_setoutput(char g)
{
		INP_GPIO(g); // must use INP_GPIO before we can use OUT_GPIO 
		OUT_GPIO(g);
}

// Set state 1=high 0=low
static void gpio_setstate(char g,char state)
{
		(state) ? (GPIO_SET = 1<<g) : (GPIO_CLR = 1<<g) ; 
}


// initialization of GPIO
static void tft_init_board(struct fb_info *info)
{

	gpio_setoutput(DATA0);
	gpio_setoutput(DATA1);
	gpio_setoutput(DATA2);
	gpio_setoutput(DATA3);
	gpio_setoutput(DATA4);
	gpio_setoutput(DATA5);
	gpio_setoutput(DATA6);
	gpio_setoutput(DATA7);
	
	gpio_setoutput(DC);
	gpio_setoutput(CS);
	gpio_setoutput(RD);
	gpio_setoutput(RW);
	gpio_setoutput(IM0);
	gpio_setoutput(RESET);
    
    gpio_setstate(DATA0,0);
    gpio_setstate(DATA1,0);
    gpio_setstate(DATA2,0);
    gpio_setstate(DATA3,0);
    gpio_setstate(DATA4,0);
    gpio_setstate(DATA5,0);
    gpio_setstate(DATA6,0);
    gpio_setstate(DATA7,0);
    
    gpio_setstate(DC,1);
    gpio_setstate(CS,0);
    gpio_setstate(RD,1);
    gpio_setstate(RW,1);
    gpio_setstate(IM0,1);
    gpio_setstate(RESET,1);

}

// hard reset of the graphic controller and the tft
static void tft_hard_reset(void)
{
    gpio_setstate(RESET,0);
    msleep(120);
    gpio_setstate(RESET,1);
    msleep(120);
}

static void gpio_set_parallel_data(char data)
{
    gpio_setstate(DATA0,((data >> 0)  & 0x01));
    gpio_setstate(DATA1,((data >> 1)  & 0x01));
    gpio_setstate(DATA2,((data >> 2)  & 0x01));
    gpio_setstate(DATA3,((data >> 3)  & 0x01));
    gpio_setstate(DATA4,((data >> 4)  & 0x01));
    gpio_setstate(DATA5,((data >> 5)  & 0x01));
    gpio_setstate(DATA6,((data >> 6)  & 0x01));
    gpio_setstate(DATA7,((data >> 7)  & 0x01));
}

// write command
static void tft_command_write(char command)
{
    gpio_setstate(DC,0);
    gpio_set_parallel_data(command);
    gpio_setstate(RW,0);
    gpio_setstate(RW,1);
}

// write data
static void tft_data_write(char data)
{
    gpio_setstate(DC,1);
    gpio_set_parallel_data(data);
    gpio_setstate(RW,0);
    gpio_setstate(RW,1);
}

// initialization of ili9341
static void tft_init(struct fb_info *info)
{
	
	tft_hard_reset();
	
	tft_command_write(0x28); //display OFF
	tft_command_write(0x11); //exit SLEEP mode
	tft_data_write(0x00);
	tft_command_write(0xCB); //Power Control A
	tft_data_write(0x39); //always 0x39
	tft_data_write(0x2C); //always 0x2C
	tft_data_write(0x00); //always 0x
	tft_data_write(0x34); //Vcore = 1.6V
	tft_data_write(0x02); //DDVDH = 5.6V
	tft_command_write(0xCF); //Power Control B
	tft_data_write(0x00); //always 0x
	tft_data_write(0x81); //PCEQ off
	tft_data_write(0x30); //ESD protection
	tft_command_write(0xE8); //Driver timing control A
	tft_data_write(0x85); //non‐overlap
	tft_data_write(0x01); //EQ timing
	tft_data_write(0x79); //Pre‐charge timing
	tft_command_write(0xEA); //Driver timing control B
	tft_data_write(0x00); //Gate driver timing
	tft_data_write(0x00); //always 0x
	tft_command_write(0xED); //Power‐On sequence control
	tft_data_write(0x64); //soft start
	tft_data_write(0x03); //power on sequence
	tft_data_write(0x12); //power on sequence
	tft_data_write(0x81); //DDVDH enhance on
	tft_command_write(0xF7); //Pump ratio control
	tft_data_write(0x20); //DDVDH=2xVCI
	tft_command_write(0xC0); //power control 1
	tft_data_write(0x26);
	tft_data_write(0x04); //second parameter for ILI9340 (ignored by ILI9341)
	tft_command_write(0xC1); //power control 2
	tft_data_write(0x11);
	tft_command_write(0xC5); //VCOM control 1
	tft_data_write(0x35);
	tft_data_write(0x3E);
	tft_command_write(0xC7); //VCOM control 2
	tft_data_write(0xBE);
	tft_command_write(0x36); //memory access control = BGR
	tft_data_write(0x88);
	tft_command_write(0xB1); //frame rate control
	tft_data_write(0x00);
	tft_data_write(0x10);
	tft_command_write(0xB6); //display function control
	tft_data_write(0x0A);
	tft_data_write(0xA2);
	tft_command_write(0x3A); //pixel format = 16 bit per pixel
	tft_data_write(0x55);
	tft_command_write(0xF2); //3G Gamma control
	tft_data_write(0x02); //off
	tft_command_write(0x26); //Gamma curve 3
	tft_data_write(0x01);
	tft_command_write(0x2A); //column address set
	tft_data_write(0x00);
	tft_data_write(0x00); //start 0x00
	tft_data_write(0x00);
	tft_data_write(0xEF); //end 0xEF
	tft_command_write(0x2B); //page address set
	tft_data_write(0x00);
	tft_data_write(0x00); //start 0x00
	tft_data_write(0x01);
	tft_data_write(0x3F); //end 0x013F
	
	tft_command_write(0x29); //display ON

}

// write memory to TFT
static void ili9341_update_display_area(const struct fb_image *image)
{
	int x,y;
	
	// set column
	(ORIENTATION) ? tft_command_write(0x2B) : tft_command_write(0x2A);
	
	tft_data_write(image->dx >> 8);
	tft_data_write(image->dx);
	
	tft_data_write((image->dx + image->width) >> 8);
	tft_data_write(image->dx + image->width);
	// set row
	(ORIENTATION) ? tft_command_write(0x2A) : tft_command_write(0x2B);
	
	tft_data_write(image->dy >> 8);
	tft_data_write(image->dy);
	
	tft_data_write((image->dy + image->height) >> 8);
	tft_data_write(image->dy + image->height);
		
	tft_command_write(0x2C); //Memory Write
	
	if(ORIENTATION == 0){
		for(y=0;y < image->width ;y++){
			for(x=0;x < image->height ;x++){
				tft_data_write(image->data[(image->dx * (2 * image->width)) + (image->dy * 2) + 1]);
				tft_data_write(image->data[(image->dx * (2 * image->width)) + (image->dy * 2) + 2]);
			}
		}
	}else{
		for(y=0;y < image->width ;y++){
			for(x=0;x < image->height ;x++){
				tft_data_write(image->data[(image->dx * (2 * image->width)) + (image->dy * 2) + 1]);
				tft_data_write(image->data[(image->dx * (2 * image->width)) + (image->dy * 2) + 2]);
			}
		}
	}

	tft_command_write(0x29); //display ON	
}


static void ili9341_update_display_color_area(const struct fb_fillrect *rect)
{
	int x,y;
	// set column
	(ORIENTATION) ? tft_command_write(0x2B) : tft_command_write(0x2A);
	
	tft_data_write(rect->dx >> 8);
	tft_data_write(rect->dx);
	
	tft_data_write((rect->dx + rect->width) >> 8);
	tft_data_write(rect->dx + rect->width);
	// set row
	
	(ORIENTATION) ? tft_command_write(0x2A) : tft_command_write(0x2B);
	
	tft_data_write(rect->dy >> 8);
	tft_data_write(rect->dy);
	
	tft_data_write((rect->dy + rect->height) >> 8);
	tft_data_write(rect->dy + rect->height);
		
	tft_command_write(0x2C); //Memory Write
	
	if(ORIENTATION == 0){
		for(y=0;y < rect->width ;y++){
			for(x=0;x < rect->height ;x++){
				tft_data_write(rect->color);
				tft_data_write(rect->color >> 8);
			}
		}
	}else{
		for(y=0;y < rect->height ;y++){
			for(x=0;x < rect->width ;x++){
				tft_data_write(rect->color);
				tft_data_write(rect->color >> 8);
			}
		}
	}
	
	tft_command_write(0x29); //display ON	
}

static void ili9341_update_display(const struct fb_info *info)
{
	int x,y;
	
	tft_command_write(0x2C); //Memory Write
	
	if(ORIENTATION == 0){
		for(y=0;y < DISPLAY_WIDTH ;y++){
			for(x=0;x < DISPLAY_HEIGHT ;x++){
				tft_data_write(info->screen_base[(x * (2 * DISPLAY_WIDTH)) + (y * 2) + 1]);
				tft_data_write(info->screen_base[(x * (2 * DISPLAY_WIDTH)) + (y * 2) + 2]);
			}
		}
	}else{
		for(y=(DISPLAY_HEIGHT - 1);y >= 0 ;y--){
			for(x=0;x < DISPLAY_WIDTH ;x++){
				tft_data_write(info->screen_base[(y * (2 * DISPLAY_WIDTH)) + (x * 2) + 1]);
				tft_data_write(info->screen_base[(y * (2 * DISPLAY_WIDTH)) + (x * 2) + 2]);
			}
		}
	}
	tft_command_write(0x29); //display ON
}

static void ili9341_fillrect(struct fb_info *info, const struct fb_fillrect *rect)
{
	//printk(KERN_INFO "fb%d: ili9341_fillrect\n", info->node);
    //ili9341_update_display_color_area(rect);
    ili9341_update_display(info);
}

static void ili9341_copyarea(struct fb_info *info, const struct fb_copyarea *area)
{
	//printk(KERN_INFO "fb%d: ili9341_copyarea\n", info->node);
    ili9341_update_display(info);
}

static void ili9341_imageblit(struct fb_info *info, const struct fb_image *image)
{
   //printk(KERN_INFO "fb%d: ili9341_imageblit\n", info->node);
   //ili9341_update_display_area(image);
   ili9341_update_display(info);
}

static ssize_t ili9341_write(struct fb_info *info, const char __user *buf, size_t count, loff_t *ppos)
{
	unsigned long p = *ppos;
	void *dst;
	int err = 0;
	unsigned long total_size;

	if (info->state != FBINFO_STATE_RUNNING)
		return -EPERM;

	total_size = info->screen_size;

	if (total_size == 0)
		total_size = info->fix.smem_len;

	if (p > total_size)
		return -EFBIG;

	if (count > total_size) {
		err = -EFBIG;
		count = total_size;
	}

	if (count + p > total_size) {
		if (!err)
			err = -ENOSPC;

		count = total_size - p;
	}

	dst = (void __force *) (info->screen_base + p);

	if (info->fbops->fb_sync)
		info->fbops->fb_sync(info);

	if (copy_from_user(dst, buf, count))
		err = -EFAULT;

	if  (!err)
		*ppos += count;

	ili9341_update_display(info);

	return (err) ? err : count;   
}

static ssize_t ili9341_read(struct fb_info *info, const char __user *buf, size_t count, loff_t *ppos)
{
	unsigned long p = *ppos;
	void *dst;
	int err = 0;
	unsigned long total_size;

	if (info->state != FBINFO_STATE_RUNNING)
		return -EPERM;

	total_size = info->screen_size;

	if (total_size == 0)
		total_size = info->fix.smem_len;

	if (p > total_size)
		return -EFBIG;

	if (count > total_size) {
		err = -EFBIG;
		count = total_size;
	}

	if (count + p > total_size) {
		if (!err)
			err = -ENOSPC;

		count = total_size - p;
	}

	dst = (void __force *) (info->screen_base + p);

	if (info->fbops->fb_sync)
		info->fbops->fb_sync(info);

	if (copy_from_user(dst, buf, count))
		err = -EFAULT;

	if  (!err)
		*ppos += count;

	return (err) ? err : count;
}

static void ili9341_deferred_io(struct fb_info *info, struct list_head *pagelist)
{
    ili9341_update_display(info);
}



static struct fb_fix_screeninfo ili9341_fix = {
    .id             = "ili9341",
    .type           = FB_TYPE_PACKED_PIXELS,
    .visual         = FB_VISUAL_TRUECOLOR,
    .accel          = FB_ACCEL_NONE,
    .xpanstep       = 0,
    .ypanstep       = 0,
    .ywrapstep      = 0,
    .line_length    = DISPLAY_WIDTH * DISPLAY_BPP / 8,
};


static struct fb_var_screeninfo ili9341_var = {
    .width          = DISPLAY_WIDTH,
    .height         = DISPLAY_HEIGHT,
    .bits_per_pixel = DISPLAY_BPP,
    .xres           = DISPLAY_WIDTH,
    .yres           = DISPLAY_HEIGHT,
    .xres_virtual   = DISPLAY_WIDTH,
    .yres_virtual   = DISPLAY_HEIGHT,
    .activate       = FB_ACTIVATE_NOW,
    .vmode          = FB_VMODE_NONINTERLACED,

    .nonstd         = 0,
    .red.offset     = 11,
    .red.length     = 5,
    .green.offset   = 5,
    .green.length   = 6,
    .blue.offset    = 0,
    .blue.length    = 5,
    .transp.offset  = 0,
    .transp.length  = 0,
};


static struct fb_ops ili9341_ops = {
    .owner          = THIS_MODULE,
    .fb_read        = ili9341_read,
    .fb_write       = ili9341_write,
    .fb_fillrect    = ili9341_fillrect,
    .fb_copyarea    = ili9341_copyarea,
    .fb_imageblit   = ili9341_imageblit,
};

static struct fb_deferred_io ili9341_defio = {
    .delay          = HZ/25,
    .deferred_io    = ili9341_deferred_io,
};

static unsigned int fps;


static int ili9341_probe(struct platform_device *pdev)
{
    struct fb_info *info;
    int retval = -ENOMEM;
    int vmem_size;
    unsigned char *vmem;


    vmem_size = ili9341_var.width * ili9341_var.height * ili9341_var.bits_per_pixel/8;
    vmem = vzalloc(vmem_size);
    if (!vmem) {
        return -ENOMEM;
    }
    memset(vmem, 0, vmem_size);


    info = framebuffer_alloc(0, &pdev->dev);
    if (!info) {
        vfree(vmem);
        return -ENOMEM;
    }


    info->screen_base = (char __force __iomem*)vmem;
    info->fbops = &ili9341_ops;
    info->fix = ili9341_fix;
    info->fix.smem_start = (unsigned long)vmem;
    info->fix.smem_len = vmem_size;
    info->var = ili9341_var;
    info->flags = FBINFO_DEFAULT | FBINFO_VIRTFB;

    info->fbdefio = &ili9341_defio;
    if (0 < fps) {
        info->fbdefio->delay = HZ/fps;
    }

    fb_deferred_io_init(info);

    retval = register_framebuffer(info);
    if (retval < 0) {
        framebuffer_release(info);
        vfree(vmem);
        return retval;
    }

    platform_set_drvdata(pdev, info);

    gpio = ioremap(GPIO_BASE, BLOCKSIZE);

    tft_init_board(info);
    tft_hard_reset();
    tft_init(info);

    printk(KERN_INFO "fb%d: ili9341 LCD framebuffer device\n", info->node);
    return 0;
}



static int ili9341_remove(struct platform_device *dev)
{
    struct fb_info *info = platform_get_drvdata(dev);

    if (info) {
        unregister_framebuffer(info);
        fb_deferred_io_cleanup(info);
        vfree((void __force *)info->screen_base);



        iounmap(gpio);
        
        
        framebuffer_release(info);
    }
    return 0;
}


static struct platform_driver ili9341_driver = {
    .probe  = ili9341_probe,
    .remove = ili9341_remove,
    .driver = {
        .name   = "ili9341",
    },
};

static struct platform_device *ili9341_device;

static int __init ili9341_init(void)
{
    int ret = platform_driver_register(&ili9341_driver);
    if (0 == ret) {
        ili9341_device = platform_device_alloc("ili9341", 0);
        if (ili9341_device) {
            ret = platform_device_add(ili9341_device);
        } else {
            ret = -ENOMEM;
        }
        if (0 != ret) {
            platform_device_put(ili9341_device);
            platform_driver_unregister(&ili9341_driver);
        }
    }
    return ret;
}

static void __exit ili9341_exit(void)
{
    platform_device_unregister(ili9341_device);
    platform_driver_unregister(&ili9341_driver);
}

module_param(fps, uint, 0);
MODULE_PARM_DESC(fps, "Frames per second (default 25)");

module_init(ili9341_init);
module_exit(ili9341_exit);

MODULE_DESCRIPTION("ili9341 LCD framebuffer driver");
MODULE_AUTHOR("sammyizimmy");
MODULE_LICENSE("GPL");

