/*
 * lunix-chrdev.c
 *
 * Implementation of character devices
 * for Lunix:TNG
 *
 * < Your name here >
 *
 */

#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/cdev.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/ioctl.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mmzone.h>
#include <linux/vmalloc.h>
#include <linux/spinlock.h>

#include "lunix.h"
#include "lunix-chrdev.h"
#include "lunix-lookup.h"

/*
 * Global data
 */
struct cdev lunix_chrdev_cdev;

struct lunix_chrdev_state_struct *lunix_states;

/*
 * Just a quick [unlocked] check to see if the cached
 * chrdev state needs to be updated from sensor measurements.
 */
static int lunix_chrdev_state_needs_refresh(struct lunix_chrdev_state_struct *state)
{
	struct lunix_sensor_struct *sensor;
	
	WARN_ON ( !(sensor = state->sensor));
	if (state->buf_timestamp == sensor->msr_data[state->type]->last_update)
		return 0;
	return 1; /* ? */
}

/*
 * Updates the cached state of a character device
 * based on sensor data. Must be called with the
 * character device state lock held.
 */
static int lunix_chrdev_state_update(struct lunix_chrdev_state_struct *state)
{
	struct lunix_sensor_struct *sensor;
	sensor = state->sensor;
	if (!lunix_chrdev_state_needs_refresh(state))
		return -EAGAIN;
		
	uint32_t data;

	/*
	 * Grab the raw data quickly, hold the
	 * spinlock for as little as possible.
	 */
	/* ? */
	/* Why use spinlocks? See LDD3, p. 119 */
	debug("Spinlock on\n");
	spin_lock(&sensor->lock);
	state->buf_timestamp = sensor->msr_data[state->type]->last_update;
	data = sensor->msr_data[state->type]->values[0];
	spin_unlock(&sensor->lock);
	debug("Spinlock off\n");
	
	/*
	 * Any new data available?
	 */
	/* ? */

	/*
	 * Now we can take our time to format them,
	 * holding only the private state semaphore
	 */
	long looked_up;
	/* ? */
	switch (state->type) {
		case BATT: looked_up = lookup_voltage[data]; break;
		case TEMP: looked_up = lookup_temperature[data]; break;
		case LIGHT: looked_up = lookup_light[data]; break;
	}
	int int_part = looked_up / 1000;
	int dec_part = looked_up % 1000;

	state->buf_lim = snprintf(state->buf_data, LUNIX_CHRDEV_BUFSZ, "%d.%d\n", int_part, dec_part);

	debug("leaving\n");
	return 0;
}

/*************************************
 * Implementation of file operations
 * for the Lunix character device
 *************************************/

static int lunix_chrdev_open(struct inode *inode, struct file *filp)
{
	/* Declarations */
	int minor = iminor(inode);
	int type = minor & 7;
	int sensor_index = (minor - type) >> 3;

	struct lunix_chrdev_state_struct *dev = (struct lunix_chrdev_state_struct*) kmalloc(sizeof(struct lunix_chrdev_state_struct), GFP_KERNEL);
	dev->type = type;
	dev->sensor = lunix_sensors + sensor_index;
	dev->buf_lim = 1;
	dev->buf_data[0] = '\0';
	dev->buf_timestamp = 0;
	sema_init(&dev->lock, 1);

	int ret;
	
	debug("entering\n");
	ret = -ENODEV;
	if ((ret = nonseekable_open(inode, filp)) < 0)
		goto out;

	filp->private_data = dev;
	/*
	 * Associate this open file with the relevant sensor based on
	 * the minor number of the device node [/dev/sensor<NO>-<TYPE>]
	 */
	
	/* Allocate a new Lunix character device private state structure */
	/* ? */
out:
	debug("leaving, with ret = %d\n", ret);
	return ret;
}

static int lunix_chrdev_release(struct inode *inode, struct file *filp)
{
	debug("Freeing_resources!\n");
	kfree(filp->private_data);
	return 0;
}

static long lunix_chrdev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	/* Why? */
	return -EINVAL;
}

