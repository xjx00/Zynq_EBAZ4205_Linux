#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/capability.h>
#include <linux/jiffies.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
 
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include "SSD1306.h"

#define DEVICE_NAME "ssd1306"

static struct i2c_client *ssd1306_client;
static struct class *drv_class;
static struct cdev *chr_dev;
static struct device *device;
static dev_t devnum;

/**************************************************************************/
void SSD1306_init(void)
{
	m_font_offset = 2;
	SSD1306_sendCommand(0xAE);            //display off
	SSD1306_sendCommand(0xA6);            //Set Normal Display (default)
	SSD1306_sendCommand(0xAE);            //DISPLAYOFF
	SSD1306_sendCommand(0xD5);            //SETDISPLAYCLOCKDIV
	SSD1306_sendCommand(0x80);            // the suggested ratio 0x80
	SSD1306_sendCommand(0xA8);            //SSD1306_SETMULTIPLEX
	SSD1306_sendCommand(0x3F);
	SSD1306_sendCommand(0xD3);            //SETDISPLAYOFFSET
	SSD1306_sendCommand(0x0);             //no offset
	SSD1306_sendCommand(0x40|0x0);        //SETSTARTLINE
	SSD1306_sendCommand(0x8D);            //CHARGEPUMP
	SSD1306_sendCommand(0x14);
	SSD1306_sendCommand(0x20);            //MEMORYMODE
	SSD1306_sendCommand(0x00);            //0x0 act like ks0108
	SSD1306_sendCommand(0xA1);            //SEGREMAP   Mirror screen horizontally (A0)
	SSD1306_sendCommand(0xC8);            //COMSCANDEC Rotate screen vertically (C0)
	SSD1306_sendCommand(0xDA);            //0xDA
	SSD1306_sendCommand(0x12);            //COMSCANDEC
	SSD1306_sendCommand(0x81);            //SETCONTRAST
	SSD1306_sendCommand(0xCF);            //
	SSD1306_sendCommand(0xd9);            //SETPRECHARGE 
	SSD1306_sendCommand(0xF1); 
	SSD1306_sendCommand(0xDB);            //SETVCOMDETECT                
	SSD1306_sendCommand(0x40);
	SSD1306_sendCommand(0xA4);            //DISPLAYALLON_RESUME        
	SSD1306_sendCommand(0xA6);            //NORMALDISPLAY             
	SSD1306_clearDisplay();
	SSD1306_sendCommand(0x2E);            //Stop scroll
	SSD1306_sendCommand(0x20);            //Set Memory Addressing Mode
	SSD1306_sendCommand(0x00);            //Set Memory Addressing Mode ab Horizontal addressing mode
	SSD1306_setFont(font8x8);
}

void SSD1306_setFont(const uint8_t* font)
{
  m_font = font;
  m_font_width = pgm_read_byte(&m_font[0]);
}

void SSD1306_sendCommand(unsigned char command)
{
    uint8_t val[2];
    val[0] = SSD1306_Command_Mode;
    val[1] = command;

	struct i2c_msg msg = { 
            .addr = ssd1306_client->addr,
            .flags = 0,            
            .len = 2,
            .buf = val,
    };  

    
	if (i2c_transfer(ssd1306_client->adapter,&msg, 1) != 1) {
        printk(KERN_ERR"i2c_transfer fail\n");
        return  -EIO;
    } 
}

void SSD1306_setBrightness(unsigned char Brightness)
{
   SSD1306_sendCommand(SSD1306_Set_Brightness_Cmd);
   SSD1306_sendCommand(Brightness);
}

void SSD1306_setHorizontalMode()
{
    addressingMode = HORIZONTAL_MODE;
    SSD1306_sendCommand(0x20);                      //set addressing mode
    SSD1306_sendCommand(0x00);                      //set horizontal addressing mode
}

