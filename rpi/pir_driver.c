#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/gpio/consumer.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/interrupt.h>
#include <linux/poll.h>
#include <linux/uaccess.h>

#define DEVICE_NAME "pir"
#define CLASS_NAME "custom_pir_class"

struct pir_dev
{
    dev_t dev_num;
    struct cdev cdev;
    struct class *class;
    struct device *device;
    struct gpio_desc *gpio_desc;
    int irq_num;

    wait_queue_head_t wq;
    atomic_t event_flag;
};

static struct pir_dev dev;

// 中断处理函数
static irqreturn_t pir_irq_handler(int irq, void *dev_id)
{
    atomic_set(&dev.event_flag, 1);
    wake_up_interruptible(&dev.wq);

    pr_info("pir_driver: Motion detected! Interrupt triggered.\n");
    return IRQ_HANDLED;
}

static int pir_open(struct inode *inode, struct file *file)
{
    pr_info("pir_driver: device opened\n");
    return 0;
}

static int pir_release(struct inode *inode, struct file *file)
{
    pr_info("pir_driver: device closed\n");
    return 0;
}

static ssize_t pir_read(struct file *file, char __user *buffer, size_t len, loff_t *loffset)
{
    int ret;
    int event;

    if (len < sizeof(int))
        return -EINVAL;

    // 非阻塞模式下直接返回
    if ((file->f_flags & O_NONBLOCK) && atomic_read(&dev.event_flag) == 0)
        return -EAGAIN;

    // 阻塞模式：如果未触发事件则休眠
    ret = wait_event_interruptible(dev.wq, atomic_read(&dev.event_flag) != 0);
    if (ret)
        return ret;

    event = 1;
    atomic_set(&dev.event_flag, 0);

    if (copy_to_user(buffer, &event, sizeof(event)))
        return -EFAULT;

    return sizeof(event);
}

// 实现非阻塞与阻塞I/O多路复用
static __poll_t pir_poll(struct file *file, struct poll_table_struct *wait)
{
    __poll_t mask = 0;

    // 将等待队列挂在到poll机制中
    poll_wait(file, &dev.wq, wait);

    if (atomic_read(&dev.event_flag) != 0)
    {
        mask |= EPOLLIN | EPOLLRDNORM;
    }

    return mask;
}

static const struct file_operations pir_fops = {
    .open = pir_open,
    .release = pir_release,
    .read = pir_read,
    .poll = pir_poll,
};

static int pir_probe(struct platform_device *pdev)
{
    int ret;
    struct device *device = &pdev->dev;
    dev_info(device, "Probind device ...\n");

    init_waitqueue_head(&dev.wq);
    atomic_set(&dev.event_flag, 0);

    dev.gpio_desc = devm_gpiod_get(device, NULL, GPIOD_IN);
    if (IS_ERR(dev.gpio_desc))
    {
        dev_err(device, "failed to get gpio descroptor\n");
        ret = PTR_ERR(dev.gpio_desc);
        return ret;
    }

    dev.irq_num = gpiod_to_irq(dev.gpio_desc);
    if (dev.irq_num < 0)
    {
        dev_err(device, "failed to get irq number\n");
        return dev.irq_num;
    }

    dev_info(device, "assigned IRQ number %d\n", dev.irq_num);

    ret = devm_request_threaded_irq(device, dev.irq_num, pir_irq_handler, NULL, IRQF_TRIGGER_RISING | IRQF_ONESHOT, "pir_motion_irq", &dev);
    if (ret)
    {
        dev_err(device, "failed to request irq\n");
        return ret;
    }

    ret = alloc_chrdev_region(&dev.dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0)
    {
        return ret;
    }

    cdev_init(&dev.cdev, &pir_fops);
    dev.cdev.owner = THIS_MODULE;
    ret = cdev_add(&dev.cdev, dev.dev_num, 1);
    if (ret < 0)
        goto unregister_chrdev;

    dev.class = class_create(CLASS_NAME);
    if (IS_ERR(dev.class))
    {
        ret = PTR_ERR(dev.class);
        goto del_cdev;
    }

    dev.device = device_create(dev.class, NULL, dev.dev_num, NULL, DEVICE_NAME);
    if (IS_ERR(dev.device))
    {
        ret = PTR_ERR(dev.device);
        goto destroy_class;
    }

    dev_info(device, "device /dev/%s created successfully!\n", DEVICE_NAME);
    return 0;

destroy_class:
    class_destroy(dev.class);
    return ret;

del_cdev:
    cdev_del(&dev.cdev);
    return ret;

unregister_chrdev:
    unregister_chrdev_region(dev.dev_num, 1);
    return ret;
}

static void pir_remove(struct platform_device *pdev)
{
    device_destroy(dev.class, dev.dev_num);
    class_destroy(dev.class);
    cdev_del(&dev.cdev);
    unregister_chrdev_region(dev.dev_num, 1);

    dev_info(&pdev->dev, "driver removed successfully\n");
}

static const struct of_device_id pir_of_match[] = {
    { .compatible = "custom,pir-sensor", },
    { }
};
MODULE_DEVICE_TABLE(of, pir_of_match);

static struct platform_driver pir_platform_driver = {
    .probe = pir_probe,
    .remove = pir_remove,
    .driver = {
        .name = "custom_pir_driver",
        .of_match_table = pir_of_match,
    },
};
module_platform_driver(pir_platform_driver);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("dylan");
MODULE_DESCRIPTION("pir driver");
