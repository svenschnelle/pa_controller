#include <due_can.h>
#include <DallasTemperature.h>
#include "lcd.h"
#include <Wire.h>
#include "pins.h"
#include "can.h"
#include "touch.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "adc.h"
#include "eeprom.h"

#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))

static OneWire oneWire(PIN_IBUS);
static DallasTemperature sensors(&oneWire);
static DeviceAddress sensor_addr;

static volatile unsigned int tacho1;
static volatile unsigned int tacho2;

static bool prot_key_pressed(int button);
static bool band_key_pressed(int button);
static bool amp_key_pressed(int button);
static bool ant_key_pressed(int button);
static bool pwr_key_pressed(int button);
static bool ptt_key_pressed(int button);

static void prot_key_update(int button);
static void band_key_update(int button);
static void amp_key_update(int button);
static void ant_key_update(int button);
static void pwr_key_update(int button);
static void ptt_key_update(int button);

static volatile uint32_t touch_int_pending;

typedef enum {
	PROT_NONE,
	PROT_TEMPERATURE,
	PROT_OUTPUT_SWR,
	PROT_FILTER_SWR,
	PROT_CURRENT,
	PROT_PSU_TIMEOUT,
} prot_type_t;

typedef enum {
	TOUCH_STATE_PRESS_DOWN=0,
	TOUCH_STATE_LIFT_UP,
	TOUCH_STATE_CONTACT,
	TOUCH_STATE_NONE,
} touch_state_t;

static const char *prot_names[] =
{
	[PROT_NONE]		= "",
	[PROT_TEMPERATURE]	= "Temperature",
	[PROT_OUTPUT_SWR]	= "Output SWR",
	[PROT_FILTER_SWR]	= "Filter SWR",
	[PROT_CURRENT]		= "Current above limit",
	[PROT_PSU_TIMEOUT]	= "PSU communication timeout"
};

typedef enum {
	PSU_WAIT_INITIAL,
	PSU_VOLTAGE_SET,
	PSU_VOLTAGE_WAIT,
	PSU_CURRENT_SET,
	PSU_CURRENT_WAIT,
	PSU_NORMAL,
} psu_state_t;

typedef enum {
	FORCE_BAND_AUTO,
	FORCE_BAND_160,
	FORCE_BAND_80,
	FORCE_BAND_6040,
	FORCE_BAND_3020,
	FORCE_BAND_1715,
	FORCE_BAND_1210,
	FORCE_BAND_6,
} force_band_t;

typedef enum {
	PTT_RESET,
	PTT_OFF,
	PTT_ON,
	PTT_CHANGE_ON,
	PTT_CHANGE_OFF
} ptt_state_t;

static const char *force_band_names[] = {
	[FORCE_BAND_AUTO]	= "AUTO",
	[FORCE_BAND_160]	= "160m",
	[FORCE_BAND_80]		= "80m",
	[FORCE_BAND_6040]	= "60/40m",
	[FORCE_BAND_3020]	= "30/20m",
	[FORCE_BAND_1715]	= "17/15m",
	[FORCE_BAND_1210]	= "12/10m",
	[FORCE_BAND_6]		= "6m"
};

typedef enum {
	FAN_MODE_AUTO,
	FAN_MODE_OFF,
	FAN_MODE_LOW,
	FAN_MODE_MED,
	FAN_MODE_HIGH,
} fan_mode_t;

static const char *fan_mode_names[] = {
	[FAN_MODE_AUTO] = "AUTO",
	[FAN_MODE_OFF] = "OFF",
	[FAN_MODE_LOW] = "LOW",
	[FAN_MODE_MED] = "MED",
	[FAN_MODE_HIGH] = "HIGH",
};

static volatile prot_type_t prot_triggered;
static psu_state_t psu_state = PSU_WAIT_INITIAL;

#define NUM_BUTTONS 8

static struct pa_config {
	int current_antenna;
	int pa_enabled;
	int pa_high_power;
	bool ptt_enabled;
	fan_mode_t fan_mode;
	force_band_t force_band;
	uint8_t csum;
} pa_config;

struct menu_button {
	const char *text;
	void (*update)(int button);
	bool (*pressed)(int button);
	bool stdby;
};

static const struct menu_button *current_menu;
static bool band_menu_key_pressed(int button);
static void prot_trigger(prot_type_t type);