void SSD1306_setPageMode()
{
    addressingMode = PAGE_MODE;
    SSD1306_sendCommand(0x20);                      //set addressing mode
    SSD1306_sendCommand(0x02);                      //set page addressing mode
}

void SSD1306_setTextXY(unsigned char row, unsigned char col)
{
    SSD1306_sendCommand(0xB0 + row);                          //set page address
    SSD1306_sendCommand(0x00 + (m_font_width*col & 0x0F));    //set column lower addr
    SSD1306_sendCommand(0x10 + ((m_font_width*col>>4)&0x0F)); //set column higher addr
}

void SSD1306_clearDisplay()
{
  unsigned char i,j;
  SSD1306_sendCommand(SSD1306_Display_Off_Cmd);     //display off
  for(j=0;j<8;j++)
  {    
    SSD1306_setTextXY(j,0);    
    {
      for(i=0;i<16;i++)  //clear all columns
      {
        SSD1306_putChar(' ');    
      }
    }
  }
  SSD1306_sendCommand(SSD1306_Display_On_Cmd);     //display on
  SSD1306_setTextXY(0,0);    
}

void SSD1306_sendData(unsigned char Data)
{
  uint8_t val[2];
  val[0] = SSD1306_Data_Mode;
  val[1] = Data;

  struct i2c_msg msg = { 
            .addr = ssd1306_client->addr,
            .flags = 0,            
            .len = 2,
            .buf = val,
    };  
   
	if (i2c_transfer(ssd1306_client->adapter,&msg, 1) != 1) {
        printk(KERN_ERR"i2c_transfer fail\n");
        return  -EIO;
    } 
}

int SSD1306_putChar(unsigned char ch)
{
    if (!m_font) return 0;
    //Ignore non-printable ASCII characters. This can be modified for
    //multilingual font.  
    if(ch < 32 || ch > 127) 
    {
        ch = ' ';
    }
    unsigned char i = 0;
    for(i=0;i<m_font_width;i++)
    {
       // Font array starts at 0, ASCII starts at 32
       SSD1306_sendData(pgm_read_byte(&m_font[(ch-32)*m_font_width+m_font_offset+i])); 
    }
}

void SSD1306_putString(const char *string)
{
    unsigned char i=0;
    while(string[i])
    {
        SSD1306_putChar(string[i]);     
        i++;
    }
}

unsigned char SSD1306_putNumber(long long_num)
{
  unsigned char char_buffer[10]="";
  unsigned char i = 0;
  unsigned char f = 0;

  if (long_num < 0) 
  {
    f=1;
    SSD1306_putChar('-');
    long_num = -long_num;
  } 
  else if (long_num == 0) 
  {
    f=1;
    SSD1306_putChar('0');
    return f;
  } 

  while (long_num > 0) 
  {
    char_buffer[i++] = long_num % 10;
    long_num /= 10;
  }

  f=f+i;
  for(; i > 0; i--)
  {
    SSD1306_putChar('0'+ char_buffer[i - 1]);
  }
  return f;

}

void SSD1306_drawBitmap(unsigned char *bitmaparray,int bytes)
{
  char localAddressMode = addressingMode;
  if(addressingMode != HORIZONTAL_MODE)
  {
      //Bitmap is drawn in horizontal mode     
      SSD1306_setHorizontalMode();
  }
  int i=0;
  for(i=0;i<bytes;i++)
  {
      SSD1306_sendData(pgm_read_byte(&bitmaparray[i]));
  }

  if(localAddressMode == PAGE_MODE)
  {
     //If pageMode was used earlier, restore it.
     SSD1306_setPageMode(); 
  }
  
}

void SSD1306_setHorizontalScrollProperties(int direction,unsigned char startPage, unsigned char endPage, unsigned char scrollSpeed)
{
   if(Scroll_Right == direction)
   {
        //Scroll right
        SSD1306_sendCommand(0x26);
   }
   else
   {
        //Scroll left  
        SSD1306_sendCommand(0x27);

   }
    SSD1306_sendCommand(0x00);
    SSD1306_sendCommand(startPage);
    SSD1306_sendCommand(scrollSpeed);
    SSD1306_sendCommand(endPage);
    SSD1306_sendCommand(0x00);
    SSD1306_sendCommand(0xFF);
}

