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

	return 0;
}

static int doors_remove(struct platform_device *pdev)
{
	int i;
	printk(KERN_WARNING"doors driver remove \n");

	clean_gpios();

	for(i=0;i<DOOR_COUNT;i++) {
		free_irq(gpio_doors[i].irq, &gpio_doors[i]);
	}

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