static const struct menu_button band_menu[NUM_BUTTONS] = {
	[0] = { "AUTO", NULL, band_menu_key_pressed, false },
	[1] = { "160m", NULL, band_menu_key_pressed, false },
	[2] = { "80m", NULL, band_menu_key_pressed, false },
	[3] = { "60/40m", NULL, band_menu_key_pressed, false },
	[4] = { "30/20m", NULL, band_menu_key_pressed, false },
	[5] = { "17/15m", NULL, band_menu_key_pressed, false },
	[6] = { "12/10m", NULL, band_menu_key_pressed, false },
	[7] = { "6m", NULL, band_menu_key_pressed, false },
};

static void setup_emb(void)
{
	SmcCs_number *smc = &SMC->SMC_CS_NUMBER[3];

	pmc_enable_periph_clk(ID_SMC);
	delay(100);

	PIO_Configure(PIOB, PIO_PERIPH_A, PIO_PB27, PIO_DEFAULT);	// NCS3  - 13
	PIO_Configure(PIOA, PIO_PERIPH_B, PIO_PA29, PIO_DEFAULT);	// NRD   -  4
	PIO_Configure(PIOC, PIO_PERIPH_A, PIO_PC2, PIO_DEFAULT);	// D0    - 34
	PIO_Configure(PIOC, PIO_PERIPH_A, PIO_PC3, PIO_DEFAULT);	// D1    - 35
	PIO_Configure(PIOC, PIO_PERIPH_A, PIO_PC4, PIO_DEFAULT);	// D2    - 36
	PIO_Configure(PIOC, PIO_PERIPH_A, PIO_PC5, PIO_DEFAULT);	// D3    - 37
	PIO_Configure(PIOC, PIO_PERIPH_A, PIO_PC6, PIO_DEFAULT);	// D4    - 38
	PIO_Configure(PIOC, PIO_PERIPH_A, PIO_PC7, PIO_DEFAULT);	// D5    - 39
	PIO_Configure(PIOC, PIO_PERIPH_A, PIO_PC8, PIO_DEFAULT);	// D6    - 40
	PIO_Configure(PIOC, PIO_PERIPH_A, PIO_PC9, PIO_DEFAULT);	// D7    - 41
	PIO_Configure(PIOC, PIO_PERIPH_A, PIO_PC18, PIO_DEFAULT);	// NWE   - 45
	PIO_Configure(PIOC, PIO_PERIPH_A, PIO_PC23, PIO_DEFAULT);	// A2    -  7
	PIO_Configure(PIOC, PIO_PERIPH_A, PIO_PC13, PIO_DEFAULT);	// NWAIT - 50
	PIO_Configure(PIOC, PIO_INPUT, PIO_PC26, PIO_DEFAULT);		// NRD - connected on due to PA29
	smc->SMC_SETUP = SMC_SETUP_NCS_RD_SETUP(4) | SMC_SETUP_NRD_SETUP(4) \
		| SMC_SETUP_NCS_WR_SETUP(4) | SMC_SETUP_NWE_SETUP(4);
	smc->SMC_PULSE = SMC_PULSE_NCS_RD_PULSE(16) | SMC_PULSE_NRD_PULSE(8) \
		       | SMC_PULSE_NCS_WR_PULSE(16) | SMC_PULSE_NWE_PULSE(8);
	smc->SMC_CYCLE = SMC_CYCLE_NRD_CYCLE(16) | SMC_CYCLE_NWE_CYCLE(16);
	smc->SMC_TIMINGS = 0;
	smc->SMC_MODE = SMC_MODE_READ_MODE | SMC_MODE_WRITE_MODE | SMC_MODE_TDF_CYCLES(15);

}

static void setup_ibus(void)
{
	sensors.begin();
	sensors.setWaitForConversion(false);
	sensors.getAddress(sensor_addr, 0);
}

void set_power(bool on)
{
	digitalWrite(PIN_HV_POWER1, on);
	digitalWrite(PIN_HV_POWER2, on);
	if (!on)
		digitalWrite(PIN_INHIBIT, LOW);
}

static bool pa_not_ready(void)
{
	return psu_state != PSU_NORMAL || prot_triggered != PROT_NONE;
}

static void tacho1_isr(void)
{
	tacho1++;
}

static void tacho2_isr(void)
{
	tacho2++;
}

static void touch_isr(void)
{
	touch_int_pending = 1;
}

