#include <due_can.h>
#include "can.h"
#include <memory>

struct r4850g2_status ps_status;
static unsigned long ps_last_rx;
static can_set_status_t can_status;

void can_setup(void)
{
	Can0.begin(CAN_BPS_125K, PIN_CAN_ENABLE);
	Can0.watchFor();
}

unsigned long can_ps_get_last_rx(void)
{
	return millis() - ps_last_rx;
}

can_set_status_t can_set_response_status(void)
{
	return can_status;
}

bool can_request_ps_status(void)
{
	CAN_FRAME frame;

	memset(&frame, 0, sizeof(frame));
	frame.id = R4850G2_REQUEST_STATUS;
	frame.extended = true;
	frame.length = 8;
	if (!Can0.sendFrame(frame)) {
		can_status = CAN_STATUS_FAIL;
		return -1;
	}
	can_status = CAN_STATUS_WAIT;
	return 0;
}

static uint16_t bswap16(uint16_t val)
{
	return (val << 8) | (val >> 8);
}

static uint32_t bswap32(uint32_t val)
{
	return (bswap16(val) << 16) | bswap16(val >> 16);
}

static void can_handle_ps_status(uint32_t code, float val)
{
	switch (code) {
	case R4850G2_INPUT_POWER:
		ps_status.input_power = val;
		ps_last_rx = millis();
		break;
	case R4850G2_INPUT_FREQUENCY:
		ps_status.input_frequency = val;
		ps_last_rx = millis();
		break;
	case R4850G2_INPUT_CURRENT:
		ps_status.input_current = val;
		ps_last_rx = millis();
		break;
	case R4850G2_OUTPUT_POWER:
		ps_status.output_power = val;
		ps_last_rx = millis();
		break;
	case R4850G2_EFFICIENCY:
		ps_status.efficiency = val;
		ps_last_rx = millis();
		break;
	case R4850G2_OUTPUT_VOLTAGE:
		ps_status.output_voltage = val;
		ps_last_rx = millis();
		break;
	case R4850G2_OUTPUT_CURRENT_MAX:
		ps_status.output_current_max = val;
		ps_last_rx = millis();
		break;
	case R4850G2_INPUT_VOLTAGE:
		ps_status.input_voltage = val;
		ps_last_rx = millis();
		break;
	case R4850G2_OUTPUT_TEMPERATURE:
		ps_status.output_temperature = val;
		ps_last_rx = millis();
		break;
	case R4850G2_INPUT_TEMPERATURE:
		ps_status.input_temperature = val;
		ps_last_rx = millis();
		break;
	case R4850G2_OUTPUT_CURRENT:
		ps_status.output_current = val;
		ps_last_rx = millis();
		break;
	default:
		break;
	}
}

void can_loop(void)
{
	while (Can0.available() > 0) {
		CAN_FRAME frame;
		Can0.read(frame);
		uint32_t val = bswap32(frame.data.high);
		float fval = (float)(val >> 10) + ((float)(val & 0x3ff) / 1024.0);
		uint32_t code = bswap32(frame.data.low);
		ps_last_rx = millis();
		switch(frame.id) {
		case R4850G2_STATUS_RESPONSE:
			can_handle_ps_status(code, fval);
			break;
		case R4850G2_PRESENT:
			break;
		case R4850G2_SET_RESPONSE:
			if (frame.data.byte[0] & 0x20)
				can_status = CAN_STATUS_FAIL;
			else
				can_status = CAN_STATUS_OK;
			break;
		default:
//			printf("unknown can rx id %08lx val %08lx, code %08lx\r\n", frame.id, val, code);
			break;
		}
	}
}

int can_ps_set(int setting, int value)
{
	CAN_FRAME frame;

	memset(&frame, 0, sizeof(frame));
	frame.id = 0x108180fe;
	frame.extended = 1;
	frame.data.byte[0] = 0x01;
	frame.data.byte[1] = setting;
	frame.length = 8;
	frame.data.high = bswap32(value << 10);
	if (!Can0.sendFrame(frame))
		return -1;
	return 0;
}
