#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/proc_fs.h>

#define RK3288_GRF_GPIO2A_P		0x0150

static int hw_id0, hw_id1, hw_id2;
static int pid_id0, pid_id1, pid_id2;
static int ddr_id0, ddr_id1, ddr_id2;

static const struct of_device_id of_board_info_match[] = {
	{ .compatible = "board-info", },
	{},
};
MODULE_DEVICE_TABLE(of, of_board_info_match);

static int ver_show(struct seq_file *m, void *v)
{
	int hwid0, hwid1, hwid2, pid0, pid1, pid2;
	int hwid, pid;
	char *boardver;

	hwid0 = gpio_get_value(hw_id0);
	hwid1 = gpio_get_value(hw_id1);
	hwid2 = gpio_get_value(hw_id2);

	hwid = (hwid2 << 2) + (hwid1 << 1) + hwid0;

	pid0 = gpio_get_value(pid_id0);
	pid1 = gpio_get_value(pid_id1);
	pid2 = gpio_get_value(pid_id2);

	pid = (pid2 << 2) + (pid1 << 1) + pid0;

	if (hwid == 0) {
		if (pid == 7)
			boardver = "1.0";
		else
			boardver = "0.99";
	} else if (hwid == 1) {
		if (pid == 7)
			boardver = "1.1";
		else
			boardver = "1.00";
	} else if (hwid == 2) {
		if (pid == 7)
			boardver = "1.2";
		else
			boardver = "1.01";
	} else if (hwid == 3)
		boardver = "2.00";
	else if (hwid == 4)
		boardver = "2.01A";
	else if (hwid == 5)
		boardver = "2.01B";
	else
		boardver = "unknown";

	seq_printf(m, "%s\n", boardver);
	return 0;
}

static int info_show(struct seq_file *m, void *v)
{
	int pid0, pid1, pid2, hwid0, hwid1, hwid2;
	int pid, hwid;
	char *boardinfo;

	pid0 = gpio_get_value(pid_id0);
	pid1 = gpio_get_value(pid_id1);
	pid2 = gpio_get_value(pid_id2);

	pid = (pid2 << 2) + (pid1 << 1) + pid0;

	hwid0 = gpio_get_value(hw_id0);
	hwid1 = gpio_get_value(hw_id1);
	hwid2 = gpio_get_value(hw_id2);

	hwid = (hwid2 << 2) + (hwid1 << 1) + hwid0;

	if (pid == 0) {
		if (hwid < 3)
			boardinfo = "Tinker Board S";
		else
			boardinfo = "Tinker Board S R2.0";
	} else if (pid == 1)
		boardinfo = "Tinker Board S/HV";
	else if (pid == 2)
		boardinfo = "Tinker Board S";
	else if (pid == 3)
		boardinfo = "Tinker Board (S) R2.0 SR";
	else if (pid == 4)
		boardinfo = "Tinker R/BR";
	else if (pid == 5)
		boardinfo = "Tinker R/BR";
	else if (pid == 7)
		if (hwid < 3)
			boardinfo = "Tinker Board";
		else
			boardinfo = "Tinker Board R2.0";
	else
		boardinfo = "unknown";

	seq_printf(m, "%s\n", boardinfo);
	return 0;
}

static int bid_show(struct seq_file *m, void *v)
{
	int id0, id1, id2;
	int hwid = -1;

	id0 = gpio_get_value(hw_id0);
	id1 = gpio_get_value(hw_id1);
	id2 = gpio_get_value(hw_id2);

	hwid = (id2 << 2) + (id1 << 1) + id0;

	seq_printf(m, "%d\n", hwid);
	return 0;
}

static int pid_show(struct seq_file *m, void *v)
{
	int id0, id1, id2;
	int pid = -1;

	id0 = gpio_get_value(pid_id0);
	id1 = gpio_get_value(pid_id1);
	id2 = gpio_get_value(pid_id2);

	pid = (id2 << 2) + (id1 << 1) + id0;

	seq_printf(m, "%d\n", pid);
	return 0;
}