static void setup_gpio(void)
{
	digitalWrite(PIN_BAND_6, 0);
	digitalWrite(PIN_BAND_1210, 0);
	digitalWrite(PIN_BAND_1715, 0);
	digitalWrite(PIN_BAND_3020, 0);
	digitalWrite(PIN_BAND_6040, 0);
	digitalWrite(PIN_BAND_80, 0);
	digitalWrite(PIN_ANT1, 0);
	digitalWrite(PIN_ANT2, 0);
	digitalWrite(PIN_PTT_OUT, 0);
	digitalWrite(PIN_PROT_CLEAR, 0);
	digitalWrite(PIN_HV_POWER1, 0);
	digitalWrite(PIN_HV_POWER2, 0);
	digitalWrite(PIN_INHIBIT, 0);
	digitalWrite(PIN_IBUS, 1);
	pinMode(PIN_ANT1, OUTPUT);
	pinMode(PIN_ANT2, OUTPUT);
	pinMode(PIN_PTT_OUT, OUTPUT);
	pinMode(PIN_BAND_80, OUTPUT);
	pinMode(PIN_BAND_6040, OUTPUT);
	pinMode(PIN_BAND_3020, OUTPUT);
	pinMode(PIN_BAND_1715, OUTPUT);
	pinMode(PIN_BAND_1210, OUTPUT);
	pinMode(PIN_BAND_6, OUTPUT);
	pinMode(PIN_PROT_CLEAR, OUTPUT);
	pinMode(PIN_HV_POWER1, OUTPUT);
	pinMode(PIN_HV_POWER2, OUTPUT);
	pinMode(PIN_LCD_BL, OUTPUT);
	pinMode(PIN_LCD_RESET, OUTPUT);
	pinMode(PIN_FAN_SWITCH, OUTPUT);
	pinMode(PIN_INHIBIT, OUTPUT);

	pinMode(PIN_PTT_IN, INPUT);
	pinMode(PIN_BANDA, INPUT_PULLUP);
	pinMode(PIN_BANDB, INPUT_PULLUP);
	pinMode(PIN_BANDC, INPUT_PULLUP);
	pinMode(PIN_BANDD, INPUT_PULLUP);
	pinMode(PIN_IBUS, OUTPUT);
	pinMode(PIN_PROT_SWR, INPUT);
	pinMode(PIN_PROT_CURRENT, INPUT);

//	pinMode(PIN_LCD_CS, INPUT);
	pinMode(PIN_FAN_TACHO1, INPUT_PULLUP);
	pinMode(PIN_FAN_TACHO2, INPUT_PULLUP);
	analogWrite(PIN_LCD_BL, 255);
	analogWrite(PIN_FAN_PWM, 50);
	attachInterrupt(digitalPinToInterrupt(PIN_FAN_TACHO1), tacho1_isr, RISING);
	attachInterrupt(digitalPinToInterrupt(PIN_FAN_TACHO2), tacho2_isr, RISING);
	attachInterrupt(digitalPinToInterrupt(PIN_TOUCH_INT), touch_isr, FALLING);
	pinMode(14, OUTPUT);
	pinMode(15, OUTPUT);
	pinMode(16, OUTPUT);
}

static void setup_timer(void)
{
	uint32_t rc = VARIANT_MCK / (128 / 16);

	pmc_set_writeprotect(false);

	// Tacho Timer
	pmc_enable_periph_clk(TC3_IRQn);
	TC_Configure(TC1, 0, TC_CMR_WAVE | TC_CMR_WAVSEL_UP_RC | TC_CMR_TCCLKS_TIMER_CLOCK1);
	TC_SetRA(TC1, 0, rc / 2);
	TC_SetRC(TC1, 0, rc);
	TC_Start(TC1, 0);
	TC1->TC_CHANNEL[0].TC_IER = TC_IER_CPCS | TC_IER_CPAS;
	TC1->TC_CHANNEL[0].TC_IDR = ~(TC_IER_CPCS | TC_IER_CPAS);
	NVIC_EnableIRQ(TC3_IRQn);
}

static void select_antenna(int num)
{
	switch (num) {
	case 0:
		digitalWrite(PIN_ANT1, 0);
		digitalWrite(PIN_ANT2, 0);
		break;
	case 1:
		digitalWrite(PIN_ANT1, 1);
		digitalWrite(PIN_ANT2, 0);
		break;
	case 2:
		digitalWrite(PIN_ANT1, 1);
		digitalWrite(PIN_ANT2, 1);
		break;
	}
}

