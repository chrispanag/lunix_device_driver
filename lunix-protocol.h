/*
 * lunix-protocol.h
 *
 * Definitions file for the implementation of the
 * protocol used by the wireless sensor network to report
 * back on the measurements of individual sensors.
 *
 * Ioannis Panagopoulos <ioannis@cslab.ece.ntua.gr>
 * Vangelis Koukis <vkoukis@cslab.ece.ntua.gr>
 *
 */

#ifndef _LUNIX_PROTOCOL_H
#define _LUNIX_PROTOCOL_H

#ifdef __KERNEL__ 

/*
 * Application/Protocol specific constants
 */
#define MAX_PACKET_LEN 300
#define PACKET_SIGNATURE_OFFSET 4
#define NODE_OFFSET 9
#define VREF_OFFSET 18
#define TEMPERATURE_OFFSET 20
#define LIGHT_OFFSET 22

/*
 * States of the Lunix protocol state machine
 */
#define SEEKING_START_BYTE             1
#define SEEKING_PACKET_TYPE            2
#define SEEKING_DESTINATION_ADDRESS    3
#define SEEKING_AM_TYPE                4
#define SEEKING_AM_GROUP               5
#define SEEKING_PAYLOAD_LENGTH         6
#define SEEKING_PAYLOAD                7
#define SEEKING_CRC                    8
#define SEEKING_END_BYTE               9

/*
 * Current state of the Lunix protocol state machine
 */
struct lunix_protocol_state_struct
{
	int state;                      /* The current state of the protocol state machine */
	int bytes_read;	
	int bytes_to_read;

	int pos;                        /* Current pos in the XMesh Packet */
	unsigned char next_is_special;  /* The next character to be received is a special character */
	unsigned char payload_length;   /* The length of the payload of the received packet */
	unsigned char packet[MAX_PACKET_LEN]; /* The XMesh packet being received */
};

/*
 * Function prototypes
 */
void lunix_protocol_init(struct lunix_protocol_state_struct *);
int lunix_protocol_received_buf(struct lunix_protocol_state_struct *, const unsigned char *buf, int count);

#endif	/* __KERNEL__ */

#endif	/* _LUNIX_H */

