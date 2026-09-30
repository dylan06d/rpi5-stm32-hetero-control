#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/gpio/consumer.h>
#include <linux/platform_device.h>
#include <linux/of.h>

#define DEVICE_NAME "relay"
#define CLASS_NAME "custom_relay_class"

#define RELAY_MAGIC 'R'
#define RELAY_CMD_OFF _IO(RELAY_MAGIC, 0)
#define RELAY_CMD_ON _IO(RELAY_MAGIC, 1)

struct relay_dev {
    dev_t dev_num;
    struct cdev cdev;
    struct class *class;
    struct device *device;
    struct gpio_desc *gpio_desc;
};

static struct relay_dev g_relay;

static int relay_open(struct inode *inode, struct file *file)
{
    pr_info("relay_driver: device opened\n");
    return 0;
}


static int relay_release(struct inode *inode, struct file *file)
{
    pr_info("relay_driver: device closed\n");
    return 0;
}


static long relay_ioctl (struct file *file, unsigned int cmd, unsigned long arg)
{
    switch (cmd)
    {
        case RELAY_CMD_ON:
            gpiod_set_value(g_relay.gpio_desc, 1);
            pr_info("relay_driver: Relay ON(GPIO High)\n");
            break;
        case RELAY_CMD_OFF:
            gpiod_set_value(g_relay.gpio_desc, 0);
            pr_info("relay_driver: Relay OFF(GPIO Low)\n");
            break;
        default:
            pr_err("relay_driver: Invalid IOCTL command\n");
            return -EINVAL;
    }
    return 0;
}

static struct file_operations relay_fops = {
    .owner = THIS_MODULE,
    .open = relay_open,
    .release = relay_release,
    .unlocked_ioctl = relay_ioctl,
};

static int relay_probe(struct platform_device *pdev)
{
    int ret;
    struct device *dev = &pdev->dev;

    dev_info(dev, "probing device ...\n");

    g_relay.gpio_desc = devm_gpiod_get(dev, NULL, GPIOD_OUT_LOW);
    if (IS_ERR(g_relay.gpio_desc))
    {
        dev_err(dev, "failed to get gpio descriptor form DTS\n");
        return PTR_ERR(g_relay.gpio_desc);
    }

    ret = alloc_chrdev_region(&g_relay.dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0)
    {
        dev_err(dev, "failed to allocate chrdev region\n");
        return ret;
    }

    cdev_init(&g_relay.cdev, &relay_fops);
    g_relay.cdev.owner = THIS_MODULE;
    ret = cdev_add(&g_relay.cdev, g_relay.dev_num, 1);
    if (ret < 0)
    {
        dev_err(dev, "failed to add cdev\n");
        goto unregister_chrdev;
    }

    g_relay.class = class_create(CLASS_NAME);
    if (IS_ERR(g_relay.class))
    {
        dev_err(dev, "failed to craete device class\n");
        ret = PTR_ERR(g_relay.device);
        goto del_cdev;
    }

    g_relay.device = device_create(g_relay.class, NULL, g_relay.dev_num, NULL, DEVICE_NAME);
    if (IS_ERR(g_relay.device))
    {
        dev_err(dev, "failed to carete divice node /dev/%s\n", DEVICE_NAME);
        ret = PTR_ERR(g_relay.device);
        goto destroy_class;
    }

    dev_info(dev, "device /dev/%s created successfully!\n", DEVICE_NAME);
    return 0;

unregister_chrdev:
    unregister_chrdev_region(g_relay.dev_num, 1);
    return ret;

del_cdev:
    cdev_del(&g_relay.cdev);
    return ret;

destroy_class:
    class_destroy(g_relay.class);
    return ret;
}

static void relay_remove(struct platform_device *pdev)
{
    gpiod_set_value(g_relay.gpio_desc, 0);
    device_destroy(g_relay.class, g_relay.dev_num);
    class_destroy(g_relay.class);
    cdev_del(&g_relay.cdev);
    unregister_chrdev_region(g_relay.dev_num, 1);

    pr_info("relay_driver: driver removed successfully\n");
}

static const struct of_device_id relay_of_match[] = {
    { .compatible = "custom,5v-relay", },
    { }
};

MODULE_DEVICE_TABLE(of, relay_of_match);

static struct platform_driver relay_paltform_driver = {
    .probe = relay_probe,
    .remove = relay_remove,
    .driver = {
        .name = "custom_relay_driver",
        .of_match_table = relay_of_match,
    },
};

module_platform_driver(relay_paltform_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("dylan");
MODULE_DESCRIPTION("Linux Character driver for 5V relay with DTS");
