#include <algorithm>
#include <stdint.h>

#define ADC_BUFSIZ (6 * 256)
static volatile uint16_t adc_values[ADC_BUFSIZ];

enum _ADCVALS {
	UFWDIN = 2,
	UREVIN = 3,
	UFWDANT = 4,
	UREVANT = 5,
	UFWDFLT = 6,
	UREVFLT = 7,
};

const struct cfgdata cfgdata = {
	.ant = { .W_low = 0.355693, .W_high = 1200, .mV_low = 1240, .mV_high = 1925 },
	.flt = { .W_low = 0.343315, .W_high = 1200, .mV_low = 1230, .mV_high = 1925 },
	.drv = { .W_low = 0.0913134, .W_high = 100, .mV_low =  1089, .mV_high = 1725 },
};

struct pwrswr pwrswr_input;
struct pwrswr pwrswr_filter;
struct pwrswr pwrswr_antenna;

struct adc_data {
	float avg;
	float min;
	float max;
};

static struct adc_data adc_raw[16];

void setup_adc(void)
{
	memset((void *)&adc_values, 0, sizeof(adc_values));
	ADC->ADC_CHER =ADC_CHER_CH2 | ADC_CHER_CH3 | ADC_CHER_CH4 \
		      | ADC_CHER_CH5 | ADC_CHER_CH6 | ADC_CHER_CH7;
	ADC->ADC_IDR = ~0UL;
	ADC->ADC_IER = ADC_IER_ENDRX;
	ADC->ADC_RNPR = (uint32_t)adc_values;
	ADC->ADC_RNCR = ARRAY_SIZE(adc_values);
	ADC->ADC_EMR = ADC_EMR_TAG;
	NVIC_EnableIRQ(ADC_IRQn);
	ADC->ADC_MR = ADC_MR_LOWRES_BITS_12 | ADC_MR_TRACKTIM(8) | ADC_MR_PRESCAL(7) | \
		ADC_MR_SETTLING_AST17 | ADC_MR_STARTUP_SUT512 | ADC_MR_TRANSFER(3) | ADC_MR_TRGEN | ADC_MR_TRGSEL_ADC_TRIG3;
	ADC->ADC_PTCR = 1;

	// adc timer
	pmc_enable_periph_clk(TC2_IRQn);
	TC_Configure(TC0, 2, TC_CMR_WAVE | TC_CMR_WAVSEL_UP_RC | TC_CMR_ACPA_CLEAR | TC_CMR_ACPC_SET | TC_CMR_TCCLKS_TIMER_CLOCK2);
	uint32_t rc = VARIANT_MCK / 1048576;
	TC_SetRA(TC0, 2, rc / 2);
	TC_SetRC(TC0, 2, rc);
	TC_Start(TC0, 2);
	TC0->TC_CHANNEL[2].TC_IER = TC_IER_CPCS;
	TC0->TC_CHANNEL[2].TC_IDR = ~TC_IER_CPCS;
	NVIC_EnableIRQ(TC2_IRQn);
}

void TC2_Handler(void)
{
	TC_GetStatus(TC0, 2);
}

static float WtoDBM(float watts)
{
	return 10.0*log10(watts*1000.0);
}

static void handle_adc(void)
{
	volatile uint16_t *p = adc_values;
	uint32_t peaks[16][16];
	int peakidx[16];
	uint16_t counts[16];
	uint32_t avg[16];
	uint16_t min[16];
	uint16_t max[16];
	uint32_t peak;

	memset(&counts, 0, sizeof(counts));
	memset(&avg, 0, sizeof(avg));
	memset(&max, 0, sizeof(max));
	memset(&min, 0xff, sizeof(min));
	memset(peakidx, 0, sizeof(peakidx));
	memset(peaks, 0, sizeof(peaks));

	for (unsigned int i = 0; i < ADC_BUFSIZ; i++) {
		uint16_t raw = *p++;
		uint16_t val = raw & 0xfff;
		int channel = raw >> 12;

		avg[channel] += val;
		counts[channel]++;

		if (val < min[channel])
			min[channel] = val;

		peaks[channel][peakidx[channel]++] = val;
		peakidx[channel] &= ARRAY_SIZE(peaks[channel])-1;

		peak = 0;
		for (unsigned int j = 0; j < ARRAY_SIZE(peaks[channel]); j++)
			peak += peaks[channel][j];
		peak /= ARRAY_SIZE(peaks[channel]);

		if (peak > max[channel])
			max[channel] = peak;
	}

	for (int i = 2; i < 8; i++) {
		avg[i] /= counts[i];
		adc_raw[i].avg = (((float)avg[i] * 2325.0) / 4096.0);
		adc_raw[i].min = ((float)min[i] * 2325.0) / 4096.0;
		adc_raw[i].max = ((float)max[i] * 2325.0) / 4096.0;
	}

	ADC->ADC_RNPR = (uint32_t)adc_values;
	ADC->ADC_RNCR = ARRAY_SIZE(adc_values);
}