void SSD1306_activateScroll()
{
    SSD1306_sendCommand(SSD1306_Activate_Scroll_Cmd);
}

void SSD1306_deactivateScroll()
{
    SSD1306_sendCommand(SSD1306_Dectivate_Scroll_Cmd);
}

void SSD1306_setNormalDisplay()
{
    SSD1306_sendCommand(SSD1306_Normal_Display_Cmd);
}

void SSD1306_setInverseDisplay()
{
    SSD1306_sendCommand(SSD1306_Inverse_Display_Cmd);
}

/***************************************************************************/
static int ssd1306_open(struct inode *inodep, struct file *filep){
    filep->private_data = ssd1306_client;
    return 0;
}

static ssize_t ssd1306_write(struct file *filep, const char __user *buf, size_t len, loff_t *pos){
    return 0;
}

static const struct file_operations ssd1306_file_operation = { 
    .open  = ssd1306_open,
    .write = ssd1306_write, 
};

static int ssd1306_probe(struct i2c_client *client, const struct i2c_device_id *id){
    int ret;
    printk(KERN_INFO"ssd1306 driver is loaded");
    ssd1306_client = client;

    ret = alloc_chrdev_region(&devnum, 0 , 1, "ssd1306" );
    if(ret < 0) {
        printk(KERN_ERR"alloc_chrdev_region fail\n");
        goto err_alloc_chrdev_region;
    }
    printk(KERN_INFO"major = %d\b",MAJOR(devnum) );
 
    chr_dev = cdev_alloc();
    if(!chr_dev ) {
        goto err_cdev_alloc;
    }
 
    cdev_init(chr_dev, &ssd1306_file_operation);
    chr_dev->owner = THIS_MODULE;
 
    ret = cdev_add(chr_dev, devnum, 1);
    if(ret) {
        printk(KERN_ERR"cdev_add fail");
        goto err_cdev_add;
 
    }

    drv_class = class_create(THIS_MODULE, "ssd1306");
    if(!drv_class) {
        printk(KERN_ERR"class_create fail\n");
        goto err_class_create;
    }
 
    device = device_create(drv_class,NULL, devnum, NULL, DEVICE_NAME);
    if( !device) {
        printk(KERN_ERR"device_create fail \n");
        goto err_device_create;
    }
    SSD1306_init();
	  SSD1306_clearDisplay();
    SSD1306_setBrightness(255);

    SSD1306_setPageMode();
	  SSD1306_setTextXY(0,0);
	  SSD1306_putString("ABCDEFGHIJKLMNOP");

    return 0;
err_device_create:
    class_destroy(drv_class);
err_class_create:  
err_cdev_add:
     cdev_del(chr_dev);
err_cdev_alloc:
     unregister_chrdev_region(devnum, 1);
err_alloc_chrdev_region:
     return ret;
}

static int ssd1306_remove(struct i2c_client *client)
{
    device_destroy(drv_class, devnum);
    class_destroy(drv_class);
    cdev_del(chr_dev);
    unregister_chrdev_region(devnum, 1);
 
    return 0;
}


static const struct i2c_device_id ssd1306_id_table[] = {
    { "ssd1306", 0 },
    {}
};

static const unsigned short normal_i2c[] = { 0x3c, I2C_CLIENT_END };


static struct i2c_driver ssd1306_driver = {
    .driver = {
        .name   = "ssd1306",        /* 这个随便起名字 */
    },
    .probe      = ssd1306_probe,
    .remove     = ssd1306_remove,
    .id_table   = ssd1306_id_table,
    .address_list = normal_i2c,
};

 
/* 声明段属性 */
module_i2c_driver(ssd1306_driver);
MODULE_LICENSE("GPL");