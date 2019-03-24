#include <linux/version.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/list.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#define MYDEV_NAME "mycdrv"
#define ramdisk_size (size_t) (16 * PAGE_SIZE) 
#define CDRV_IOC_MAGIC 'Z'
#define ASP_CLEAR_BUF _IOW(CDRV_IOC_MAGIC, 1, int)

static int NUM_DEVICES = 3;


struct ASP_mycdrv {
	struct list_head list;
	struct cdev cdev;
	char *ramDisk;
        unsigned long buffer_size;
	struct semaphore sem;
	int devNo;
};


module_param(NUM_DEVICES, int, S_IRUGO);
static unsigned int mycdev_major = 0;
static struct class *class_device = NULL;
LIST_HEAD(DeviceListHead);

int asp_open(struct inode *inode, struct file *filp)
{
        unsigned int mjnum = imajor(inode);
        unsigned int mnnum = iminor(inode);
	struct list_head *pos = NULL;
        struct ASP_mycdrv *device = NULL;

	if (mjnum != mycdev_major || mnnum < 0 || mnnum >= NUM_DEVICES) {
                printk("No device found!!\n");
                return -ENODEV;
        }

	list_for_each(pos, &DeviceListHead) {
		device = list_entry(pos, struct ASP_mycdrv, list);
		if(device->devNo == mnnum) {
			break;
		}
	}

        filp->private_data = device; 

        return 0;
}

int asp_release(struct inode *inode, struct file *filp)
{
        return 0;
}

ssize_t asp_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	struct ASP_mycdrv *dev = (struct ASP_mycdrv *)filp->private_data;
	ssize_t retval = 0;
	unsigned char *tmp_buff = NULL;
	loff_t i;

	if (down_interruptible(&(dev->sem))!=0) {
		pr_err("%s: could not lock during open operation\n", MYDEV_NAME);
	}

	if (*f_pos >= dev->buffer_size) { 
		up(&(dev->sem));
	        return retval;
        }

	tmp_buff = (unsigned char*) kmalloc(count, GFP_KERNEL);
	memset(tmp_buff, 0, count); 	

	for (i = 0; i < count; i++) {
		tmp_buff[i] = dev->ramDisk[*f_pos + i];
	}
	*f_pos += count;

	if (copy_to_user(buf, tmp_buff, count) != 0)
	{
		retval = -EFAULT;
		kfree(tmp_buff);
                up(&(dev->sem));
                return retval;
	}

	retval = count;

        kfree(tmp_buff);
	up(&(dev->sem));
	return retval;
}

ssize_t asp_write(struct file *filp, const char __user *buf, size_t count, 
		loff_t *f_pos)
{
	struct ASP_mycdrv *dev = (struct ASP_mycdrv *)filp->private_data;
	ssize_t retval = 0;
	unsigned char *tmp_buff = NULL;
	loff_t i;

	if (down_interruptible(&(dev->sem))!=0) {
		pr_err("%s: could not lock during open operation\n", MYDEV_NAME);
	}

	if (*f_pos >= dev->buffer_size) {
		retval = -EINVAL;
		up(&(dev->sem));
		return retval;
	} 

	tmp_buff = (unsigned char*) kmalloc(count, GFP_KERNEL);
	memset(tmp_buff, 0, count); 	

	if (copy_from_user(tmp_buff, buf, count) != 0)
	{
		retval = -EFAULT;
		kfree(tmp_buff);
                up(&(dev->sem));
	        return retval;
	}

	for (i = 0; i < count; i++) {
		dev->ramDisk[*f_pos + i] = tmp_buff[i];
	}
	*f_pos += count;

	retval = count;

	kfree(tmp_buff);
	up(&(dev->sem));
	return retval;
}

long asp_ioctl (struct file *filp, unsigned int cmd, unsigned long arg)
{
        struct ASP_mycdrv *dev = (struct ASP_mycdrv *)filp->private_data;
	
	if (cmd != ASP_CLEAR_BUF) {
		pr_err("Invalid command detected\n");
		return -1;
	}

	if (down_interruptible(&(dev->sem))!=0) {
		pr_err("%s: could not lock during open operation\n", MYDEV_NAME);
	}

        memset((volatile void *)dev->ramDisk, 0, dev->buffer_size);  	
        filp->f_pos = 0;						

	up(&(dev->sem));
	return 1;
}

