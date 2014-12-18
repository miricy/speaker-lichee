#include "tas5711.h"

static struct i2c_adapter *tas5711_adapter = NULL;
static struct i2c_client *tas5711_client = NULL;
static unsigned int tas5711_init_done = 0;//init flag
static int tas5711_i2c_id = 1;            //twi1
static script_item_u tas5711_reset_item;  //tas5711 reset gpio
static script_item_u tas5711_power_item;  //tas5711 power gpio

static dev_t dev_num;
static int dev_num_major;
struct tas5711_cdev *tas_cdev;
struct class *tas_class;

//write count bytes
int tas5711_write(const char *buf, int count)
{
    int ret = 0;
    if(tas5711_client != NULL && tas5711_init_done == 1){
        ret = i2c_master_send(tas5711_client, buf, count);
        if(DEBUG) print_debug("#tas5711_write:%d\n",ret);
	}
	if(ret < 0)
        print_err("#tas5711_write err\n");
    return ret;
}
//read count bytes
int tas5711_read(char sub_addr, char *buf)
{
    int ret = 0;
    if(tas5711_client != NULL && tas5711_init_done == 1){
		struct i2c_msg msgs[] = {
			{
				.addr	= tas5711_client->addr,
				.flags	= 0,
				.len	= 1,
				.buf	= &sub_addr,
			},
			{
				.addr	= tas5711_client->addr,
				.flags	= I2C_M_RD,
				.len	= 1,
				.buf	= buf,
			},
		};
		ret = i2c_transfer(tas5711_adapter, msgs, 2);
	}
    if(ret < 0)
        print_err("#tas5711_read err\n");
    return ret;
}

//n milliseconds
int delay_msec(int n)
{
    msleep(n);
    return 0;
}

void transmit_registers(cfg_reg *r, int n)
{
        int i = 0;
        while (i < n) {
            switch (r[i].command) {
            case CFG_META_SWITCH:
                // Used in legacy applications.  Ignored here.
                break;
            case CFG_META_DELAY:
                delay_msec(r[i].param);
                break;
            case CFG_META_BURST:
                tas5711_write((unsigned char *)&r[i+1], r[i].param);
                i += (r[i].param + 1)/2;
                break;
            default:
                tas5711_write((unsigned char *)&r[i], 2);
                break;
            }
            i++;
        }
}

//adjust tas5711 master volume
void tas5711_master_vol(int vol_level)
{
    if(vol_level > 15 || vol_level < 0){
	    print_err("set channel volume,invlid vol level.\n");
		return;
	}
	print_err("vol_level = %d\n",vol_level);
	cfg_reg m[] = {Master_vol, vol_to_reg[vol_level]};
    transmit_registers(m, sizeof(m)/sizeof(m[0]));
}
EXPORT_SYMBOL(tas5711_master_vol);

/*set tas5711 channel volume
 *vol_level:0~15
 */
void tas5711_channel_vol(ch_num ch, int vol_level)
{
    if(vol_level > 15 || vol_level < 0){
	    print_err("set channel volume,invlid vol level.\n");
		return;
	}
    switch(ch)
	{
	    case CH_1:{
            cfg_reg m[] = {Channel1_vol, vol_to_reg[vol_level]};
            transmit_registers(m, sizeof(m)/sizeof(m[0]));
		}break;
		case CH_2:{
            cfg_reg m[] = {Channel1_vol, vol_to_reg[vol_level]};
            transmit_registers(m, sizeof(m)/sizeof(m[0]));
		}break;
		case CH_3:{
            cfg_reg m[] = {Channel1_vol, vol_to_reg[vol_level]};
            transmit_registers(m, sizeof(m)/sizeof(m[0]));
		}break;

		default:
		     print_err("set channel volume,invlid channel!\n");
		     break;
	}
}

EXPORT_SYMBOL(tas5711_master_vol);
//set tas5711 mute
void tas5711_set_mute()
{
    cfg_reg m[] = {Soft_mute, 0x07}; //ch1~3 are mute
	transmit_registers(m, sizeof(m)/sizeof(m[0]));
}
EXPORT_SYMBOL(tas5711_set_mute);

