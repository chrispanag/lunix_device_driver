/*
 * lunix-protocol.c
 *
 * This code implements the protocol used by the
 * wireless sensor network to report back on the
 * measurements of individual sensors.
 *
 * It receives raw packet data from the Lunix line discipline
 * and updates the relevant sensor structures with the
 * newly received measured values.
 *
 * Ioannis Panagopoulos <ioannis@cslab.ece.ntua.gr>
 * Vangelis Koukis <vkoukis@cslab.ece.ntua.gr>
 *
 */

#include <linux/kernel.h>
#include <asm/byteorder.h>

#include "lunix.h"
#include "lunix-protocol.h"

/*
 * Returns an unsigned 16-bit integer in native byte-order from 
 * two bytes in an XMesh packet, which is always little-endian
 */
static uint16_t uint16_from_packet(unsigned char *p)
{
	uint16_t le;
	
	memcpy(&le, p, 2);
	return le16_to_cpu(le);
}

/*
 * Will display the contents of an incoming XMesh packet
 * that have been received so far
 */
static inline void lunix_protocol_show_packet(struct lunix_protocol_state_struct *state)
{
#if LUNIX_DEBUG
	int ctr;

	debug("Current packet: called, pos = %d, Packet data: { ", state->pos);
	for (ctr = 0; ctr < state->pos; ctr++)
		printk("0x%02x%s", state->packet[ctr], (ctr == state->pos - 1) ? "" : ", ");
	printk(" }\n");
#endif
}

/*
 * Receives a complete XMesh packet and updates the node structures if
 * the packet contains sensor information. The function ignores other
 * types of packets. In future releases check packets with packet[4]
 * equal to 0x03, 0xFD for extending this function.
 */
static void lunix_protocol_update_sensors(struct lunix_protocol_state_struct *state, struct lunix_sensor_struct *lunix_sensors)
{
	uint16_t batt;
	uint16_t temp;
	uint16_t light;
	uint16_t nodeid;

	//debug("WHOLE PACKET\n");

	if (0x0B == state->packet[PACKET_SIGNATURE_OFFSET])
	{
		nodeid = uint16_from_packet(&state->packet[NODE_OFFSET]);
		batt = uint16_from_packet(&state->packet[VREF_OFFSET]);
		temp = uint16_from_packet(&state->packet[TEMPERATURE_OFFSET]);
		light = uint16_from_packet(&state->packet[LIGHT_OFFSET]);
		
		/* FIXME */
		//debug ("I have the following raw data from nodeid = %d: { batt, temp, light } = { 0x%04x, 0x%04x, 0x%04x }\n",
		//	nodeid, batt, temp, light);

		if (nodeid > 0 && nodeid <= lunix_sensor_cnt)
			lunix_sensor_update(&lunix_sensors[nodeid - 1], batt, temp, light);
		else
			printk(KERN_WARNING "Node id %d is out of bounds [maximum %d sensors]\n",
				nodeid, lunix_sensor_cnt);
	}
}

/**********************************************************************************
 * PACKET STRUCTURE						
 * BYTE				VALUE		MEANING
 * 0				0x7E		Packet Start byte signature
 * 1				0x??		Packet Type
 * 2-3				0x??		Destination Address
 * 4				0x??		AM Type
 * 5				0x??		AM Group
 * 6				0x??		Payload length (PL)
 * 7-(7 + PL-1)			0x??		PayLoad
 * (7 + PL)-(7 + PL + 1)	0x??		CRC
 * (7 + PL + 2)			0X7E		Packet End byte signature
 **********************************************************************************/

/*
 * Helper function to quickly set the current state
 */
static inline void set_state(struct lunix_protocol_state_struct *statep, int state, int btr, int br)
{
	//debug("switching to state %d, bytes_to_read = %d, bytes_read = %d\n",
	//	state, btr, br);
	statep->state = state;
	statep->bytes_to_read = btr;
	statep->bytes_read = br;
	//lunix_protocol_show_packet(statep);
}

/*
 * Initialization of protocol state machine
 */
void lunix_protocol_init(struct lunix_protocol_state_struct *state)
{
	state->pos = 0;
	state->next_is_special = 0;
	set_state(state, SEEKING_START_BYTE, 1, 0);
}

/*
 * Crucial function for parsing the input packet according
 * to the current state.
 *
 * struct lunix_protocol_state_struct *state: 
 * unsigned char *data: the data received
 * int length: the amount of bytes received
 * int *i: the pointer to the data received is updated when data are 
 *         transferred to the unparsed_packet array
 * int use_specials: if 1 special characters are treated acc
 */