static uint8_t get_band_inputs(void)
{
	uint8_t ret = 0;

	if (digitalRead(PIN_BANDA))
		ret |= 1;
	if (digitalRead(PIN_BANDB))
		ret |= 2;
	if (digitalRead(PIN_BANDC))
		ret |= 4;
	if (digitalRead(PIN_BANDD))
		ret |= 8;
	return ret;
}

static void set_band_outputs(void)
{
	const char *bandnames[16] = {
		"", "160m", "80m", "40m",
		"30m", "20m", "17m", "15m",
		"12m", "10m", "6m", "60m",
		""
	};

	if (!pa_config.pa_enabled) {
		digitalWrite(PIN_BAND_6, LOW);
		digitalWrite(PIN_BAND_1210, LOW);
		digitalWrite(PIN_BAND_1715, LOW);
		digitalWrite(PIN_BAND_3020, LOW);
		digitalWrite(PIN_BAND_6040, LOW);
		digitalWrite(PIN_BAND_80, LOW);
		lcd_printf(400, 0, COLOR65K_GREEN, COLOR65K_GRAYSCALE15, "    ");
		return;
	}

	if (pa_config.force_band != FORCE_BAND_AUTO) {
		digitalWrite(PIN_BAND_6, pa_config.force_band == FORCE_BAND_6);
		digitalWrite(PIN_BAND_1210, pa_config.force_band == FORCE_BAND_1210);
		digitalWrite(PIN_BAND_1715, pa_config.force_band == FORCE_BAND_1715);
		digitalWrite(PIN_BAND_3020, pa_config.force_band == FORCE_BAND_3020);
		digitalWrite(PIN_BAND_6040, pa_config.force_band == FORCE_BAND_6040);
		digitalWrite(PIN_BAND_80, pa_config.force_band == FORCE_BAND_80);
		lcd_printf(400, 0, COLOR65K_GREEN, COLOR65K_GRAYSCALE15, "    ");
	} else {
		int val = get_band_inputs();
		digitalWrite(PIN_BAND_6, val == 10); // TODO: verify
		digitalWrite(PIN_BAND_1210, val == 8 || val == 9);
		digitalWrite(PIN_BAND_1715, val == 6 || val == 7);
		digitalWrite(PIN_BAND_3020, val == 4 || val == 5);
		digitalWrite(PIN_BAND_6040, val == 11 || val == 3); // TODO: verify
		digitalWrite(PIN_BAND_80, val == 2);
		lcd_printf(400, 0, COLOR65K_GREEN, COLOR65K_GRAYSCALE15, "%4.4s", bandnames[val]);
	}
}

static void ps_voltage_update(void)
{
	if (psu_state == PSU_NORMAL)
		psu_state = PSU_WAIT_INITIAL;
}

static void prot_reset(void)
{
	prot_triggered = PROT_NONE;
	set_power(pa_config.pa_enabled);
	swr_reset_all();
	pwrswr_filter.swr = 1;
	pwrswr_antenna.swr = 1;
	pwrswr_input.swr = 1;
	status_display(COLOR65K_RED, "                        ");
}

static const struct menu_button main_menu[NUM_BUTTONS] = {
	{ "AMP", amp_key_update, amp_key_pressed, true },
	{ "ANT", ant_key_update, ant_key_pressed, true },
	{ "PWR", pwr_key_update, pwr_key_pressed, false },
	{ "BAND", band_key_update, band_key_pressed, false },
	{ "PTT", ptt_key_update, ptt_key_pressed, false },
	{ "FAN", fan_key_update, fan_key_pressed, false },
	{ NULL, NULL, NULL, false },
	{ "PROT", prot_key_update, prot_key_pressed, false },

};

static bool band_key_pressed(int button)
{
	(void)button;
	activate_menu(band_menu);
	return false;
}

static void band_key_update(int button)
{
	lcd_printf_button(button, 0, force_band_names[pa_config.force_band]);
}

static bool ptt_key_pressed(int button)
{
	(void)button;

	pa_config.ptt_enabled ^= 1;
	return true;
}

static void ptt_key_update(int button)
{
	lcd_printf_button(button, 0, pa_config.ptt_enabled ? "ON" : "OFF");
}

static bool ant_key_pressed(int button)
{
	(void)button;

	if (++pa_config.current_antenna > 2)
		pa_config.current_antenna = 0;
	select_antenna(pa_config.current_antenna);
	return true;
}

static void ant_key_update(int button)
{
	lcd_printf_button(button, 0, "%d", pa_config.current_antenna+1);
}

