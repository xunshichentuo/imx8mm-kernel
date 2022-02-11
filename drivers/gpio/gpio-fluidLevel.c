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

static irqreturn_t fluid_level_irq_request(int irq, void *dev_id)
{
	struct fluid_level_data *fluid_level = dev_id;
	int val;
	val = gpiod_get_value(fluid_level->desc);
	printk(KERN_WARNING"fluid level %d %d \n", fluid_level->index, val);

	return IRQ_HANDLED;
}

static int fluid_level_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	int i = 0, err, count = 0;
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
		if(node) {
			fluid_level_datas[i].irq = of_irq_get(node, i);
			if(fluid_level_datas[i].irq <= 0) {
				dev_err(&pdev->dev, 
						"Failed to get fluid level %d irq error:%d\n", 
						i, fluid_level_datas[i].irq);
				break;
			}
			err = request_irq(fluid_level_datas[i].irq, fluid_level_irq_request,
					IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
					"fluid-level-irq", &fluid_level_datas[i]);
			if(err) {
				dev_err(&pdev->dev, 
						"Failed to reuqest fluid level %d irq for driver\n", i);
			}
			printk(KERN_WARNING"fluid level %d driver get irq num:%d \n", 
					i, fluid_level_datas[i].irq);
		}
	}

	return 0;
}

static int fluid_level_remove(struct platform_device *pdev)
{
	int i = 0;
	
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
