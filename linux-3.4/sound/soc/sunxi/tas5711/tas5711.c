#include "tas5711.h"

static struct i2c_adapter *tas5711_adapter = NULL;
static struct i2c_client *tas5711_client = NULL;
static unsigned int tas5711_init_done = 0;//init flag
static int tas5711_i2c_id = 1;            //twi1
static script_item_u tas5711_reset_item;  //tas5711 reset gpio
static script_item_u tas5711_power_item;  //tas5711 power gpio

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
void tas5711_master_vol(unsigned char vol_level)
{
    //todo
}
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
            cfg_reg *m = {Channel1_vol, vol_to_reg[vol_level]};
            transmit_registers(m, 2);
		}break;
		case CH_2:{
            cfg_reg *m = {Channel1_vol, vol_to_reg[vol_level]};
            transmit_registers(m, 2);
		}break;
		case CH_3:{
            cfg_reg *m = {Channel1_vol, vol_to_reg[vol_level]};
            transmit_registers(m, 2);
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
    cfg_reg *m = {Soft_mute, 0x07}; //ch1~3 are mute
	transmit_registers(m, 2);
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
    tas5711_power_item.gpio.data = en;
	gpio_set_value(tas5711_power_item.gpio.gpio, en);
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

//power manager
#ifdef CONFIG_PM
    //suspend
	//resume
#endif

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
	int ret = 0;
	int req_status = -1;
	script_item_value_type_e  type;
	if(DEBUG) print_debug("#tas5711_init\r\n");
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
	if(DEBUG) print_debug("##tas5711_init done!\n");
    return ret;
}

static void __exit tas5711_exit(void)
{
    i2c_del_driver(&tas5711_driver);
	tas5711_client = NULL;
}

late_initcall(tas5711_init);
module_exit(tas5711_exit);
MODULE_AUTHOR("linjunqian");
MODULE_DESCRIPTION("tas5711 driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("CODEC TAS5711");