static bool amp_key_pressed(int button)
{
	(void)button;

	if (!pa_config.pa_enabled && (psu_state != PSU_NORMAL || prot_triggered != PROT_NONE))
		return false;


	pa_config.pa_enabled ^= 1;
	if (pa_config.pa_enabled) {
		pa_config.ptt_enabled = 1;
		draw_bargraphs();
	} else {
		pa_config.ptt_enabled = 0;
		clear_bargraphs();
		prot_reset();
		pa_config.fan_mode = FAN_MODE_AUTO;
	}
	set_power(pa_config.pa_enabled);
	activate_menu(main_menu);
	return true;
}

static void amp_key_update(int button)
{
	lcd_printf_button(button, 0, "%s", pa_config.pa_enabled ? "ON" : "OFF");
}

static void prot_key_update(int button)
{
	(void)button;

	if (pa_config.pa_enabled)
		lcd_printf_button(button, 0, "%s", prot_triggered ? "FAIL" : "OK");
}

static bool prot_key_pressed(int button)
{
	(void)button;

	prot_reset();
	prot_key_update(button);
	return false;
}

static void pwr_key_update(int button)
{
	(void)button;

	lcd_printf_button(button, 0, "%s", pa_config.pa_high_power ? "HIGH" : "LOW");
}

static bool pwr_key_pressed(int button)
{
	(void)button;

	pa_config.pa_high_power ^= 1;
	ps_voltage_update();
	return true;
}

static void fan_key_update(int button)
{
	lcd_printf_button(button, 0, "%s", fan_mode_names[pa_config.fan_mode]);
}

static bool fan_key_pressed(int button)
{
	(void)button;

	switch(pa_config.fan_mode) {
	case FAN_MODE_OFF:
		analogWrite(PIN_FAN_PWM, 66);
		digitalWrite(PIN_FAN_SWITCH, 1);
		pa_config.fan_mode = FAN_MODE_AUTO;
		break;
	case FAN_MODE_AUTO:
		analogWrite(PIN_FAN_PWM, 50);
		digitalWrite(PIN_FAN_SWITCH, 1);
		pa_config.fan_mode = FAN_MODE_LOW;
		break;
	case FAN_MODE_LOW:
		analogWrite(PIN_FAN_PWM, 100);
		digitalWrite(PIN_FAN_SWITCH, 1);
		pa_config.fan_mode = FAN_MODE_MED;
		break;
	case FAN_MODE_MED:
		analogWrite(PIN_FAN_PWM, 255);
		digitalWrite(PIN_FAN_SWITCH, 1);
		pa_config.fan_mode = FAN_MODE_HIGH;
		break;
	case FAN_MODE_HIGH:
		analogWrite(PIN_FAN_PWM, 0);
		digitalWrite(PIN_FAN_SWITCH, 0);
		pa_config.fan_mode = FAN_MODE_OFF;
		break;
	}
	return true;
}

static void prot_trigger(prot_type_t type)
{
	prot_triggered = type;
	digitalWrite(PIN_PTT_OUT, LOW);
	digitalWrite(PIN_INHIBIT, HIGH);
	set_power(0);

	if (current_menu == main_menu)
		lcd_printf_button(7, 0, "FAIL");
	delay(50);
	digitalWrite(PIN_INHIBIT, LOW);
}

static bool show_key(const struct menu_button *button)
{
	if (!button->pressed)
		return false;
	if (!button->stdby && !pa_config.pa_enabled)
		return false;
	return true;
}

static void activate_menu(const struct menu_button *menu)
{
	int width = 1024 / NUM_BUTTONS;

	current_menu = menu;

	for (int i = 0; i < NUM_BUTTONS; i++) {
		const struct menu_button *button = menu + i;
		int startx = i * width;
		lcd_drawSquareFill(startx + 1, 501, startx + width - 1, 599,
				   show_key(button) ? COLOR65K_GRAYSCALE15 : COLOR65K_BLACK);
		if (show_key(button)) {
			if (button->text)
				lcd_printf_button(i, 1, button->text);
			if (button->update)
				button->update(i);
		}
	}
}

static bool band_menu_key_pressed(int button)
{
	switch (button) {
	case 0:
		pa_config.force_band = FORCE_BAND_AUTO;
		break;
	case 1:
		pa_config.force_band = FORCE_BAND_160;
		break;
	case 2:
		pa_config.force_band = FORCE_BAND_80;
		break;
	case 3:
		pa_config.force_band = FORCE_BAND_6040;
		break;
	case 4:
		pa_config.force_band = FORCE_BAND_3020;
		break;
	case 5:
		pa_config.force_band = FORCE_BAND_1715;
		break;
	case 6:
		pa_config.force_band = FORCE_BAND_1210;
		break;
	case 7:
		pa_config.force_band = FORCE_BAND_6;
		break;
	}
	activate_menu(main_menu);
	return false;
}

