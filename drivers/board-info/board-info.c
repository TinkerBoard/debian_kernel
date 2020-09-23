/* NOTICE:
 * This file is for asus project id pins,
 */

#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/module.h>

//#include <linux/rockchip/iomap.h>
#include <linux/rockchip/grf.h>


/* Project id (GPIO2_A3, GPIO2_A2, GPIO2_A1)
 * Ram id (GPIO2_B6, GPIO2_B5, GPIO2_B4)
 * PCB id (GPIO2_B2, GPIO2_B1 ,GPIO2_B0)
 */
#define GPIO2_A1 57
#define GPIO2_A2 58
#define GPIO2_A3 59
#define GPIO2_B0 64
#define GPIO2_B1 65
#define GPIO2_B2 66
#define GPIO2_B4 68
#define GPIO2_B5 69
#define GPIO2_B6 70

/* board hardware parameter*/
int project_id_0, project_id_1, project_id_2;
int ram_id_0, ram_id_1, ram_id_2;
int pcb_id_0, pcb_id_1, pcb_id_2;

void *regs;
int err;

char *board_type = "Unknown Board type";
char *ram_size = "Unknown RAM size";
char *pcb = "Unknown PCB";

static struct proc_dir_entry *project_id_proc_file;

static void read_project_id(void)
{
	err = gpio_request(GPIO2_A1, "project_id_0");
	if (err < 0) {
		printk("%s: gpio_request failed for GPIO2_A1 %d\n", __func__, GPIO2_A1);
		return ;
	} else {
		gpio_direction_input(GPIO2_A1);
		project_id_0 = gpio_get_value(GPIO2_A1);
	}
	err = gpio_request(GPIO2_A2, "project_id_1");
	if (err < 0) {
		printk("%s: gpio_request failed for GPIO2_A2 %d\n", __func__, GPIO2_A2);
		return ;
	} else {
		gpio_direction_input(GPIO2_A2);
		project_id_1 = gpio_get_value(GPIO2_A2);
	}
	err = gpio_request(GPIO2_A3, "project_id_2");
	if (err < 0) {
		printk("%s: gpio_request failed for GPIO2_A3 %d\n", __func__, GPIO2_A3);
		return ;
	} else {
		gpio_direction_input(GPIO2_A3);
		project_id_2 = gpio_get_value(GPIO2_A3);
	}
	gpio_free(GPIO2_A1);
	gpio_free(GPIO2_A2);
	gpio_free(GPIO2_A3);

	printk("project_id_2:0x%x, project_id_1:0x%x, project_id_0:0x%x \n",
		project_id_2, project_id_1, project_id_0);

	if (project_id_2 == 0 && project_id_1 == 0 && project_id_0 == 0)
		board_type = "Tinker Board S";
	else if (project_id_2 == 0 && project_id_1 == 0 && project_id_0 == 1)
		board_type = "Tinker Board S/HV";
	else if (project_id_2 == 1 && project_id_1 == 0 && project_id_0 == 0)
		board_type = "Tinker R/BR";
	else if (project_id_2 == 1 && project_id_1 == 1 && project_id_0 == 1)
		board_type = "Tinker Board";
	else
		board_type = "unknown board name";
}

static void read_ram_id(void)
{
	err = gpio_request(GPIO2_B4, "ram_id_0");
	if (err < 0) {
		printk("%s: gpio_request failed for GPIO2_B4 %d\n", __func__, GPIO2_B4);
		return ;
	} else {
		gpio_direction_input(GPIO2_B4);
		ram_id_0 = gpio_get_value(GPIO2_B4);
	}
	err = gpio_request(GPIO2_B5, "ram_id_1");
	if (err < 0) {
		printk("%s: gpio_request failed for GPIO2_B5 %d\n", __func__, GPIO2_B5);
		return ;
	} else {
		gpio_direction_input(GPIO2_B5);
		ram_id_1 = gpio_get_value(GPIO2_B5);
	}
	err = gpio_request(GPIO2_B6, "ram_id_2");
	if (err < 0) {
		printk("%s: gpio_request failed for GPIO2_B6 %d\n", __func__, GPIO2_B6);
		return ;
	} else {
		gpio_direction_input(GPIO2_B6);
		ram_id_2 = gpio_get_value(GPIO2_B6);
	}
	gpio_free(GPIO2_B4);
	gpio_free(GPIO2_B5);
	gpio_free(GPIO2_B6);

	printk("ram_id_2:0x%x, ram_id_1:0x%x, ram_id_0:0x%x \n",
		ram_id_2, ram_id_1, ram_id_0);

	if (ram_id_2 == 0 && ram_id_1 == 0 && ram_id_0 == 0)
		ram_size = "4 GB";
	else if (ram_id_2 == 0 && ram_id_1 == 1 && ram_id_0 == 0)
		ram_size = "2 GB";
	else if (ram_id_2 == 1 && ram_id_1 == 0 && ram_id_0 == 0)
		ram_size = "1 GB";
	else
		ram_size = "unknown ram";
}

