/**
*	\file         tamplate.c
*	\brief        Tamplate.
* 
*	\author       Roman Garanin
*/

#include "TFminiPlus.h"

struct tf_out_data {
	union {
		struct {
			uint8_t header_1; //0x59
			uint8_t header_2; //0x59
			uint8_t dist_l;
			uint8_t dist_h;
			uint8_t strenght_l;
			uint8_t strenght_h;
			uint8_t temp_l;
			uint8_t temp_h;
			uint8_t checksum;
		} packet_struct;
		uint8_t packet[9];
	};
};

uint8_t checksum(struct tf_out_data *packet);
uint16_t parse_dist(struct tf_out_data *packet);
uint16_t parse_strenght(struct tf_out_data *packet);
uint16_t parse_temp(struct tf_out_data *packet);

uint8_t parse_data(uint8_t** data, uint8_t len, uint16_t* dist, uint16_t* strangth, uint16_t* temp);

uint8_t checksum(struct tf_out_data *packet)
{
	uint8_t calc = 0;
	for(uint8_t i = 0; i < 8; i++)
	{
		calc += packet->packet[i];
	}
	if(calc == packet->packet_struct.checksum){
		return 1;
	} else {
		return 0;
	}
}

uint16_t parse_dist(struct tf_out_data *packet)
{
	uint16_t dist = (uint16_t)packet->packet_struct.dist_h << 8;
	dist |= (uint16_t)packet->packet_struct.dist_l & 0x00FF;

	return dist;
}

uint16_t parse_strenght(struct tf_out_data *packet)
{
	uint16_t strenght = (uint16_t)packet->packet_struct.strenght_h << 8;
	strenght |= (uint16_t)packet->packet_struct.strenght_l  & 0x00FF;;

	return strenght;
}

uint16_t parse_temp(struct tf_out_data *packet)
{
	uint16_t temp = (uint16_t)packet->packet_struct.temp_h << 8;
	temp |= (uint16_t)packet->packet_struct.temp_l  & 0x00FF;;

	return temp;
}

#define PACKET_LENGHT 8
#define BUFFER_MAX 256
enum {
	wait = 0,
	wait_next,
	parse
};

static uint8_t buffer[BUFFER_MAX];

uint16_t d = 0;
uint16_t s = 0; 
uint16_t t = 0;

void input_data(uint8_t * data, uint8_t len)
{
	static uint8_t all_len = 0;
	static uint8_t state = wait;
	static uint8_t* write_ptr = buffer;
	static uint8_t* header = NULL;

	if((NULL == data) || (len == 0) || (len > BUFFER_MAX)){
		all_len = 0;
		state = wait;
		write_ptr = buffer;
		return;
	}

	if ((all_len + len) > BUFFER_MAX) {
		all_len = 0;
		state = wait;
		write_ptr = buffer;
		return;
	}

	all_len += len;
	memcpy(write_ptr, data, len);

	switch (state){
	case wait: //first call.

		all_len = find_header(write_ptr,&header, all_len);
		all_len = parse_data(&header, all_len, &d, &s, &t);
		break;
	case wait_next:
		all_len = parse_data(&header, all_len, &d, &s, &t);
		break;
	}

	if(0 == all_len) {
		write_ptr = buffer;
		header = NULL;
		state = wait;
		return;
	} else {
		write_ptr += len;
		state = wait_next;	
	}

} 

uint8_t parse_data(uint8_t** data, uint8_t len, uint16_t *dist, uint16_t *stranght, uint16_t *temp)
{
	if((NULL == data) || (0 == len)) {
		return 0;
	}

	uint8_t packets = len / 9;
	uint8_t last_data = len % 9;

	for (uint8_t i = 0; i < packets; i++)
	{
		bool cs_ok = checksum((struct tf_out_data*)*data);
		if (true == cs_ok) {
	 		*dist = parse_dist((struct tf_out_data*)*data);
			*stranght = parse_strenght((struct tf_out_data*)*data);
			*temp = parse_temp((struct tf_out_data*)*data);	
		} else {
			return 0;
		}
		*data += 9;
	}
	return last_data;
}

uint8_t find_header(uint8_t* seq, uint8_t** header, uint8_t len)
{
	if ((NULL == seq) || (0 == len) || (NULL == header)) {
		return 0;
	}
	for (uint8_t i = 0; i < len - 1; i++)
	{
		if((seq[i] == 0x59) && (seq[i+1] == 0x59)) {
			*header = &seq[i];

			return len - i;
		}
	}
	*header = NULL;
	return 0;
}