static void check_temperature(void)
{
	unsigned long now = millis();
	static unsigned long last;
	static unsigned int state;
	int diff;

	switch (state) {
	case 0:
		sensors.requestTemperatures();
		state++;
		break;
	case 1:
		if (!sensors.isConversionComplete())
			break;
		state++;
		/* fallthrough */
	case 2:
		if (now - last < 1000)
			break;
		last = now;
		float temp = sensors.getTempCByIndex(0);
		lcd_printf(240, 0, COLOR65K_CYAN, COLOR65K_GRAYSCALE15, "%4.1fC", temp);

		if (pa_config.fan_mode == FAN_MODE_AUTO) {
			diff = temp - 25;
			if (diff < 0)
				diff = 0;
			if (diff > 50)
				diff = 50;
			analogWrite(PIN_FAN_PWM, 50 + diff * 4);
			digitalWrite(PIN_FAN_SWITCH, pa_config.pa_enabled || diff > 10);
		}

		if (temp > 50)
			prot_trigger(PROT_TEMPERATURE);
		state = 0;
		break;
	}
}

static void update_power(void)
{
	static float peakin_watt, peakin_dbm, peakout_watt, peakout_dbm;
	static uint32_t swr_filter_fail, swr_antenna_fail;
	static uint32_t last;

	uint32_t ms = millis();

	if (pwrswr_antenna.fwd_dBmpeak > peakout_dbm)
		peakout_dbm = pwrswr_antenna.fwd_dBmpeak;

	if (pwrswr_input.fwd_dBmpeak > peakin_dbm)
		peakin_dbm = pwrswr_input.fwd_dBmpeak;

	if (pwrswr_antenna.fwd_peakwatt > peakout_watt)
		peakout_watt = pwrswr_antenna.fwd_peakwatt;

	if (pwrswr_input.fwd_peakwatt > peakin_watt)
		peakin_watt = pwrswr_input.fwd_peakwatt;

	calc_PwrBridges(&cfgdata);

	update_bargraph_peak(20, 100, 900, 30, 30,
			     pwrswr_antenna.fwd_dBm,
			     peakout_dbm,
			     61,
			     peakout_watt,
			     "%4.0fW");

	update_bargraph_peak(20, 225, 400, 30, 30,
			     pwrswr_antenna.rev_dBm,
			     pwrswr_antenna.rev_dBm,
			     53,
			     pwrswr_antenna.rev_watt,
			     "%4.1fW");

	update_bargraph_peak(525, 225, 400, 30, 1,
			     scale_swr(pwrswr_antenna.swr),
			     scale_swr(pwrswr_antenna.swr),
			     100,
			     pwrswr_antenna.swr,
			     " %4.2f");

//	update_bargraph_peak(561, 300, 324, 30, MIN_DBM,
//			     pwrswr_filter.fwd_dBm,
//			     pwrswr_filter.fwd_dBmpeak,
//			     MAX_DBM_PWR,
//			     pwrswr_filter.fwd_peakwatt,
//			     "%6.2fW");

//	update_bargraph(560, 350, 324, 30, 0, pwrswr_filter.rev_watt, 500, "%6.2fW");
//	update_bargraph(560, 400, 324, 30, 1, pwrswr_filter.swr, 3, " %6.2f");

	update_bargraph_peak(20, 350, 400, 30, 30,
			     pwrswr_input.fwd_dBm,
			     peakin_dbm,
			     53,
			     peakin_watt,
			     "%4.1fW");

	if (pwrswr_filter.swr > 5 && pwrswr_filter.rev_watt > 20) {
		if (ms - swr_filter_fail > 250)
			prot_trigger(PROT_FILTER_SWR);
	} else {
		swr_filter_fail = ms;
	}

	if (pwrswr_antenna.swr > 3 && pwrswr_antenna.rev_watt > 20) {
		if (ms - swr_antenna_fail > 25)
			prot_trigger(PROT_OUTPUT_SWR);
	} else {
		swr_antenna_fail = ms;
	}

	if (ms - last > 100) {
		peakout_dbm /= 1.1;
		peakin_dbm /= 1.1;
		peakout_watt /= 1.5;
		peakin_watt /= 1.5;
		last = ms;
	}
	lcd_printf(250, 465, COLOR65K_GREEN, COLOR65K_BLACK, "FSWR %4.2f FPWR %4.0fW FREV %4.0fW  ",
		   pwrswr_filter.swr, pwrswr_filter.fwd_peakwatt, pwrswr_filter.rev_watt);
}