static int ddr_show(struct seq_file *m, void *v)
{
	int id0, id1, id2;
	int ddrid;
	char *ddr;

	id0 = gpio_get_value(ddr_id0);
	id1 = gpio_get_value(ddr_id1);
	id2 = gpio_get_value(ddr_id2);

	ddrid = (id2 << 2) + (id1 << 1) + id0;

	if (ddrid == 0)
		ddr = "4GB";
	else if (ddrid == 2)
		ddr = "2GB";
	else if (ddrid == 4)
		ddr = "1GB";
	else
		ddr = "unknown";

	seq_printf(m, "%s\n", ddr);
	return 0;
}

static int ver_open(struct inode *inode, struct file *file)
{
	return single_open(file, ver_show, NULL);
}

static int info_open(struct inode *inode, struct file *file)
{
	return single_open(file, info_show, NULL);
}

static int bid_open(struct inode *inode, struct file *file)
{
	return single_open(file, bid_show, NULL);
}

static int pid_open(struct inode *inode, struct file *file)
{
	return single_open(file, pid_show, NULL);
}

static int ddr_open(struct inode *inode, struct file *file)
{
	return single_open(file, ddr_show, NULL);
}

static struct file_operations boardver_ops = {
	.owner	= THIS_MODULE,
	.open	= ver_open,
	.read	= seq_read,
};

static struct file_operations boardinfo_ops = {
	.owner	= THIS_MODULE,
	.open	= info_open,
	.read	= seq_read,
};

static struct file_operations boardid_ops = {
	.owner	= THIS_MODULE,
	.open	= bid_open,
	.read	= seq_read,
};

static struct file_operations projectid_ops = {
	.owner	= THIS_MODULE,
	.open	= pid_open,
	.read	= seq_read,
};

static struct file_operations ddr_ops = {
	.owner  = THIS_MODULE,
	.open   = ddr_open,
	.read   = seq_read,
};

