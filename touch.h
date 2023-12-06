#ifndef PA_TOUCH_H
#define PA_TOUCH_H

#include <stdint.h>

#define I2C_SLAVE_TOUCH uint8_t(0x38)

void setup_touch(void);
int readFT5316TouchAddr(uint8_t regAddr, uint8_t *pBuf, uint8_t len);
#endif