static int lunix_protocol_parse_state(struct lunix_protocol_state_struct *state,
	const unsigned char *data, int length, int *i, int use_specials)
{
	int iter;

	//debug("entering, for *i = %d, length = %d, state = %d, btr = %d, br = %d, next_is_special = %d\n",
	//	*i, length, state->state, state->bytes_to_read, state->bytes_read, state->next_is_special);

	iter = 0;
	while ((*i < length) && (state->bytes_read < state->bytes_to_read))
	{
#if LUNIX_DEBUG
		if (++iter == 50) {
			debug("OOPS!\n");
			return -1;
		}
#endif
		/* Prevent buffer overflows */
		if (state->pos == MAX_PACKET_LEN) {
			printk(KERN_ERR "WARNING: state->pos == %d, MAX_PACKET_LEN is %d,"
				"packet buffer would overflow!\n", state->pos, MAX_PACKET_LEN);
			printk(KERN_ERR "How will I ever resync with the input stream?\n");
			state->pos = 0;
			return -1;
		}

		if (1 == use_specials)
		{
			if (state->next_is_special)
			{
				if (0x7E == state->next_is_special)
					state->packet[state->pos] = data[*i];
				if (0x7D == state->next_is_special)
					state->packet[state->pos] = data[*i]^0x20;
				++state->pos;
				++state->bytes_read;
				++(*i);
				state->next_is_special = 0;
			}
			else
			{
				if ((0x7E == data[*i]) || (0x7D == data[*i]))
				{
					state->next_is_special = data[*i];
					++(*i);
				} else {
					state->packet[state->pos] = data[*i];
					++state->pos;
					++state->bytes_read;
					++(*i);
				}
			}
		}
		else
		{
			state->packet[state->pos] = data[*i];
			++state->pos;
			++state->bytes_read;
			++(*i);
		}
	}

	if (state->bytes_read == state->bytes_to_read) {
		//debug("returning 1 from state = %d, btr = %d, br = %d, next_is_special = %d\n",
		//	state->state, state->bytes_to_read, state->bytes_read, state->next_is_special);
		return 1;
	}	

	//debug("returning 0 from state = %d, btr = %d, br = %d, next_is_special = %d\n",
	//	state->state, state->bytes_to_read, state->bytes_read, state->next_is_special);
	return 0;
}

/*
 * This function gets called for incoming data
 * to update the protocol state machine.
 */

int lunix_protocol_received_buf(struct lunix_protocol_state_struct *state,
	const unsigned char *buf, int length)
{
	int i;
	int payload_length;

	i = 0;

	if (state->state == SEEKING_START_BYTE) 
		if (lunix_protocol_parse_state(state, buf, length, &i, 0) == 1)
			set_state(state, SEEKING_PACKET_TYPE, 1, 0);


	if (state->state == SEEKING_PACKET_TYPE) 
		if (lunix_protocol_parse_state(state, buf, length, &i, 0) == 1)
			set_state(state, SEEKING_DESTINATION_ADDRESS, 2, 0);

	if (state->state == SEEKING_DESTINATION_ADDRESS) 
		if (lunix_protocol_parse_state(state, buf, length, &i, 1) == 1)
			set_state(state, SEEKING_AM_TYPE, 1, 0);

	if (state->state == SEEKING_AM_TYPE) 
		if (lunix_protocol_parse_state(state, buf, length, &i, 1) == 1)
			set_state(state, SEEKING_AM_GROUP, 1, 0);

	if (state->state == SEEKING_AM_GROUP) 
		if (lunix_protocol_parse_state(state, buf, length, &i, 1) == 1)
			set_state(state, SEEKING_PAYLOAD_LENGTH, 1, 0);

	if (state->state == SEEKING_PAYLOAD_LENGTH) 
		if (lunix_protocol_parse_state(state, buf, length, &i, 1) == 1) {
			payload_length = state->packet[state->pos - 1];
			set_state(state, SEEKING_PAYLOAD, payload_length, 0);
		}

	if (state->state == SEEKING_PAYLOAD) 
		if (lunix_protocol_parse_state(state, buf, length, &i, 1) == 1)
			set_state(state, SEEKING_CRC, 2, 0);

	if (state->state == SEEKING_CRC) 
		if (lunix_protocol_parse_state(state, buf, length, &i, 1) == 1)
			set_state(state, SEEKING_END_BYTE, 1, 0);

	if (state->state == SEEKING_END_BYTE) 
		if (lunix_protocol_parse_state(state, buf, length, &i, 0) == 1) {
			//debug("An XMesh packet has been received, updating sensors\n");

			lunix_protocol_update_sensors(state, lunix_sensors);
			state->pos = 0;
			state->next_is_special = 0;
			set_state(state, SEEKING_START_BYTE, 1, 0);
		}

	//debug("leaving\n");

	return 0;
}