//remove tas5711
static int __devexit tas5711_remove(struct i2c_client *client)
{
    //todo
    return 0;
}

static int tas5711_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    
    tas5711_client = client;
    tas5711_init_done = 1;
    return 0;
}

//en will be 0 or 1
void tas5711_power_en(int en)
{
    if(down_interruptible(&tas_cdev->tas_sem))
        return -ERESTARTSYS; 
    tas5711_power_item.gpio.data = en;
	gpio_set_value(tas5711_power_item.gpio.gpio, en);
	up(&tas_cdev->tas_sem);
}

void tas5711_set_reset(int en)
{
    tas5711_reset_item.gpio.data = en;
	gpio_set_value(tas5711_reset_item.gpio.gpio, en);
}

int tas5711_dev_init()
{
    if(tas5711_client != NULL && tas5711_init_done == 1)
    {
        print_debug("tas5711_dev_init\n");
	    tas5711_power_en(1);
		delay_msec(2);    //delay 2ms
		tas5711_set_reset(1);
		delay_msec(5);
		tas5711_set_reset(0);
		delay_msec(2);    //delay 2ms
		tas5711_set_reset(1);
		delay_msec(20);   //delay 20ms for stabilization
		//config registers
        transmit_registers(registers, sizeof(registers)/sizeof(registers[0]));
		return 0;
    }
	if(DEBUG) print_err("tas5711_dev_init failed\n");
	return -EFAULT;
}
EXPORT_SYMBOL(tas5711_dev_init);

int tas5711_volume_test()
{
    char buf;
	int ret;
	delay_msec(2); 
	tas5711_master_vol(15);
	delay_msec(2); 
	ret = tas5711_read(Master_vol,&buf);
	print_err("##master vol = 0x%x\n",buf);
	tas5711_master_vol(14);
	delay_msec(2);
	ret = tas5711_read(Master_vol,&buf);
	print_err("##master vol = 0x%x\n",buf);
	tas5711_master_vol(13);
	delay_msec(2);
	ret = tas5711_read(Master_vol,&buf);
	print_err("##master vol = 0x%x\n",buf);
	tas5711_master_vol(12);
	delay_msec(2); 
	ret = tas5711_read(Master_vol,&buf);
	print_err("##master vol = 0x%x\n",buf);

    return 0;
}
EXPORT_SYMBOL(tas5711_volume_test);

//power manager
#ifdef CONFIG_PM
    //suspend
	//resume
#endif

static ssize_t sunxi_masetvolume_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
    int ret,i;
	char result;
	ret = tas5711_read(Master_vol,&result);
	for(i=0; i<16; i++){
       if(vol_to_reg[i] == result)
	   	  break;
    }
	if(i == 16) i = 14;
	return sprintf(buf, "%d\n", i);
}

static ssize_t sunxi_masetvolume_store(struct device *dev,struct device_attribute *attr,
		const char *buf, size_t size)
{
    int ret;
    unsigned long vol;
    if(size < 1)
        return -EINVAL;

    ret = strict_strtoul(buf, 10, &vol);
    if(ret){
        print_err("invalid volume\n");
        return ret;
    }
    tas5711_master_vol((int)vol);
	return size;
}

static ssize_t sunxi_power_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t sunxi_power_store(struct device *dev,struct device_attribute *attr,
		const char *buf, size_t size)
{
    int ret,en;
    unsigned long vol;
    if(size < 1)
        return -EINVAL;
    ret = strict_strtoul(buf,10,&vol);
	if(ret){
        print_err("invalid parameter\n");
        return ret;
    }
	en = (int)vol;
	print_err("%s:en = %d\n",__func__,en);
    if(en){
        tas5711_power_en(1);
        print_debug("tas5711 power on\n");
		delay_msec(50);
		tas5711_dev_init();
    }else{
        tas5711_power_en(0);
        print_debug("tas5711 power off\n");
    }
	return size;
}

