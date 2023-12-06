#ifndef PA_CONTROLLER_ADC_H
#define PA_CONTROLLER_ADC_H

extern struct pwrswr pwrswr_input;
extern struct pwrswr pwrswr_filter;
extern struct pwrswr pwrswr_antenna;

#define SWRSMOOTHLEN 10

struct pwrswr {
	float fwd_dBm;			// gemessene Leistung in dBm
	float fwd_dBmpeak;		// gemessene Spitzenleistung in dBm
	float fwd_watt;			// gemessene Leistung in Watt
	float fwd_peakwatt;		// gemessene Spitzenleistung in Watt
	float rev_dBm;			// gemessene Leistung in dBm
	float rev_watt;			// gemessene Leistung in Watt
	float swr;			// das berechnete SWR
	float smoothbuf[SWRSMOOTHLEN];
};

struct pwrcfg {
	float W_low;
	float W_high;
	float mV_low;
	float mV_high;
};

struct cfgdata {
	struct pwrcfg ant;
	struct pwrcfg flt;
	struct pwrcfg drv;
};

extern const struct cfgdata cfgdata;

void setup_adc(void);
void calc_PwrBridges(struct cfgdata *cfg);
void swr_reset_all(void);
#endif
