/*
 * lunix-chrdev.h
 *
 * Definition file for the
 * Lunix:TNG TTY line discipline
 *
 * Vangelis Koukis <vkoukis@cslab.ece.ntua.gr>
 *
 */

#ifndef _LUNIX_LDISC_H
#define _LUNIX_LDISC_H

/* Compile-time parameters */

#ifdef __KERNEL__ 

/*
 * Function prototypes
 */
int lunix_ldisc_init(void);
void lunix_ldisc_destroy(void);

#endif	/* __KERNEL__ */

#endif	/* _LUNIX_H */