static DEVICE_ATTR(mastervol, S_IRUGO|S_IWUSR|S_IWGRP,
		sunxi_masetvolume_show, sunxi_masetvolume_store);
static DEVICE_ATTR(power, S_IRUGO|S_IWUSR|S_IWGRP,
		sunxi_power_show, sunxi_power_store);

static struct attribute *tas5711_attributes[] = {
	&dev_attr_mastervol.attr,
    &dev_attr_power.attr,
	NULL
};

static struct attribute_group tas5711_attribute_group = {
	.name = "tas5711",
	.attrs = tas5711_attributes
};

static struct miscdevice tas5711_dev = {
	.minor =	MISC_DYNAMIC_MINOR,
	.name =		"audio",
};
/* open the driver */
int tas5711_dev_open(struct inode *inode, struct file *filp)
{
    struct tas5711_cdev *tas_dev;
    /* get the pstr_dev from filp */
    tas_dev = container_of(inode->i_cdev,struct tas5711_cdev,cdev);
    /* store the pointer in filp,easy access */
    filp->private_data = tas_dev;
    return 0;
}

int tas5711_dev_read(struct file *file,char __user *buf,size_t size,loff_t *pos)
{
    return 0;
}

ssize_t tas5711_dev_write(struct file *file,const char __user *buf,size_t size,loff_t *pos)
{
    return 0;
}

int tas5711_dev_release(void)
{
    return 0;
}

int tas5711_dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    unsigned long karg[4];
	unsigned long ubuffer[4] = {0};
    int ret = 0;

    if (copy_from_user((void*)karg,(void __user*)arg,4*sizeof(unsigned long)))
    {
        print_err("%s:copy_from_user fail\n",__func__);
        return -EFAULT;
    }

    ubuffer[0] = *(unsigned long*)karg;
    ubuffer[1] = (*(unsigned long*)(karg+1));
    ubuffer[2] = (*(unsigned long*)(karg+2));
    ubuffer[3] = (*(unsigned long*)(karg+3));

    if(cmd > TAT5711_CMD_MAX_NUM){
        print_err("%s:invalid command\n",__func__);
    }
	switch(cmd){
        case TAS5711_SET_MASTER_VOLUME:
            tas5711_master_vol(ubuffer[0]);
            break;
		case TAS5711_GET_MASTER_VOLUME:{
            int i;
            char result;
            ret = tas5711_read(Master_vol,&result);
            for(i=0; i<16; i++){
               if(vol_to_reg[i] == result)
	   	           break;
            }
	        if(i == 16) i = 14;
                ret = i;
            }break;
        case TAS5711_SET_POWER:
            tas5711_power_en((int)ubuffer[0]);
			break;
        case TAS5711_SET_MUTE:
            break;
        default:
            print_err("%s:no case match\n",__func__);
			break;
    }
	return ret;
}

static const struct file_operations tas5711_fops ={
    .owner = THIS_MODULE,
    .open  = tas5711_dev_open,
    .unlocked_ioctl = tas5711_dev_ioctl,
    .read  = tas5711_dev_read,
    .write = tas5711_dev_write,
    .release =  tas5711_dev_release,
};

int tas5711_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	if(tas5711_i2c_id == client->adapter->nr){
        const char *type_name = TAS5711_DEV;
        tas5711_adapter = client->adapter;
        if(DEBUG) print_err("[audio]tas5711_detect, adapter, i2c id=%d\n", tas5711_adapter->nr);
        strlcpy(info->type, type_name, I2C_NAME_SIZE);
        return 0;
    }
    return -ENODEV;
}


static const struct i2c_device_id tas5711_id_table[] = {
    { TAS5711_DEV, 0 },
    {}
};
MODULE_DEVICE_TABLE(i2c, tas5711_id_table);