void TC3_Handler(void)
{
	static int cnt;

	TC_GetStatus(TC1, 0);
	if (cnt++ < 64)
		return;
	cnt = 0;

	tacho1 = 0;
	tacho2 = 0;
}

static uint8_t csum_config(struct pa_config *config)
{
	uint8_t *p = (uint8_t *)config;
	uint8_t csum = 0;

	for (unsigned int i = 0; i < sizeof(struct pa_config); i++)
		csum += p[i];
	return csum;
}

static void update_config(void)
{
	pa_config.csum = 0;
	pa_config.csum = -csum_config(&pa_config);
	eeprom_write(0, (uint8_t *)&pa_config, sizeof(pa_config));
}

void setup()
{
	setup_gpio();
	setup_emb();
	setup_lcd();
	set_power(0);
	setup_timer();
	setup_touch();
	can_setup();
	can_request_ps_status();
	setup_ibus();
	select_antenna(0);
	analogReadResolution(12);

	activate_menu(main_menu);

	// TC3 needs to have a lower priority than tacho*_isr to
	// prevent races
	NVIC_SetPriority(TC3_IRQn, 15);
	setup_adc();

	pwrswr_antenna.swr = 1;
	pwrswr_filter.swr = 1;
	pwrswr_input.swr = 1;

	int ret = eeprom_read(0, (uint8_t *)&pa_config, sizeof(pa_config));
	if (ret != sizeof(pa_config) || csum_config(&pa_config)) {
		memset(&pa_config, 0, sizeof(pa_config));
		update_config();
	}

	if (pa_config.pa_enabled)
		draw_bargraphs();
	activate_menu(main_menu);
	set_power(pa_config.pa_enabled);
}

static void touch_loop(void)
{
	static int last_state = TOUCH_STATE_NONE, current_state;
	int x, y, button, ret;
	uint8_t regs[8];

	noInterrupts();
	if (!touch_int_pending) {
		interrupts();
		return;
	}
	touch_int_pending = 0;
	interrupts();

	if (digitalRead(PIN_TOUCH_INT) == HIGH)
		return;

	ret = readFT5316TouchAddr(0x02, regs, 6);
	if (ret != 6)
		return;

	if ((regs[0] & 0x0f) < 1) // more than one point?
		return;

	current_state = regs[1] >> 6;
	if (current_state == last_state)
		return;
	last_state = current_state;

	x = 1023 - ((regs[1] & 0x0f) << 8 | regs[2]);
	y = 599 - ((regs[3] & 0x0f) << 8 | regs[4]);

	if (current_state != TOUCH_STATE_PRESS_DOWN)
		return;

	if (y < 520) // outsize button area
		return;

	button = x / (1024 / NUM_BUTTONS);
	if (button < 0 || button > NUM_BUTTONS - 1)
		return;

	const struct menu_button *b = current_menu + button;
	if (!pa_config.pa_enabled && !b->stdby)
		return;
	if (b->pressed) {
		if (b->pressed(button) && b->update)
			b->update(button);
		update_config();
	}
}