static int board_info_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int ret;
	struct proc_dir_entry* file;

	void *regs;
	/* Pull up GPIO2 A1 A2 A3*/
	regs = ioremap(0xff770000, 64*1024);
	if (regs == NULL) {
		printk("[boardinfo] ioremap failed");
		return -ENODEV;
	}
	writel((readl(regs + RK3288_GRF_GPIO2A_P) & ~(0x3f << 18) & ~(0x3f << 2))
			| (0x3f << 18) | (0x15 << 2), regs + RK3288_GRF_GPIO2A_P);

	iounmap(regs);

	hw_id0 = of_get_named_gpio(dev->of_node, "hw-id0", 0);
	if (!gpio_is_valid(hw_id0)) {
		printk("No hw-id0 pin available in board-info\n");
		return -ENODEV;
	} else {
		ret = devm_gpio_request_one(dev, hw_id0, GPIOF_DIR_IN, "HW_ID0");
		if (ret < 0) {
			printk("Fail to set hw-id0 pin\n");
			return ret;
		}
	}

	hw_id1 = of_get_named_gpio(dev->of_node, "hw-id1", 0);
	if (!gpio_is_valid(hw_id1)) {
		printk("No hw-id1 pin available in board-info\n");
		return -ENODEV;
	} else {
		ret = devm_gpio_request_one(dev, hw_id1, GPIOF_DIR_IN, "HW_ID1");
		if (ret < 0) {
			printk("Fail to set hw-id1 pin\n");
			return ret;
		}
	}

	hw_id2 = of_get_named_gpio(dev->of_node, "hw-id2", 0);
	if (!gpio_is_valid(hw_id2)) {
		printk("No hw-id2 pin available in board-info\n");
		return -ENODEV;
	} else {
		ret = devm_gpio_request_one(dev, hw_id2, GPIOF_DIR_IN, "HW_ID2");
		if (ret < 0) {
			printk("Fail to set hw-id2 pin\n");
			return ret;
		}
	}

	pid_id0 = of_get_named_gpio(dev->of_node, "pid-id0", 0);
	if (!gpio_is_valid(pid_id0)) {
		printk("No pid-id0 pin available in board-info\n");
		return -ENODEV;
	} else {
		ret = devm_gpio_request_one(dev, pid_id0, GPIOF_DIR_IN, "PID_ID0");
		if (ret < 0) {
			printk("Fail to set pid-id0 pin\n");
			return ret;
		}
	}

	pid_id1 = of_get_named_gpio(dev->of_node, "pid-id1", 0);
	if (!gpio_is_valid(pid_id1)) {
		printk("No pid-id1 pin available in board-info\n");
		return -ENODEV;
	} else {
		ret = devm_gpio_request_one(dev, pid_id1, GPIOF_DIR_IN, "PID_ID1");
		if (ret < 0) {
			printk("Fail to set pid-id1 pin\n");
			return ret;
		}
	}

	pid_id2 = of_get_named_gpio(dev->of_node, "pid-id2", 0);
	if (!gpio_is_valid(pid_id2)) {
		printk("No pid-id2 pin available in board-info\n");
		return -ENODEV;
	} else {
		ret = devm_gpio_request_one(dev, pid_id2, GPIOF_DIR_IN, "PID_ID2");
		if (ret < 0) {
			printk("Fail to set pid-id2 pin\n");
			return ret;
		}
	}

	ddr_id0 = of_get_named_gpio(dev->of_node, "ddr-id0", 0);
	if (!gpio_is_valid(ddr_id0)) {
		printk("No ddr-id0 pin available in board-info\n");
		return -ENODEV;
	} else {
		ret = devm_gpio_request_one(dev, ddr_id0, GPIOF_DIR_IN, "DDR_ID0");
		if (ret < 0) {
			printk("Fail to set ddr-id0 pin\n");
			return ret;
		}
	}

	ddr_id1 = of_get_named_gpio(dev->of_node, "ddr-id1", 0);
	if (!gpio_is_valid(ddr_id1)) {
		printk("No ddr-id1 pin available in board-info\n");
		return -ENODEV;
	} else {
		ret = devm_gpio_request_one(dev, ddr_id1, GPIOF_DIR_IN, "DDR_ID1");
		if (ret < 0) {
			printk("Fail to set ddr-id1 pin\n");
			return ret;
		}
	}

	ddr_id2 = of_get_named_gpio(dev->of_node, "ddr-id2", 0);
	if (!gpio_is_valid(ddr_id2)) {
		printk("No ddr-id2 pin available in board-info\n");
		return -ENODEV;
	} else {
		ret = devm_gpio_request_one(dev, ddr_id2, GPIOF_DIR_IN, "DDR_ID2");
		if (ret < 0) {
			printk("Fail to set ddr-id2 pin\n");
			return ret;
		}
	}

	file = proc_create("boardver", 0444, NULL, &boardver_ops);
	if (!file)
		return -ENOMEM;

	file = proc_create("boardinfo", 0444, NULL, &boardinfo_ops);
	if (!file)
		return -ENOMEM;

	file = proc_create("boardid", 0444, NULL, &boardid_ops);
	if (!file)
		return -ENOMEM;

	file = proc_create("projectid", 0444, NULL, &projectid_ops);
	if (!file)
		return -ENOMEM;

	file = proc_create("ddr", 0444, NULL, &ddr_ops);
	if (!file)
		return -ENOMEM;

	return 0;
}

static int board_info_remove(struct platform_device *pdev)
{
	gpio_free(hw_id0);
	gpio_free(hw_id1);
	gpio_free(hw_id2);

	gpio_free(pid_id0);
	gpio_free(pid_id1);
	gpio_free(pid_id2);

	gpio_free(ddr_id0);
	gpio_free(ddr_id1);
	gpio_free(ddr_id2);

	return 0;
}

static struct platform_driver boardinfo_driver = {
	.probe          = board_info_probe,
	.remove		= board_info_remove,
	.driver = {
		.name   = "board-info",
#ifdef CONFIG_OF_GPIO
		.of_match_table = of_match_ptr(of_board_info_match),
#endif
	},
};

module_platform_driver(boardinfo_driver);