static ssize_t lunix_chrdev_read(struct file *filp, char __user *usrbuf, size_t cnt, loff_t *f_pos)
{
	ssize_t ret;

	struct lunix_sensor_struct *sensor;
	struct lunix_chrdev_state_struct *state;
	

	state = filp->private_data;
	WARN_ON(!state);
	printk("This sensor is of type: %d\n", state->type);
	
	sensor = state->sensor;
	WARN_ON(!sensor);
	printk("Last Update Cache: %d\n", state->buf_timestamp);
	printk("Last Update Sensor: %d\n", sensor->msr_data[state->type]->last_update);
	printk("Data: %s\n", state->buf_data);

	/* Lock? */
	if (down_interruptible(&state->lock))
		return -ERESTARTSYS;
	
	/*
	 * If the cached character device state needs to be
	 * updated by actual sensor data (i.e. we need to report
	 * on a "fresh" measurement, do so
	 */
	if (*f_pos == 0) {
		while (lunix_chrdev_state_update(state) == -EAGAIN) {
			up(&state->lock); /* release the lock */
			
			/* ? */
			/* The process needs to sleep */
			/* See LDD3, page 153 for a hint */
			
 			if (filp->f_flags & O_NONBLOCK)
 				return -EAGAIN;
 			if (wait_event_interruptible(sensor->wq, lunix_chrdev_state_needs_refresh(state)))
 				return -ERESTARTSYS; /* signal: tell the fs layer to handle it */

 			/* otherwise loop, but first reacquire the lock */
 			if (down_interruptible(&state->lock))
 				return -ERESTARTSYS;	
		}
	}

	/* End of file */
	if (*f_pos == state->buf_lim && false) {
		ret = 0;
		/* Auto-rewind on EOF mode? */
		goto out;
	}

	/* Determine the number of cached bytes to copy to userspace */
	ssize_t size = cnt;
	if (size > state->buf_lim)
		size = state->buf_lim;
	
	if (copy_to_user(usrbuf, state->buf_data, size)) {
		ret = -EFAULT;
		goto out;
	}
	*f_pos += size;
	ret = *f_pos;
	*f_pos = 0;
out:
	/* Unlock? */
	up(&state->lock);
	return ret;
}

static int lunix_chrdev_mmap(struct file *filp, struct vm_area_struct *vma)
{
	return -EINVAL;
}

static struct file_operations lunix_chrdev_fops = 
{
        .owner          = THIS_MODULE,
	.open           = lunix_chrdev_open,
	.release        = lunix_chrdev_release,
	.read           = lunix_chrdev_read,
	.unlocked_ioctl = lunix_chrdev_ioctl,
	.mmap           = lunix_chrdev_mmap
};

static void lunix_setup_cdev (struct cdev *dev, int index) {
	int err, devno = MKDEV(LUNIX_CHRDEV_MAJOR, index);
	cdev_init(dev, &lunix_chrdev_fops);
	(*dev).owner = THIS_MODULE;
	(*dev).ops = &lunix_chrdev_fops;
	err = cdev_add(dev, devno, 1);
	if (err)
		printk(KERN_NOTICE "Error %d adding lunix%d", err, index);
}

int lunix_chrdev_init(void)
{
	/*
	 * Register the character device with the kernel, asking for
	 * a range of minor numbers (number of sensors * 8 measurements / sensor)
	 * beginning with LINUX_CHRDEV_MAJOR:0
	 */
	int ret;
	dev_t dev_no;
	unsigned int lunix_minor_cnt = lunix_sensor_cnt << 3;

	lunix_states = (struct lunix_chrdev_state_struct*) kmalloc(sizeof(struct lunix_chrdev_state_struct)*lunix_minor_cnt, GFP_KERNEL);
	/* Check if no ram */
	
	debug("initializing character device\n");
	cdev_init(&lunix_chrdev_cdev, &lunix_chrdev_fops);
	lunix_chrdev_cdev.owner = THIS_MODULE;
	
	dev_no = MKDEV(LUNIX_CHRDEV_MAJOR, 0);
	/* ? */
	ret = register_chrdev_region(dev_no, lunix_minor_cnt, "lunix");
	if (ret < 0) {
		debug("failed to register region, ret = %d\n", ret);
		goto out;
	}
	int i, j;
	for (i = 0; i < lunix_sensor_cnt; i++) {
		for (j = 0; j < 3; j++) {
			int minor = i * 8 + j;
			debug("Registered cdev with minor: %d\n", minor);
			lunix_setup_cdev(&lunix_chrdev_cdev, minor);
		}
	}
	if (ret < 0) {
		debug("failed to add character device\n");
		goto out_with_chrdev_region;
	}
	debug("completed successfully\n");
	return 0;

out_with_chrdev_region:
	unregister_chrdev_region(dev_no, lunix_minor_cnt);
out:
	return ret;
}

void lunix_chrdev_destroy(void)
{
	dev_t dev_no;
	unsigned int lunix_minor_cnt = lunix_sensor_cnt << 3;
		
	debug("entering\n");
	dev_no = MKDEV(LUNIX_CHRDEV_MAJOR, 0);
	cdev_del(&lunix_chrdev_cdev);
	unregister_chrdev_region(dev_no, lunix_minor_cnt);
	debug("leaving\n");
}
