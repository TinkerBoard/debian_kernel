/*
 *
 * Tinker board Touchscreen MCU driver.
 *
 * Copyright (c) 2016 ASUSTek Computer Inc.
 * Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/backlight.h>
#include "tinker_mcu.h"

#define BL_DEBUG 0
static struct tinker_mcu_data *g_mcu_ili9881c_data;
static int connected = 0;
static int lcd_bright_level = 0;
int lcd_size_flag = 0;

static int is_hex(char num)
{
	//0-9, a-f, A-F
	if ((47 < num && num < 58) || (64 < num && num < 71) || (96 < num && num < 103))
		return 1;
	return 0;
}

static int string_to_byte(const char *source, unsigned char *destination, int size)
{
	int i = 0, counter = 0;
	char c[3] = {0};
	unsigned char bytes;

	if (size%2 == 1)
		return -EINVAL;

	for(i = 0; i < size; i++){
		if(!is_hex(source[i])) {
			return -EINVAL;
		}
		if(0 == i%2){
			c[0] = source[i];
			c[1] = source[i+1];
			sscanf(c, "%hhx", &bytes);
			destination[counter] = bytes;
			counter++;
		}
	}
	return 0;
}

static int send_cmds(struct i2c_client *client, const char *buf)
{
	int ret, size = strlen(buf);
	unsigned char byte_cmd[size/2];

	if ((size%2) != 0) {
		LOG_ERR("size should be even\n");
		return -EINVAL;
	}

	LOG_INFO("%s\n", buf);

	string_to_byte(buf, byte_cmd, size);

	ret = i2c_master_send(client, byte_cmd, size/2);
	if (ret <= 0) {
		LOG_ERR("send command failed, ret = %d\n", ret);
		return ret!=0 ? ret : -ECOMM;
	}
	msleep(20);
	return 0;
}

static int recv_cmds(struct i2c_client *client, char *buf, int size)
{
	int ret;

	ret = i2c_master_recv(client, buf, size);
	if (ret <= 0) {
		LOG_ERR("receive commands failed, %d\n", ret);
		return ret!=0 ? ret : -ECOMM;
	}
	msleep(20);
	return 0;
}

static int init_cmd_check(struct tinker_mcu_data *mcu_data)
{
	int ret;
	char recv_buf[1] = {0};

	ret = send_cmds(mcu_data->client, "04");
	if (ret < 0)
		goto error;

	recv_cmds(mcu_data->client, recv_buf, 1);
	if (ret < 0)
		goto error;

	LOG_INFO("recv_cmds: 0x%X\n", recv_buf[0]);
	printk("****lcd size value is: 0x%X\n", recv_buf[0]);

	if(recv_buf[0]==0x85)
		lcd_size_flag = 0;
	else if(recv_buf[0]==0x89)
		lcd_size_flag = 1;
	else if(recv_buf[0]==0x8d)
		lcd_size_flag = 2;

	return 0;

error:
	return ret;
}

int tinker_mcu_ili9881c_screen_power_up(void)
{
	if (!connected)
		return -ENODEV;

	LOG_INFO("\n");
	send_cmds(g_mcu_ili9881c_data->client, "0503");
	msleep(20);
	send_cmds(g_mcu_ili9881c_data->client, "0500");
	msleep(20);
	send_cmds(g_mcu_ili9881c_data->client, "0503");
	msleep(200);

	return 0;
}
EXPORT_SYMBOL_GPL(tinker_mcu_ili9881c_screen_power_up);

int tinker_mcu_ili9881c_set_bright(int bright)
{
	unsigned char cmd[2];
	int ret;

	if (!connected)
		return -ENODEV;

	if (bright > 0xff || bright < 0)
		return -EINVAL;

	if(BL_DEBUG) LOG_INFO("set bright = 0x%x\n", bright);

	cmd[0] = 0x06;
	cmd[1] = bright | 0x80;

	ret = i2c_master_send(g_mcu_ili9881c_data->client, cmd, 2);
	if (ret <= 0) {
		LOG_ERR("send command failed, ret = %d\n", ret);
		return ret != 0 ? ret : -ECOMM;
	}

	lcd_bright_level = bright;

	return 0;
}
EXPORT_SYMBOL_GPL(tinker_mcu_ili9881c_set_bright);

int tinker_mcu_ili9881c_get_brightness(void)
{
	return lcd_bright_level;
}
EXPORT_SYMBOL_GPL(tinker_mcu_ili9881c_get_brightness);

static int tinker_mcu_ili9881c_bl_get_brightness(struct backlight_device *bd)
{
	return lcd_bright_level;
}

static const struct backlight_ops tinker_mcu_ili9881c_bl_ops = {
	.get_brightness	= tinker_mcu_ili9881c_bl_get_brightness,
};


static ssize_t tinker_mcu_ili9881c_bl_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    if(BL_DEBUG) LOG_INFO("get bright = 0x%x\n", lcd_bright_level);

    return sprintf(buf, "%d\n", lcd_bright_level);
}

static ssize_t tinker_mcu_ili9881c_bl_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int value;

	value = simple_strtoul(buf, NULL, 0);

	if((value < 0) || (value > 255)) {
		LOG_ERR("Invalid value for backlight setting, value = %d\n", value);
	} else
		tinker_mcu_ili9881c_set_bright(value);

	return strnlen(buf, count);
}
static DEVICE_ATTR(tinker_mcu_ili9881c_bl, S_IRUGO | S_IWUSR, tinker_mcu_ili9881c_bl_show, tinker_mcu_ili9881c_bl_store);

int tinker_mcu_ili9881c_is_connected(void)
{
	return connected;
}
EXPORT_SYMBOL_GPL(tinker_mcu_ili9881c_is_connected);

static int tinker_mcu_ili9881c_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct tinker_mcu_data *mcu_data;
	int ret;
	struct backlight_properties props;
	struct backlight_device *bl;

	LOG_INFO("address = 0x%x\n", client->addr);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		LOG_ERR("I2C check functionality failed\n");
		return -ENODEV;
	}

	mcu_data = kzalloc(sizeof(struct tinker_mcu_data), GFP_KERNEL);
	if (mcu_data == NULL) {
		LOG_ERR("no memory for device\n");
		return -ENOMEM;
	}

	mcu_data->client = client;
	i2c_set_clientdata(client, mcu_data);
	g_mcu_ili9881c_data = mcu_data;

	ret = init_cmd_check(mcu_data);
	if (ret < 0) {
		LOG_ERR("init_cmd_check failed, %d\n", ret);
		goto error;
	}
	connected = 1;

	memset(&props, 0, sizeof(props));
	props.type = BACKLIGHT_RAW;
	props.max_brightness = 255;

	bl = backlight_device_register("panel_backlight", NULL, NULL,
					   &tinker_mcu_ili9881c_bl_ops, &props);
	if (IS_ERR(bl)) {
		pr_err("unable to register backlight device\n");
	}

	ret = device_create_file(&client->dev, &dev_attr_tinker_mcu_ili9881c_bl);
	if (ret != 0) {
		dev_err(&client->dev, "Failed to create tinker_mcu_ili9881c_bl sysfs files %d\n", ret);
		return ret;
	}

	return 0;

error:
	kfree(mcu_data);
	return ret;
}

static int tinker_mcu_ili9881c_remove(struct i2c_client *client)
{
	struct tinker_mcu_data *mcu_data = i2c_get_clientdata(client);
	connected = 0;
	kfree(mcu_data);
	return 0;
}

static const struct i2c_device_id tinker_mcu_ili9881c_id[] = {
	{"tinker_mcu_ili9881c", 0},
	{},
};

static struct i2c_driver tinker_mcu_ili9881c_driver = {
	.driver = {
		.name = "tinker_mcu_ili9881c",
	},
	.probe = tinker_mcu_ili9881c_probe,
	.remove = tinker_mcu_ili9881c_remove,
	.id_table = tinker_mcu_ili9881c_id,
};
module_i2c_driver(tinker_mcu_ili9881c_driver);

MODULE_DESCRIPTION("Tinker Board TouchScreen MCU ili9881c driver");
MODULE_LICENSE("GPL v2");