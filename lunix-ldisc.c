/*
 * lunix-ldisc.c
 *
 * TTY line discipline for Lunix:TNG
 *
 * Vangelis Koukis <vkoukis@cslab.ece.ntua.gr>
 *
 */

#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/serio.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <asm/atomic.h>
#include <asm/uaccess.h>

#include "lunix.h"
#include "lunix-ldisc.h"
#include "lunix-protocol.h"

/*
 * This line discipline can only be associated
 * with a single TTY at any time.
 */
static atomic_t lunix_disc_available;

/*
 * This function runs when the userspace helper
 * sets the Lunix:TNG line discipline on a TTY.
 */
static int lunix_ldisc_open(struct tty_struct *tty)
{
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	
	/* Can only be associated with a single TTY */
	if ( !atomic_add_unless(&lunix_disc_available, -1, 0))
		return -EBUSY;

	tty->receive_room = 65536; /* No flow control, FIXME */

	debug("lunix ldisc associated with TTY %s\n", tty->name);
	return 0;
}

/*
 * Called called whenever the discipline
 * is unregistered from a port.
 */

static void lunix_ldisc_close(struct tty_struct *tty)
{
	atomic_inc(&lunix_disc_available);
	/* FIXME */
	/* Shouldn't we wake up all sleepers in all sensors here? */
	debug("lunix ldisc being closed\n");
}

/*
 * lunix_ldisc_receive() is called by the TTY layer when data have been
 * received by the low level TTY driver and are ready for us. This function
 * will not be re-entered while running.
 */
static void lunix_ldisc_receive(struct tty_struct *tty,
	const unsigned char *cp, char *fp, int count)
{
#if 1
#if LUNIX_DEBUG
	int i;

	debug("called, %d characters have been received. Data at *cp: { ", count);
	for (i = 0; i < count; i++)
		printk("0x%02x%s", cp[i], (i == count - 1) ? "" : ", ");
	printk(" }\n");
#endif
	printk(KERN_INFO "lunix_ldisc_receive called\n");
#endif
	/*
	 * Pass incoming characters to protocol processing code,
	 * which handle any necessary sensor updates.
	 */
	lunix_protocol_received_buf(&lunix_protocol_state, cp, count);
	//debug("passed incoming bytes to state machine, leaving\n");
}

/*
 * Userspace can no longer access a TTY using read()
 * or write() calls after this discipline has been set to it.
 */

static ssize_t lunix_ldisc_read(struct tty_struct * tty, struct file * file,
	unsigned char __user * buf, size_t cnt)
{
	debug("called, returning -EIO\n");
	return -EIO;
}

static ssize_t lunix_ldisc_write(struct tty_struct * tty, struct file * file,
	const unsigned char __user * buf, size_t cnt)
{
	debug("called, returning -EIO\n");
	return -EIO;
}

/*
 * The line discipline structure.
 * Initialization and release functions.
 */

static struct tty_ldisc_ops lunix_ldisc_ops = {
	.owner =	THIS_MODULE,
	.name =		"lunix",
	.open =		lunix_ldisc_open,
	.close =	lunix_ldisc_close,
	.read =		lunix_ldisc_read,
	.write =	lunix_ldisc_write,
	.receive_buf =	lunix_ldisc_receive
};

int lunix_ldisc_init(void)
{
	int ret;

	debug("initializing lunix ldisc\n");
	atomic_set(&lunix_disc_available, 1);
	ret = tty_register_ldisc(N_LUNIX_LDISC, &lunix_ldisc_ops);
	if (ret)
		printk(KERN_ERR "%s: Error registering line discipline, ret = %d.\n", __FILE__, ret);
	
	debug("leaving with ret = %d\n", ret);
	return ret;
}

void lunix_ldisc_destroy(void)
{
	debug("unregistering lunix ldisc\n");
	tty_unregister_ldisc(N_LUNIX_LDISC);
	debug("lunix ldisc unregistered\n");
}

