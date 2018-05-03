/*
 * lunix.h
 *
 * Definition file for Lunix:TNG
 *
 * Vangelis Koukis <vkoukis@cslab.ece.ntua.gr>
 *
 */

#ifndef _LUNIX_H
#define _LUNIX_H

/* Compile-time parameters */
#define LUNIX_VERSION_STRING	"0.1701-D"

#ifdef __KERNEL__ 

#include <linux/fs.h>
#include <linux/tty.h>
#include <linux/kernel.h>
#include <linux/module.h>

/*
 * A structure representing a hardware sensor
 * and pages holding the most recent measurements received
 */

#define LUNIX_MSR_MAGIC 0xF00DF00D

enum lunix_msr_enum { BATT = 0, TEMP, LIGHT, N_LUNIX_MSR };
struct lunix_sensor_struct {
	/*
	 * A number of pages, one for each measurement.
	 * They can be mapped to userspace.
	 */
	struct lunix_msr_data_struct *msr_data[N_LUNIX_MSR];

	/*
	 * Spinlock used to assert mutual exclusion between
	 * the serial line discipline and the character device driver
	 */
	spinlock_t lock;

	/*
	 * A list of processes waiting to be woken up
	 * when this sensor has been updated with new data
	 */
	wait_queue_head_t wq;
};

/*
 * The default value for the maximum number of sensors supported
 */
#define LUNIX_SENSOR_CNT			16
extern int lunix_sensor_cnt;
extern struct lunix_sensor_struct *lunix_sensors;
extern struct lunix_protocol_state_struct lunix_protocol_state;

/*
 * Debugging
 */

#if LUNIX_DEBUG
#define debug(fmt,arg...)     printk(KERN_DEBUG "%s: " fmt, __func__ , ##arg)
#else
#define debug(fmt,arg...)     do { } while(0)
#endif

/*
 * Function prototypes
 */
int lunix_sensor_init(struct lunix_sensor_struct *);
void lunix_sensor_destroy(struct lunix_sensor_struct *);
void lunix_sensor_update(struct lunix_sensor_struct *s,
	uint16_t batt, uint16_t temp, uint16_t light);

#else
#include <inttypes.h>
#endif	/* __KERNEL__ */
/*
 * A structure, living at the start of a page, containing a version number
 * [timestamp of last update] and a variable number of 32-bit quantities. It is
 * meant to be mappable to userspace.
 */
struct lunix_msr_data_struct {
	uint32_t magic;
	uint32_t last_update;
	uint32_t values[];
};

/*
 * Lunix:TNG line discipline number:
 * Hijack the "Mobitex module" line discipline, since the number
 * of allowed line disciplines is hard-coded in <linux/tty.h>:
 *
 * #define N_MASC          8       / * Reserved for Mobitex module <kaz@cafe.net> * /
 */
#define N_LUNIX_LDISC            N_MASC

#endif	/* _LUNIX_H */
