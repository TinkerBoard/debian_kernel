/*
 *
 * TINKER BOARD FT5406 touch driver.
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
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/module.h>
#include <linux/workqueue.h>
#include "tinker_ft5406.h"

struct tinker_ft5406_data *g_ts_data = NULL;
int g_mcu_ready = 0;

static int fts_i2c_read(struct i2c_client *client, char *writebuf,
			   int writelen, char *readbuf, int readlen)
{
	int ret;

	if (writelen > 0) {
		struct i2c_msg msgs[] = {
			{
				 .addr = client->addr,
				 .flags = 0,
				 .len = writelen,
				 .buf = writebuf,
			 },
			{
				 .addr = client->addr,
				 .flags = I2C_M_RD,
				 .len = readlen,
				 .buf = readbuf,
			 },
		};
		ret = i2c_transfer(client->adapter, msgs, 2);
		if (ret < 0)
			LOG_ERR("i2c read error, %d\n", ret);
	} else {
		struct i2c_msg msgs[] = {
			{
				 .addr = client->addr,
				 .flags = I2C_M_RD,
				 .len = readlen,
				 .buf = readbuf,
			 },
		};
		ret = i2c_transfer(client->adapter, msgs, 1);
		if (ret < 0)
			LOG_ERR("i2c read error, %d\n", ret);
	}

	return ret;
}

static int fts_read_reg(struct i2c_client *client, u8 addr, u8 *val)
{
	return fts_i2c_read(client, &addr, 1, val, 1);
}

static int fts_check_fw_ver(struct i2c_client *client)
{
	u8 reg_addr, fw_ver[3];
	int ret;

	reg_addr = FT_REG_FW_VER;
	ret = fts_i2c_read(client, &reg_addr, 1, &fw_ver[0], 1);
	if (ret < 0)
		goto error;

	reg_addr = FT_REG_FW_MIN_VER;
	ret = fts_i2c_read(client, &reg_addr, 1, &fw_ver[1], 1);
	if (ret < 0)
		goto error;

	reg_addr = FT_REG_FW_SUB_MIN_VER;
	ret = fts_i2c_read(client, &reg_addr, 1, &fw_ver[2], 1);
	if (ret < 0)
		goto error;

	LOG_INFO("Firmware version = %d.%d.%d\n", fw_ver[0], fw_ver[1], fw_ver[2]);
	return 0;

error:
	return ret;
}

static int fts_read_td_status(struct tinker_ft5406_data *ts_data)
{
	u8 td_status;
	int ret = -1;
	ret = fts_read_reg(ts_data->client, FT_TD_STATUS_REG, &td_status);
	if (ret < 0) {
		LOG_ERR("get reg td_status failed, %d\n", ret);
		return ret;
	}
	return (int)td_status;
}

static int fts_read_touchdata(struct tinker_ft5406_data *ts_data)
{
	struct ts_event *event = &ts_data->event;
	int ret = -1, i;
	u8 buf[FT_ONE_TCH_LEN-2] = { 0 };
	u8 reg_addr, pointid = FT_MAX_ID;

	for (i = 0; i < event->touch_point && i < MAX_TOUCH_POINTS; i++) {
		reg_addr = FT_TOUCH_X_H_REG + (i * FT_ONE_TCH_LEN);
		ret = fts_i2c_read(ts_data->client, &reg_addr, 1, buf, FT_ONE_TCH_LEN-2);
		if (ret < 0) {
			LOG_ERR("read touchdata failed.\n");
			return ret;
		}

		pointid = (buf[FT_TOUCH_ID]) >> 4;
		if (pointid >= MAX_TOUCH_POINTS)
			break;
		event->au8_finger_id[i] = pointid;
		event->au16_x[i] = (s16) (buf[FT_TOUCH_X_H] & 0x0F) << 8 | (s16) buf[FT_TOUCH_X_L];
		event->au16_y[i] = (s16) (buf[FT_TOUCH_Y_H] & 0x0F) << 8 | (s16) buf[FT_TOUCH_Y_L];
		event->au8_touch_event[i] = buf[FT_TOUCH_EVENT] >> 6;

		if (ts_data->xy_reverse) {
			event->au16_x[i] = ts_data->screen_width - event->au16_x[i] - 1;
			event->au16_y[i] = ts_data->screen_height - event->au16_y[i] - 1;
		}
	}
	event->pressure = FT_PRESS;

	return 0;
}

static void fts_report_value(struct tinker_ft5406_data *ts_data)
{
	struct ts_event *event = &ts_data->event;
	int i, modified_ids = 0, released_ids;

	for (i = 0; i < event->touch_point && i < MAX_TOUCH_POINTS; i++) {
		if (event->au8_touch_event[i]== FT_TOUCH_DOWN
				|| event->au8_touch_event[i] == FT_TOUCH_CONTACT)
		{
			modified_ids |= 1 << event->au8_finger_id[i];
			input_mt_slot(ts_data->input_dev, event->au8_finger_id[i]);
			input_mt_report_slot_state(ts_data->input_dev, MT_TOOL_FINGER,
				true);
			input_report_abs(ts_data->input_dev, ABS_MT_TOUCH_MAJOR,
					event->pressure);
			input_report_abs(ts_data->input_dev, ABS_MT_POSITION_X,
					event->au16_x[i]);
			input_report_abs(ts_data->input_dev, ABS_MT_POSITION_Y,
					event->au16_y[i]);

			if(!((1 << event->au8_finger_id[i]) & ts_data->known_ids))
				LOG_DBG("Touch id-%d: x = %d, y = %d\n",
					event->au8_finger_id[i], event->au16_x[i], event->au16_y[i]);
		}
	}

	released_ids = ts_data->known_ids & ~modified_ids;
	for(i = 0; released_ids && i < MAX_TOUCH_POINTS; i++) {
		if(released_ids & (1<<i)) {
			LOG_DBG("Release id-%d, known = %x modified = %x\n", i, ts_data->known_ids, modified_ids);
			input_mt_slot(ts_data->input_dev, i);
			input_mt_report_slot_state(ts_data->input_dev, MT_TOOL_FINGER, false);
			modified_ids &= ~(1 << i);
		}
	}
	ts_data->known_ids = modified_ids;
	input_mt_report_pointer_emulation(ts_data->input_dev, true);
	input_sync(ts_data->input_dev);
}

extern int tinker_mcu_is_connected(void);
extern int tinker_mcu_ili9881c_is_connected(void);

static void fts_retry_clear(struct tinker_ft5406_data *ts_data)
{
	if (ts_data->retry_count != 0)
		ts_data->retry_count = 0;
}

static int fts_retry_wait(struct tinker_ft5406_data *ts_data)
{
	if (ts_data->retry_count < RETRY_COUNT) {
		LOG_INFO("wait and retry, count = %d\n", ts_data->retry_count)
		ts_data->retry_count++;
		msleep(1000);
		return 1;
	}
	LOG_ERR("attach retry count\n");
	return 0;
}

static void tinker_ft5406_work(struct work_struct *work)
{
	struct ts_event *event = &g_ts_data->event;
	int ret = 0, count = 5, td_status;

	while(count > 0) {
		ret = fts_check_fw_ver(g_ts_data->client);
		if (ret == 0)
			break;
		LOG_INFO("checking touch ic, countdown: %d\n", count);
		msleep(1000);
		count--;
	}
	if (!count) {
		LOG_ERR("checking touch ic timeout, %d\n", ret);
		g_ts_data->is_polling = 0;
		return;
	}

	//polling 60fps
	while(1) {
		td_status = fts_read_td_status(g_ts_data);
		if (td_status < 0) {
			ret = fts_retry_wait(g_ts_data);
			if (ret == 0) {
				LOG_ERR("stop touch polling\n");
				g_ts_data->is_polling = 0;
				break;
			}
		} else if (td_status < VALID_TD_STATUS_VAL+1 && (td_status > 0 || g_ts_data->known_ids != 0)) {
			fts_retry_clear(g_ts_data);
			memset(event, -1, sizeof(struct ts_event));
			event->touch_point = td_status;
			ret = fts_read_touchdata(g_ts_data);
			if (ret == 0)
				fts_report_value(g_ts_data);
		}
		msleep_interruptible(17);
	}
}

void tinker_ft5406_start_polling(void)
{
	if (g_ts_data == NULL) {
		LOG_ERR("touch is not ready\n");
	} else if (g_ts_data->is_polling == 1) {
		LOG_ERR("touch is busy\n");
	} else {
		g_ts_data->is_polling = 1;
		schedule_work(&g_ts_data->ft5406_work);
	}
	g_mcu_ready = 1;
}
EXPORT_SYMBOL_GPL(tinker_ft5406_start_polling);

static int tinker_ft5406_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct input_dev *input_dev;
	int ret = 0, timeout = 10;

	LOG_INFO("address = 0x%x\n", client->addr);

	g_ts_data = kzalloc(sizeof(struct tinker_ft5406_data), GFP_KERNEL);
	if (g_ts_data == NULL) {
		LOG_ERR("no memory for device\n");
		return -ENOMEM;
	}

	g_ts_data->client = client;
	i2c_set_clientdata(client, g_ts_data);

	while(!tinker_mcu_is_connected() && !tinker_mcu_ili9881c_is_connected() && timeout > 0) {
		msleep(50);
		timeout--;
	}

	if (timeout == 0) {
		LOG_ERR("wait connected timeout\n");
		ret = -ENODEV;
		goto timeout_failed;
	}

	if (tinker_mcu_ili9881c_is_connected()) {
		g_ts_data->screen_width = 720;
		g_ts_data->screen_height = 1280;
		g_ts_data->xy_reverse = 0;
	} else {
		g_ts_data->screen_width = 800;
		g_ts_data->screen_height = 480;
		g_ts_data->xy_reverse = 1;
	}
	LOG_INFO("width = %d, height = %d, reverse = %d\n",
			g_ts_data->screen_width, g_ts_data->screen_height, g_ts_data->xy_reverse);

	input_dev = input_allocate_device();
	if (!input_dev) {
		LOG_ERR("failed to allocate input device\n");
		goto input_allocate_failed;
	}
	input_dev->name = "fts_ts";
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &g_ts_data->client->dev;

	g_ts_data->input_dev = input_dev;
	input_set_drvdata(input_dev, g_ts_data);

	__set_bit(EV_SYN, input_dev->evbit);
	__set_bit(EV_KEY, input_dev->evbit);
	__set_bit(EV_ABS, input_dev->evbit);
	__set_bit(BTN_TOUCH, input_dev->keybit);

	input_mt_init_slots(input_dev, MAX_TOUCH_POINTS, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, g_ts_data->screen_width, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, g_ts_data->screen_height, 0, 0);

	ret = input_register_device(input_dev);
	if (ret) {
		LOG_ERR("Input device registration failed\n");
		goto input_register_failed;
	}

	INIT_WORK(&g_ts_data->ft5406_work, tinker_ft5406_work);
	if (g_mcu_ready == 1)
		schedule_work(&g_ts_data->ft5406_work);

	return 0;

input_register_failed:
	input_free_device(input_dev);
input_allocate_failed:
timeout_failed:
	kfree(g_ts_data);
	g_ts_data = NULL;
	return ret;
}

static int tinker_ft5406_remove(struct i2c_client *client)
{
	cancel_work_sync(&g_ts_data->ft5406_work);
	if (g_ts_data->input_dev) {
		input_unregister_device(g_ts_data->input_dev);
		input_free_device(g_ts_data->input_dev);
	}
	kfree(g_ts_data);
	g_ts_data = NULL;
	g_mcu_ready = 0;
	return 0;
}

static const struct i2c_device_id tinker_ft5406_id[] = {
	{"tinker_ft5406", 0},
	{},
};

static struct i2c_driver tinker_ft5406_driver = {
	.driver = {
		.name = "tinker_ft5406",
	},
	.probe = tinker_ft5406_probe,
	.remove = tinker_ft5406_remove,
	.id_table = tinker_ft5406_id,
};
module_i2c_driver(tinker_ft5406_driver);

MODULE_DESCRIPTION("TINKER BOARD FT5406 Touch driver");
MODULE_LICENSE("GPL v2");

