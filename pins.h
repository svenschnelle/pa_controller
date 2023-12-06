#ifndef PA_PINS_H
#define PA_PINS_H

// analog inputs
#define PIN_SWR_FILTER_REV 0
#define PIN_SWR_FILTER_FWD 1
#define PIN_SWR_OUTPUT_REV 2
#define PIN_SWR_OUTPUT_FWD 3
#define PIN_SWR_INPUT_REV 4
#define PIN_SWR_INPUT_FWD 5


// digital inputs
#define PIN_PTT_IN		22
#define PIN_BANDA		23
#define PIN_BANDB		24
#define PIN_BANDC		25
#define PIN_BANDD		26
#define PIN_INHIBIT		27
#define PIN_IBUS		28
#define PIN_PROT_CURRENT	29
#define PIN_PROT_SWR		30

// digital outputs
#define PIN_BAND_6		9
#define PIN_BAND_80		52
#define PIN_BAND_6040		51
#define PIN_BAND_3020		8
#define PIN_BAND_1715		49
#define PIN_BAND_1210		48
#define PIN_PTT_OUT		47
#define PIN_ANT1		46
#define PIN_ANT2		32
#define PIN_PROT_CLEAR		44
#define PIN_HV_POWER1		43
#define PIN_HV_POWER2		42

// LCD
#define PIN_LCD_CS		4
#define PIN_LCD_BL		2
#define PIN_LCD_RESET		53

// Touch controller
#define PIN_TOUCH_INT		33

// Fans
#define PIN_FAN_SWITCH		3
#define PIN_FAN_PWM		5
#define PIN_FAN_TACHO1		11
#define PIN_FAN_TACHO2		12

#define PIN_CAN_ENABLE		255

#endif
