#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/init.h>
#include <linux/interrupt.h>


#define DOOR_COUNT 2
struct gpio_door {
	struct gpio_desc *gpiod;
	int irq;
};
static struct gpio_door *gpio_doors;
static int door_major = 0;
static struct class *door_class;

static irqreturn_t door_irq_request(int irq, void *dev_id)
{
	struct gpio_door *door = dev_id;
	int val;
	val = gpiod_get_value(door->gpiod);

	printk(KERN_WARNING"key %d\n", val);
	return IRQ_HANDLED;
}

static void clean_gpios(void)
{
    if(!IS_ERR(gpio_doors[0].gpiod)) {
        printk("%s gpiod_put door0 \n", __FUNCTION__);
        gpiod_put(gpio_doors[0].gpiod);
    }
    if(!IS_ERR(gpio_doors[1].gpiod)) {
        printk("%s gpiod_put door1 \n", __FUNCTION__);
        gpiod_put(gpio_doors[1].gpiod);
    }
}
static int door_drv_open(struct inode *node, struct file *file)
{
	return 0;
}

static ssize_t door_drv_read(struct file *file, char __user *buf, size_t size, loff_t *offset)
{
	return 0;
}

static ssize_t door_drv_write(struct file *file, const char __user *buf, size_t size, loff_t *offset)
{
	return 0;
}

static int door_drv_close(struct inode *node, struct file *file)
{
	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
	return 0;
}

static struct file_operations doors_ops = {
	.owner = THIS_MODULE,
	.open = door_drv_open,
	.read = door_drv_read,
	.write = door_drv_write,
	.release = door_drv_close,
};

static int create_door_chrdev(void)
{
	door_major = register_chrdev(0, "door", &doors_ops);
	if(door_major < 0) {
		printk(KERN_ERR"door: couldn't get a major number\n");
		return -1;
	}
	
	door_class = class_create(THIS_MODULE, "door_class");
	if(IS_ERR(door_class)) {
		printk(KERN_ERR"door class: create failed\n");
		unregister_chrdev(door_major, "door");
		return -1;
	}

	device_create(door_class, NULL, MKDEV(door_major, 0), NULL, "doors");

	return 0;
}

static void delete_door_chrdev(void)
{
	if(!IS_ERR(door_class)) {
		device_destroy(door_class, MKDEV(door_major, 0));
		class_destroy(door_class);
	}

	if(door_major>=0) {
		unregister_chrdev(door_major, "door");
	}
}

static int doors_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	int i, err = -1;
	gpio_doors = (struct gpio_door *)kzalloc(sizeof(struct gpio_door)*2, GFP_KERNEL);

	printk(KERN_WARNING"doors driver probe \n");
	for(i=0;i<DOOR_COUNT;i++) {
		char gpio_name[10] = "door";
		sprintf(gpio_name,"door%d", i);
		printk(KERN_WARNING"doors gpio_name %s \n", gpio_name);
		gpio_doors[i].gpiod = gpiod_get(&pdev->dev, gpio_name, 0);
		if(IS_ERR(gpio_doors[i].gpiod)) {
			dev_err(&pdev->dev, "Failed to get door0 for user\n");
			err = PTR_ERR(gpio_doors[i].gpiod);
			clean_gpios();
			return err;
		}
	}

	if(node) {
		for(i=0;i<DOOR_COUNT;i++) {
			gpio_doors[i].irq= of_irq_get(node, i);
			printk(KERN_WARNING"door%d driver get irq num:%d\n", i, gpio_doors[i].irq);
			err = request_irq(gpio_doors[i].irq, door_irq_request, 
					IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
					"door-irq", &gpio_doors[i]); 
			if(err) {
				dev_err(&pdev->dev, "Failed to request gpio %d irq for user\n", i);
			}
		}
	}

	err = create_door_chrdev();

	return err;
}

static int doors_remove(struct platform_device *pdev)
{
	int i;
	printk(KERN_WARNING"doors driver remove \n");

	delete_door_chrdev();

	for(i=0;i<DOOR_COUNT;i++) {
		free_irq(gpio_doors[i].irq, &gpio_doors[i]);
	}
	clean_gpios();

	kfree(gpio_doors);
	return 0;
}

static struct of_device_id doors_id[] = {
	{ .compatible = "sprintray,doors" },
	{},
};

static struct platform_driver doors_driver = {
	.probe = doors_probe,
	.remove = doors_remove,
	.driver = {
		.name = "doors_driver",
		.of_match_table = doors_id, 
	},
};

static int door_init(void)
{
	int err;
	err = platform_driver_register(&doors_driver);
	printk(KERN_WARNING"doors driver init\n");
	return 0;
}

static void door_exit(void)
{
	platform_driver_unregister(&doors_driver);
	printk(KERN_WARNING"doors driver exit\n");
}

module_init(door_init);
module_exit(door_exit);
MODULE_LICENSE("GPL");