#define MAX(a, b) ((a) > (b) ? (a) : (b))
static void ps_loop(void)
{
	static unsigned long last;
	unsigned long now = millis();

	switch (psu_state) {
	case PSU_WAIT_INITIAL:
		status_display(COLOR65K_YELLOW, "PSU init");
		if (now > 5000 && can_ps_get_last_rx() < 500)
			psu_state = PSU_VOLTAGE_SET;
		break;

	case PSU_VOLTAGE_SET:
		status_display(COLOR65K_YELLOW, "PSU set voltage");
		can_ps_set(R4850G2_SET_ONLINE_OUTPUT_VOLTAGE, pa_config.pa_high_power ? 53 : 42);
		psu_state = PSU_VOLTAGE_WAIT;
		break;

	case PSU_VOLTAGE_WAIT:
		if (can_set_response_status() == CAN_STATUS_OK)
			psu_state = PSU_CURRENT_SET;
		else
			psu_state = PSU_WAIT_INITIAL;
		break;

	case PSU_CURRENT_SET:
		status_display(COLOR65K_YELLOW, "PSU set current");
		can_ps_set(R4850G2_SET_ONLINE_CURRENT_LIMIT, 35);
		psu_state = PSU_CURRENT_WAIT;
		break;

	case PSU_CURRENT_WAIT:
		if (can_set_response_status() == CAN_STATUS_OK) {
			status_display(COLOR65K_YELLOW, "");
			psu_state = PSU_NORMAL;
		} else {
			psu_state = PSU_WAIT_INITIAL;
		}
		break;

	case PSU_NORMAL:
		if (pa_config.pa_enabled && now - last > 125) {
			float temp = MAX(ps_status.output_temperature, ps_status.input_temperature);
			lcd_printf(0, 465, COLOR65K_WHITE, COLOR65K_BLACK, "PSU %4.1fC", temp);
			lcd_printf(160, 465, COLOR65K_WHITE, COLOR65K_BLACK, "%4.1fV", ps_status.output_voltage);
			update_bargraph(525, 350, 400, 30, 0, ps_status.output_current, 40, "%4.1fA");
			can_request_ps_status();
			last = now;
		}

		if (can_ps_get_last_rx() > 5000) {
			printf("last rx %ld ms, timeout\n", can_ps_get_last_rx());
			prot_trigger(PROT_PSU_TIMEOUT);
			psu_state = PSU_WAIT_INITIAL;
		}
		break;
	}
}

static void update_status(void)
{
	static int last_update;

	if (prot_triggered != PROT_NONE) {
		status_display(COLOR65K_RED, prot_names[prot_triggered]);
		return;
	}

	if (pa_config.pa_enabled && millis() - last_update > 50) {
		last_update = millis();
		update_power();
	}
}

static int get_ptt_input(void)
{
	if (!pa_config.pa_enabled || pa_not_ready() || !pa_config.ptt_enabled)
		return false;
	return digitalRead(PIN_PTT_IN) ? 0 : 1;
}

static void ptt_loop(void)
{
	static ptt_state_t ptt_state;
	static int debounce, lastcheck, ms;

	ms = millis();

	if (ms - lastcheck < 3)
		return;
	lastcheck = ms;

	noInterrupts();

	switch (ptt_state) {
	case PTT_RESET:
		digitalWrite(PIN_PTT_OUT, LOW);
		digitalWrite(PIN_INHIBIT, LOW);
		ptt_state = PTT_OFF;
		break;

	case PTT_OFF:
		if (!get_ptt_input()) {
			digitalWrite(PIN_INHIBIT, LOW);
			debounce = 0;
			break;
		}

		if (debounce++ > 5) {
			lcd_printf(990, 0, COLOR65K_WHITE, COLOR65K_RED, "TX");
			digitalWrite(PIN_INHIBIT, HIGH);
			ptt_state = PTT_CHANGE_ON;
		}
		break;

	case PTT_CHANGE_ON:
		digitalWrite(PIN_PTT_OUT, HIGH);
		ptt_state = PTT_ON;
		break;

	case PTT_ON:
		if (get_ptt_input()) {
			digitalWrite(PIN_INHIBIT, LOW);
			debounce = 0;
			break;
		}

		if (debounce++ > 5) {
			digitalWrite(PIN_INHIBIT, HIGH);
			ptt_state = PTT_CHANGE_OFF;
		}
		break;

	case PTT_CHANGE_OFF:
		ptt_state = PTT_OFF;
		digitalWrite(PIN_PTT_OUT, LOW);
		lcd_printf(990, 0, COLOR65K_GREEN, COLOR65K_GRAYSCALE15, "RX");
		break;
	};
	interrupts();
}

static void prot_current_loop(void)
{
	static int prot_current_ctr;

	if (digitalRead(PIN_PROT_CURRENT)) {
		if (prot_current_ctr++ > 100)
			prot_trigger(PROT_CURRENT);
		return;
	}
	prot_current_ctr = 0;

}

void loop()
{
	ps_loop();
	touch_loop();
	can_loop();
	check_temperature();
	update_status();
	set_band_outputs();
	ptt_loop();
	prot_current_loop();
}

void BusFault_Handler(void)
{
	lcd_printf(0, 200, COLOR65K_RED, COLOR65K_BLACK, "Bus Fault");
	for(;;);
}

void HardFault_Handler(void)
{
	lcd_printf(0, 200, COLOR65K_RED, COLOR65K_BLACK, "Hard Fault");
	for(;;);
}