loff_t asp_llseek(struct file *filp, loff_t off, int whence)
{
	struct ASP_mycdrv *dev = (struct ASP_mycdrv *)filp->private_data;
	loff_t upos = 0;       

	if (down_interruptible(&(dev->sem))!=0) {
		pr_err("%s:failed to acquire lock\n", MYDEV_NAME);
	}

	switch(whence) {
		case 0: 
			upos = off;
			break;

		case 1: 
			upos = filp->f_pos + off;
			break;

		case 2: 
			upos = dev->buffer_size + off;
			break;

		default: 
			upos = -EINVAL;
	
	}

        unsigned char* tmp_buffer;
	if (upos > dev->buffer_size) {
                tmp_buffer = (unsigned char*)kmalloc(upos, GFP_KERNEL);
		memset(tmp_buffer, 0, upos);
                memcpy(tmp_buffer, dev->ramDisk, dev->buffer_size);
                kfree(dev->ramDisk);
                dev->ramDisk = tmp_buffer;
                dev->buffer_size = upos;
                
	}

	filp->f_pos = upos;

	up(&(dev->sem));
	return upos;
}

struct file_operations mycdev_fops = {
        .owner =    THIS_MODULE,
        .read =     asp_read,
        .write =    asp_write,
        .open =     asp_open,
        .release =  asp_release,
        .llseek =   asp_llseek,
	.unlocked_ioctl = asp_ioctl,
};



static void asp_cleanup(void)
{
        int i = 0;
        struct list_head *pos = NULL;
        struct ASP_mycdrv *device = NULL;

loop:
	list_for_each(pos, &DeviceListHead) {
		device = list_entry(pos, struct ASP_mycdrv, list);
		device_destroy(class_device, MKDEV(mycdev_major, i));
        	cdev_del(&device->cdev);
        	kfree(device->ramDisk);
		list_del(&(device->list));
                kfree(device);
		i++;
        	goto loop;
        }

	if (class_device)
		class_destroy(class_device);

	unregister_chrdev_region(MKDEV(mycdev_major, 0), NUM_DEVICES);
	return;
}

static int __init my_init(void)
{
        
        int i = 0;
        dev_t device = 0;
        struct ASP_mycdrv *mycdev_device = NULL;
        alloc_chrdev_region(&device, 0, NUM_DEVICES, MYDEV_NAME);  
        mycdev_major = MAJOR(device);
        class_device = class_create(THIS_MODULE, MYDEV_NAME);
        
        
 
	for (i = 0; i < NUM_DEVICES; ++i) 
        {     
		mycdev_device = (struct ASP_mycdrv *)kmalloc(sizeof(struct ASP_mycdrv), GFP_KERNEL);
		memset(mycdev_device, 0, sizeof(struct ASP_mycdrv)); 
		dev_t devno = MKDEV(mycdev_major, i);
	        struct device *device = NULL;
	        mycdev_device->buffer_size = ramdisk_size;
		mycdev_device->devNo = i;
		mycdev_device->ramDisk = NULL;
		sema_init(&(mycdev_device->sem),1);
        	cdev_init(&mycdev_device->cdev, &mycdev_fops);
	        mycdev_device->cdev.owner = THIS_MODULE;
	        mycdev_device->ramDisk = (unsigned char*)kmalloc(mycdev_device->buffer_size, GFP_KERNEL);
		memset(mycdev_device->ramDisk, 0, mycdev_device->buffer_size);
	       	cdev_add(&mycdev_device->cdev, devno, 1);
	        device = device_create(class_device, NULL, devno, NULL, MYDEV_NAME "%d", i);
		INIT_LIST_HEAD(&(mycdev_device->list));
		list_add(&(mycdev_device->list), &DeviceListHead);
        }

        return 0;

}

static void __exit my_exit(void)
{
        asp_cleanup();
        return;
}

module_init(my_init);
module_exit(my_exit);

MODULE_AUTHOR("Harshal");
MODULE_LICENSE("GPL v2");
