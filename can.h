#ifndef PA_CAN_H
#define PA_CAN_H

typedef enum {
	CAN_STATUS_WAIT,
	CAN_STATUS_OK,
	CAN_STATUS_FAIL,
} can_set_status_t;

void can_setup(void);
void can_loop(void);
bool can_request_ps_status(void);
unsigned long can_ps_get_last_rx(void);
can_set_status_t can_set_response_status(void);

#define R4850G2_INPUT_POWER		0x01700000
#define R4850G2_INPUT_FREQUENCY		0x01710000
#define R4850G2_INPUT_CURRENT		0x01720000
#define R4850G2_OUTPUT_POWER		0x01730000
#define R4850G2_EFFICIENCY		0x01740000
#define R4850G2_OUTPUT_VOLTAGE		0x01750000
#define R4850G2_OUTPUT_CURRENT_MAX	0x01760000
#define R4850G2_INPUT_VOLTAGE		0x01780000
#define R4850G2_OUTPUT_TEMPERATURE	0x017f0000
#define R4850G2_INPUT_TEMPERATURE	0x01800000
#define R4850G2_OUTPUT_CURRENT		0x01810000

#define R4850G2_REQUEST_STATUS		0x108040fe
#define R4850G2_PRESENT			0x100011fe //0x1001117e
#define R4850G2_SET_RESPONSE		0x1081807e
#define R4850G2_STATUS_RESPONSE		0x1081407f

#define R4850G2_SET_ONLINE_OUTPUT_VOLTAGE	0x00
#define R4850G2_SET_OFFLINE_OUTPUT_VOLTAGE	0x01
#define R4850G2_SET_OVP				0x02
#define R4850G2_SET_ONLINE_CURRENT_LIMIT	0x03
#define R4850G2_SET_OFFLINE_CURRENT_LIMIT	0x04

struct r4850g2_status {
	float input_power;
	float input_frequency;
	float input_current;
	float input_voltage;
	float input_temperature;
	float output_power;
	float output_current;
	float output_voltage;
	float output_current_max;
	float output_temperature;
	float efficiency;
};

extern struct r4850g2_status ps_status;
int can_ps_set(int setting, int value);
#endif
