#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/uaccess.h>
#include <linux/gpio/consumer.h>

struct fluid_level_data {
	int index;
	struct gpio_desc *desc;
	int irq;
};

static struct fluid_level_data *fluid_level_datas;
static struct gpio_descs *fluid_level_descs;

#define FLUID_LEVEL_MAJOR_NAME "fluid_level_major"
#define FLUID_LEVEL_CLASS_NAME "fluid_level_class"
#define FLUID_LEVEL_DEVICE_NAME "fluid_levels"
static int fluid_level_major = 0;
static struct class *fluid_level_class;

static int g_fluid_level = 0;
static DECLARE_WAIT_QUEUE_HEAD(fluid_level_wait);

static irqreturn_t fluid_level_irq_request(int irq, void *dev_id)
{
	struct fluid_level_data *fluid_level = dev_id;
	int val;
	val = gpiod_get_value(fluid_level->desc);

	g_fluid_level = (fluid_level->index<<8)|val;
	printk(KERN_WARNING"fluid level %d %d %x\n", 
			fluid_level->index, val, g_fluid_level);
	wake_up_interruptible(&fluid_level_wait);

	return IRQ_HANDLED;
}

static ssize_t fluid_level_read(struct file *file, char __user *buf, size_t size, loff_t *offset)
{
	int err;
	
	wait_event_interruptible(fluid_level_wait, g_fluid_level);
	err = copy_to_user(buf, &g_fluid_level, 4);
	g_fluid_level = 0;
	if(err != 4) {
		return -1;
	}
	
	return 0;
}

static struct file_operations fluid_level_ops = {
	.owner = THIS_MODULE,
	.read = fluid_level_read,
};

static int create_fluid_level_chrdev(void)
{
	fluid_level_major = register_chrdev(0, FLUID_LEVEL_MAJOR_NAME,
			&fluid_level_ops);
	if(fluid_level_major < 0) {
		printk(KERN_ERR"fluid level: couldn't get a major number\n");
		return -1;
	}

	fluid_level_class = class_create(THIS_MODULE, FLUID_LEVEL_CLASS_NAME);
	if(IS_ERR(fluid_level_class)) {
		printk(KERN_ERR"fluid level class:create failed\n");
		unregister_chrdev(fluid_level_major, FLUID_LEVEL_MAJOR_NAME); 
		return -1;
	}

	device_create(fluid_level_class, NULL, MKDEV(fluid_level_major, 0),
			NULL, FLUID_LEVEL_DEVICE_NAME); 
	return 0;
}

static void delete_fluid_level_chrdev(void)
{
	if(!IS_ERR(fluid_level_class)) {
		device_destroy(fluid_level_class, MKDEV(fluid_level_major, 0));
		class_destroy(fluid_level_class);
	}

	if(fluid_level_major>=0) {
		unregister_chrdev(fluid_level_major, FLUID_LEVEL_MAJOR_NAME);
	}
}

static int request_fluid_level_irq(struct platform_device *pdev, 
		struct device_node *node, struct fluid_level_data *data)
{
	int err = 0;
	if(!node) return -1;

	data->irq = of_irq_get(node, data->index);
	if(data->irq <= 0) {
		dev_err(&pdev->dev, 
				"Failed to get fluid level %d irq error:%d\n", 
				data->index, data->irq);
		return -1;
	}
	err = request_irq(data->irq, fluid_level_irq_request,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
			"fluid-level-irq", data);
	if(err) {
		dev_err(&pdev->dev, 
				"Failed to reuqest fluid level %d irq for driver\n", data->index);
	}
	printk(KERN_WARNING"fluid level %d driver get irq num:%d \n", 
			data->index, data->irq);
	return 0;
}

static int fluid_level_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	int i = 0, count = 0;
	fluid_level_descs = devm_gpiod_get_array(&pdev->dev, 
			"fluidLevel", GPIOD_IN);
	if(IS_ERR(fluid_level_descs)) {
		dev_err(&pdev->dev, "Failed to get fluid levels for driver\n");
		return PTR_ERR(fluid_level_descs);
	}

	count = fluid_level_descs->ndescs;
	dev_warn(&pdev->dev, "fluid level descs ndescs:%d\n", count);
	fluid_level_datas = (struct fluid_level_data*)kzalloc(
			sizeof(struct fluid_level_data)*fluid_level_descs->ndescs, 
			GFP_KERNEL); 
	if(IS_ERR(fluid_level_datas)) {
		dev_err(&pdev->dev, "Failed to create fluid_level_data for driver \n");
		return PTR_ERR(fluid_level_datas);
	}
	
	for(i=0;i<fluid_level_descs->ndescs;i++) {
		fluid_level_datas[i].desc = fluid_level_descs->desc[i];
		fluid_level_datas[i].index = i;
		if(request_fluid_level_irq(pdev, node, &fluid_level_datas[i]) < 0)
			break;
	}

	create_fluid_level_chrdev();

	return 0;
}

static int fluid_level_remove(struct platform_device *pdev)
{
	int i = 0;

	delete_fluid_level_chrdev();
	
	for(i=0;i<fluid_level_descs->ndescs;i++) {
		free_irq(fluid_level_datas[i].irq, &fluid_level_datas[i]);
		printk("%s free fluid level %d irq %d\n", __FUNCTION__, i, 
				fluid_level_datas[i].irq);
	}

	kfree(fluid_level_datas);

	if(!IS_ERR(fluid_level_descs)) {
		devm_gpiod_put_array(&pdev->dev, fluid_level_descs);
		printk("%s devm_gpiod_put_array\n", __FUNCTION__);
	}

	return 0;
}

static struct of_device_id fluid_level_id[] = {
	{ .compatible = "sprintray,fluidLevel" },
	{},
};

static struct platform_driver fluid_level_driver = {
	.probe = fluid_level_probe,
	.remove = fluid_level_remove,
	.driver = {
		.name = "fluid_level_driver",
		.of_match_table = fluid_level_id,
	},
};

static int fluid_level_init(void)
{
	int err;
	err = platform_driver_register(&fluid_level_driver);
	printk(KERN_WARNING"%s\n", __FUNCTION__);
	return 0;
}

static void fluid_level_exit(void)
{
	platform_driver_unregister(&fluid_level_driver);
	printk(KERN_WARNING"%s\n", __FUNCTION__);
}

module_init(fluid_level_init);
module_exit(fluid_level_exit);
MODULE_LICENSE("GPL");
