#include <linux/module.h>
#include <linux/kernel.h>


static int door_init(void)
{
	return 0;
}

static void door_exit(void)
{

}

module_init(door_init);
module_exit(door_exit);
MODULE_LINCENSE("GPL");