void ADC_Handler(void)
{
	if (ADC->ADC_ISR & ADC_ISR_ENDRX)
		handle_adc();
}

// Bilde Mittelwert über die letzten SWR Messungen
static float swrValueSmooth(float *swrsmoothbuf, float swr)
{
	for(int i=(SWRSMOOTHLEN-1); i>0; i--)
		swrsmoothbuf[i] = swrsmoothbuf[i-1];
	swrsmoothbuf[0] = swr;

	float midval = 0;
	for(int i = 0; i < SWRSMOOTHLEN; i++)
		midval += swrsmoothbuf[i];
	midval /= SWRSMOOTHLEN;
	return midval;
}

static void calc_psvals(struct pwrswr *vals,
			const struct pwrcfg *cfg,
			float fwdvolt, float peakfwdvolt,
			float revvolt)
{
	const float ref_low_dBm = WtoDBM(cfg->W_low);
	const float ref_high_dBm = WtoDBM(cfg->W_high);
	float ufwd, urev, fswr, stepmV_per_dB;

	stepmV_per_dB = (cfg->mV_high - cfg->mV_low) / (ref_high_dBm - ref_low_dBm);

	vals->fwd_dBm = ref_low_dBm + ((fwdvolt - cfg->mV_low) / stepmV_per_dB);
	vals->rev_dBm = ref_low_dBm + ((revvolt - cfg->mV_low) / stepmV_per_dB);
	vals->fwd_dBmpeak = ref_low_dBm + ((peakfwdvolt - cfg->mV_low) / stepmV_per_dB);

	vals->fwd_peakwatt = pow(10, vals->fwd_dBmpeak / 10.0) / 1000;
	vals->fwd_watt = pow(10, vals->fwd_dBm / 10.0) / 1000;
	vals->rev_watt = pow(10, vals->rev_dBm / 10.0) / 1000;

	// calculate SWR
	ufwd = sqrt(vals->fwd_watt * 50 * 1000);
	urev = sqrt(vals->rev_watt * 50 * 1000);
	if (ufwd > urev)
		fswr = (ufwd + urev) / (ufwd - urev);
	else
		fswr = 99;

	 if(fswr < 1.0001)
		 fswr = 1.0001;

	 if(vals->fwd_watt > 9999)
		 vals->fwd_watt = 9999;

	 if(vals->fwd_peakwatt > 9999)
		vals->fwd_peakwatt = 9999;

	 if(vals->fwd_watt < 0.5)
		 fswr = 1.0001;

	 vals->swr = swrValueSmooth(vals->smoothbuf, fswr);
}

void calc_PwrBridges(const struct cfgdata *cfg)
{
	struct adc_data raw[16];

	noInterrupts();
	memcpy(raw, &adc_raw, sizeof(raw));
	memset(adc_raw, 0, sizeof(adc_raw));
	interrupts();

	calc_psvals(&pwrswr_antenna, &cfg->ant,
		    raw[UFWDANT].avg, raw[UFWDANT].max, raw[UREVANT].avg);

	calc_psvals(&pwrswr_filter, &cfg->flt,
		    raw[UFWDFLT].avg, raw[UFWDFLT].max, raw[UREVFLT].avg);

	calc_psvals(&pwrswr_input, &cfg->drv,
		    raw[UFWDIN].avg, raw[UFWDIN].max, raw[UREVIN].avg);
}

static void swr_reset(float *swrsmoothbuf)
{
	for(int i = 0; i < SWRSMOOTHLEN; i++)
		swrsmoothbuf[i] = 1;
}

void swr_reset_all(void)
{
	swr_reset(pwrswr_antenna.smoothbuf);
	swr_reset(pwrswr_filter.smoothbuf);
	swr_reset(pwrswr_input.smoothbuf);
}
