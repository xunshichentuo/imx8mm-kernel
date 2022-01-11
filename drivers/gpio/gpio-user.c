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

static int major = 0;
static struct class *user_class;
struct gpio_desc *user_gpio;

static int user_drv_open(struct inode *node, struct file *file)
{
	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
	gpiod_direction_output(user_gpio, 0);
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

	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
	err = copy_from_user(&status, buf, 1);
	printk("write status:%x \n", status);

	gpiod_set_value(user_gpio, status);
	
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

static int gpio_test_probe(struct platform_device *pdev)
{
	printk("%s %s\n", __FILE__, __FUNCTION__);
	user_gpio = gpiod_get(&pdev->dev, "user", 0);
	if(IS_ERR(user_gpio)) {
		dev_err(&pdev->dev, "Failed to get GPIO for user\n");
		return PTR_ERR(user_gpio);
	}

	major = register_chrdev(0, "user_gpio", &user_drv);

	user_class = class_create(THIS_MODULE, "user_class");
	if(IS_ERR(user_class)) {
		unregister_chrdev(major, "user_gpio");
		gpiod_put(user_gpio);
		printk(KERN_WARNING "class create failed\n");
		return PTR_ERR(user_class);
	}

	device_create(user_class, NULL, MKDEV(major, 0), NULL, "user_gpio%d", 0);

	return 0;
}

static int gpio_test_remove(struct platform_device *pdev)
{
	printk("%s %s\n", __FILE__, __FUNCTION__);
	device_destroy(user_class, MKDEV(major, 0));
	class_destroy(user_class);
	unregister_chrdev(major, "user_gpio");
	gpiod_put(user_gpio);
	return 0;
}

static const struct of_device_id gpio_test_of[] = {
	{ .compatible = "user,gpio" },
	{ },
};

static struct platform_driver gpio_test_driver = {
	.probe = gpio_test_probe,
	.remove = gpio_test_remove,
	.driver = {
		.name = "gpio-test",
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