static void read_pcb_id(void)
{
	err = gpio_request(GPIO2_B0, "pcb_id_0");
	if (err < 0) {
		printk("%s: gpio_request failed for GPIO2_B0 %d\n", __func__, GPIO2_B0);
		return ;
	} else {
		gpio_direction_input(GPIO2_B0);
		pcb_id_0 = gpio_get_value(GPIO2_B0);
	}
	err = gpio_request(GPIO2_B1, "pcb_id_1");
	if (err < 0) {
		printk("%s: gpio_request failed for GPIO2_B1 %d\n", __func__, GPIO2_B1);
		return ;
	} else {
		gpio_direction_input(GPIO2_B1);
		pcb_id_1 = gpio_get_value(GPIO2_B1);
	}
	err = gpio_request(GPIO2_B2, "pcb_id_2");
	if (err < 0) {
		printk("%s: gpio_request failed for GPIO2_B2 %d\n", __func__, GPIO2_B2);
		return ;
	} else {
		gpio_direction_input(GPIO2_B2);
		pcb_id_2 = gpio_get_value(GPIO2_B2);
	}
	gpio_free(GPIO2_B0);
	gpio_free(GPIO2_B1);
	gpio_free(GPIO2_B2);

	printk("pcb_id_2:0x%x, pcb_id_1:0x%x, pcb_id_0:0x%x \n",
		pcb_id_2, pcb_id_1, pcb_id_0);

	if (pcb_id_2 == 0 && pcb_id_1 == 0 && pcb_id_0 == 0)
		pcb = "SR";
	else if (pcb_id_2 == 0 && pcb_id_1 == 0 && pcb_id_0 == 1)
		pcb = "ER";
	else if (pcb_id_2 == 0 && pcb_id_1 == 1 && pcb_id_0 == 0)
		pcb = "PR";
	else
		pcb = "unknown pcb";
}

static int board_info_proc_read(struct seq_file *buf, void *v)
{
	/* Board info display */
	seq_printf(buf, "%s\n", board_type);
	printk("[board_info] %s board_type=\'%s\' ram_size=\'%s' pcb=\'%s\'\n",
		__func__, board_type, ram_size, pcb);
	return 0;
}

static int project_id_proc_open(struct inode *inode, struct  file *file)
{
	return single_open(file, board_info_proc_read, NULL);
}


static struct file_operations project_id_proc_ops = {
	.open = project_id_proc_open,
	.read = seq_read,
	.release = single_release,
};

static void create_project_id_proc_file(void)
{
	project_id_proc_file = proc_create("board_info", 0444, NULL,
						&project_id_proc_ops);
	if (project_id_proc_file) {
		printk("[board_info] create Board_info_proc_file sucessed!\n");
	} else {
		printk("[board_info] create Board_info_proc_file failed!\n");
	}

	/* Pull up GPIO2 A1 A2 A3*/
	regs = ioremap(0xff770000, 64*1024);
	if (regs == NULL) {
		printk("[board_info] ioremap failed");
		return ;
	}
	writel((readl(regs + RK3288_GRF_GPIO2A_P) & ~(0x3f << 18) & ~(0x3f << 2))
		| (0x3f << 18) | (0x15 << 2), regs + RK3288_GRF_GPIO2A_P);
	// printk("[board_info] %x\n", readl(regs + RK3288_GRF_GPIO2A_P));

	iounmap(regs);

	/* Read GPIO */
	read_project_id();
	read_ram_id();
	read_pcb_id();
}

static int __init proc_asusPRJ_init(void)
{
	create_project_id_proc_file();
	return 0;
}

module_init(proc_asusPRJ_init);