struct i2c_driver tas5711_driver =
{
    .driver = {
        .owner  = THIS_MODULE,
        .name   = TAS5711_DEV,
    },
    .class      = I2C_CLASS_HWMON,
    .id_table   = tas5711_id_table,
    .probe      = tas5711_probe,
    .remove     = __devexit_p(tas5711_remove),
    .detect     = tas5711_detect,
    .address_list = tas5711_addr,
};
// Initialization tas5711
static int __init tas5711_init(void)
{
	int ret = -1;
	int err = -1;
	int req_status = -1;
	script_item_value_type_e  type;
	if(DEBUG) print_debug("tas5711_init\r\n");
	//get gpio tas5711_reset
	type = script_get_item("tas5711", "tas5711_reset", &tas5711_reset_item);
	if (SCIRPT_ITEM_VALUE_TYPE_PIO != type) {
		printk("script_get_item return type err.\n");
		return -EFAULT;
	}
	/*request gpio*/
	req_status = gpio_request(tas5711_reset_item.gpio.gpio, NULL);
	if (0 != req_status) {
		print_err("request gpio failed!\n");
	}
	//set gpio output
	gpio_direction_output(tas5711_reset_item.gpio.gpio, 1);
	
	//get gpio tas5711_power
	type = script_get_item("tas5711", "tas5711_power", &tas5711_power_item);
	if (SCIRPT_ITEM_VALUE_TYPE_PIO != type) {
		print_err("script_get_item return type err.\n");
		return -EFAULT;
	}
	/*request gpio*/
	req_status = gpio_request(tas5711_power_item.gpio.gpio, NULL);
	if (0 != req_status) {
		print_err("request gpio failed!\n");
	}
	//set gpio output
	gpio_direction_output(tas5711_power_item.gpio.gpio, 1);
    ret = i2c_add_driver(&tas5711_driver);

    if(alloc_chrdev_region(&dev_num,0,1,DEVICE_NAME)<0){
        print_err("%s:fail to alloc char device\n");
        return -1;
    }
	err = misc_register(&tas5711_dev);
	if(err) {
		pr_err("%s register driver as misc device error\n", __FUNCTION__);
		goto exit;
	}
	/* get the major num*/
	dev_num_major = MAJOR(dev_num);
	/* alloc mem to device*/
	tas_cdev = kmalloc(sizeof (struct tas5711_cdev),GFP_KERNEL);
	if(!tas_cdev){
		ret = - ENOMEM;
        goto fail_kmalloc;
    }
		/* clear memory */	
		memset(tas_cdev,0,sizeof(struct tas5711_cdev));
		/* initializate device and bind file_operations ,register device to system */
		cdev_init(&tas_cdev->cdev,&tas5711_fops);
		err = cdev_add(&tas_cdev->cdev,dev_num,1);
		/* fail to add device */
		if(err){
            print_err("%s:Error adding tas_cdev\n",__func__);
            return -1;
        }
		/* create device pstr in /proc */
        tas_class = class_create(THIS_MODULE,"tas5711");
		device_create(tas_class,NULL,dev_num,NULL,"tas5711");

        // create attribute group
	    err = sysfs_create_group(&tas5711_dev.this_device->kobj,&tas5711_attribute_group);
	    if(err){
	        print_err("%s sysfs_create_group  error\n", __FUNCTION__);
        }
		sema_init(&tas_cdev->tas_sem,1);
exit:
	if(DEBUG) print_debug("tas5711_init done!\n");
    return ret;
fail_kmalloc:
    unregister_chrdev_region(dev_num,1);
    return err;
}

static void __exit tas5711_exit(void)
{
    // unregister tas5711 misc dev
	misc_deregister(&tas5711_dev);
	sysfs_remove_group(&tas5711_dev.this_device->kobj,&tas5711_attribute_group);
    // delete char device
    cdev_del(&tas_cdev->cdev);
	/* free the memories */
	kfree(tas_cdev);
    unregister_chrdev_region(dev_num,1);
    i2c_del_driver(&tas5711_driver);
	tas5711_client = NULL;
}

late_initcall(tas5711_init);
module_exit(tas5711_exit);
MODULE_AUTHOR("allwinner");
MODULE_DESCRIPTION("tas5711 driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("CODEC TAS5711");
