#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/major.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>
#include <linux/of.h>

static int major = -1;
static struct class *user_class = NULL;
struct gpio_desc *pump0_gpio = NULL;
struct gpio_desc *pump1_gpio = NULL;
struct gpio_desc *pump2_gpio = NULL;
struct gpio_desc *pump3_gpio = NULL;

static int user_drv_open(struct inode *node, struct file *file)
{
	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
	gpiod_direction_output(pump0_gpio, 0);
	gpiod_direction_output(pump1_gpio, 0);
	gpiod_direction_output(pump2_gpio, 0);
	gpiod_direction_output(pump3_gpio, 0);
	return 0;
}

static ssize_t user_drv_read(struct file *file, char __user *buf, size_t size, loff_t *offset)
{
	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
	return 0;
}

static ssize_t user_drv_write(struct file *file, const char __user *buf, size_t size, loff_t *offset)
{
	int err;
	char status;
	struct inode *inode = file_inode(file);
	int minor = iminor(inode);

	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
	err = copy_from_user(&status, buf, 1);
	printk("write status:%x \n", status);

	switch(minor) {
		case 0:
			gpiod_set_value(pump0_gpio, status);
			break;
		case 1:
			gpiod_set_value(pump1_gpio, status);
			break;
		case 2:
			gpiod_set_value(pump2_gpio, status);
			break;
		case 3:
			gpiod_set_value(pump3_gpio, status);
			break;
		default:
			printk("%s error minor number:%d\n", __FUNCTION__, minor);
			break;
	}
	
	return 1;
}

static int user_drv_close(struct inode *node, struct file *file)
{
	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
	return 0;
}

static struct file_operations user_drv = {
	.owner = THIS_MODULE,
	.open = user_drv_open,
	.read = user_drv_read,
	.write = user_drv_write,
	.release = user_drv_close,
};

static bool init_pumps_gpio(struct platform_device *pdev)
{
	pump0_gpio = gpiod_get(&pdev->dev, "pump0", 0);
	if(IS_ERR(pump0_gpio)) {
		dev_err(&pdev->dev, "Failed to get GPIO 0 for user\n");
		return PTR_ERR(pump0_gpio);
	}

	pump1_gpio = gpiod_get(&pdev->dev, "pump1", 0);
	if(IS_ERR(pump1_gpio)) {
		dev_err(&pdev->dev, "Failed to get GPIO 1 for user\n");
		return PTR_ERR(pump1_gpio);
	}

	pump2_gpio = gpiod_get(&pdev->dev, "pump2", 0);
	if(IS_ERR(pump2_gpio)) {
		dev_err(&pdev->dev, "Failed to get GPIO 2 for user\n");
		return PTR_ERR(pump2_gpio);
	}

	pump3_gpio = gpiod_get(&pdev->dev, "pump3", 0);
	if(IS_ERR(pump3_gpio)) {
		dev_err(&pdev->dev, "Failed to get GPIO 3 for user:%ld\n", PTR_ERR(pump3_gpio));
		return PTR_ERR(pump3_gpio);
	}

	return 0;
}

static int gpio_test_probe(struct platform_device *pdev)
{
	int i = 0;
	int ret = 0;
	printk("%s %s\n", __FILE__, __FUNCTION__);
	ret = init_pumps_gpio(pdev);
	if(ret != 0) {
		return ret;
	}

	major = register_chrdev(0, "pumps", &user_drv);

	user_class = class_create(THIS_MODULE, "pumps_class");
	if(IS_ERR(user_class)) {
		unregister_chrdev(major, "pumps");
		gpiod_put(pump0_gpio);
		gpiod_put(pump1_gpio);
		gpiod_put(pump2_gpio);
		gpiod_put(pump3_gpio);
		printk(KERN_WARNING "class create failed\n");
		return PTR_ERR(user_class);
	}

	for(i=0;i<4;i++)
		device_create(user_class, NULL, MKDEV(major, i), NULL, "pump%d", i);

	return 0;
}

static int gpio_test_remove(struct platform_device *pdev)
{
	int i=0;
	printk("%s %s\n", __FILE__, __FUNCTION__);
	if(!IS_ERR(user_class)) {
		for(i=0;i<4;i++)
			device_destroy(user_class, MKDEV(major, i));
		class_destroy(user_class);
	}
	if(major>=0) {
		unregister_chrdev(major, "pumps");
	}

	if(!IS_ERR(pump0_gpio)) {
		printk("%s gpiod_put pump0 \n", __FUNCTION__);
		gpiod_put(pump0_gpio);
	}

	if(!IS_ERR(pump1_gpio)) {
		printk("%s gpiod_put pump1 \n", __FUNCTION__);
		gpiod_put(pump1_gpio);
	}

	if(!IS_ERR(pump2_gpio)) {
		printk("%s gpiod_put pump2 \n", __FUNCTION__);
		gpiod_put(pump2_gpio);
	}

	if(!IS_ERR(pump3_gpio)) {
		printk("%s gpiod_put pump3 \n", __FUNCTION__);
		gpiod_put(pump3_gpio);
	}

	return 0;
}

static const struct of_device_id gpio_test_of[] = {
	{ .compatible = "sprintray,pumps" },
	{ },
};

static struct platform_driver gpio_test_driver = {
	.probe = gpio_test_probe,
	.remove = gpio_test_remove,
	.driver = {
		.name = "gpio-pumps",
		.of_match_table = gpio_test_of,
	},
};

static int gpio_test_init(void)
{
	int err = 0;
	printk("%s %s\n", __FILE__, __FUNCTION__);
	err = platform_driver_register(&gpio_test_driver);
	return 0;
}

static void gpio_test_exit(void)
{
	printk("%s %s\n", __FILE__, __FUNCTION__);
	platform_driver_unregister(&gpio_test_driver);
}
module_init(gpio_test_init);
module_exit(gpio_test_exit);
MODULE_LICENSE("GPL");
