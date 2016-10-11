#include <linux/module.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/workqueue.h>
#include "rpi_ts_mcu.h"

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

static void send_cmds(struct i2c_client *client, const char *buf)
{
	int ret, size = strlen(buf);
	unsigned char byte_cmd[size/2];

	if ((size%2) != 0) {
		LOG_ERR("size should be even\n");
		return;
	}

	LOG_INFO("%s\n", buf);

	string_to_byte(buf, byte_cmd, size);

	ret = i2c_master_send(client, byte_cmd, size/2);
	if (ret < 0) {
		LOG_ERR("send command failed, ret = %d\n", ret);
	}
	msleep(20);
}

static void recv_cmds(struct i2c_client *client, char *buf, int size)
{
	int ret;

	ret = i2c_master_recv(client, buf, size);
	if (ret < 0) {
		LOG_ERR("receive commands failed, %d\n", ret);
		return;
	}
	msleep(20);
}

DECLARE_COMPLETION(bridge_ready_comp);
EXPORT_SYMBOL_GPL(bridge_ready_comp);

static void mcu_power_up(struct work_struct *work)
{
	struct ts_mcu_data *mcu_data = container_of(work, struct ts_mcu_data, work.work);

	char recv_buf[1];

	send_cmds(mcu_data->client, "80");

	recv_cmds(mcu_data->client, recv_buf, 1);
	LOG_INFO("recv_cmds: 0x%X\n", recv_buf[0]);
	if (recv_buf[0] != 0xC3) {
		LOG_ERR("read wrong info\n");
		complete(&bridge_ready_comp);
		return;
	}

	msleep(500);
	send_cmds(mcu_data->client, "8500");
	msleep(2000);
	send_cmds(mcu_data->client, "8501");
	send_cmds(mcu_data->client, "86FF");
	send_cmds(mcu_data->client, "8104");

	complete(&bridge_ready_comp);
}

static int rpi_ts_mcu_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct ts_mcu_data *mcu_data;

	LOG_INFO("address = 0x%x\n", client->addr);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		LOG_ERR("I2C check functionality failed\n");
		return -ENODEV;
	}

	mcu_data = kzalloc(sizeof(struct ts_mcu_data), GFP_KERNEL);
	if (mcu_data == NULL) {
		LOG_ERR("no memory for device\n");
		return -ENOMEM;
	}

	mcu_data->client = client;
	i2c_set_clientdata(client, mcu_data);

	mcu_data->wq = create_singlethread_workqueue("rpi_ts_mcu_wq");
	INIT_DELAYED_WORK(&mcu_data->work, mcu_power_up);
	queue_delayed_work(mcu_data->wq, &mcu_data->work, 0.5 * HZ);

	return 0;
}

static int rpi_ts_mcu_remove(struct i2c_client *client)
{
	struct ts_mcu_data *mcu_data = i2c_get_clientdata(client);
	cancel_delayed_work_sync(&mcu_data->work);
	destroy_workqueue(mcu_data->wq);
	kfree(mcu_data);
	return 0;
}

static const struct i2c_device_id rpi_ts_mcu_id[] = {
	{"rpi_ts_mcu", 0},
	{},
};

static struct i2c_driver rpi_ts_mcu_driver = {
	.driver = {
		.name = "rpi_ts_mcu",
	},
	.probe = rpi_ts_mcu_probe,
	.remove = rpi_ts_mcu_remove,
	.id_table = rpi_ts_mcu_id,
};
module_i2c_driver(rpi_ts_mcu_driver);

MODULE_DESCRIPTION("RPI TouchScreen MCU driver");
MODULE_LICENSE("GPL v2");
