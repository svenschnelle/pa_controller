#include "touch.h"
#include <stdint.h>

#if 0
static uint8_t readFT5316TouchRegister(uint8_t reg, uint8_t *out)
{
	Wire.beginTransmission(I2C_SLAVE_TOUCH);
	Wire.write(reg);
	if (Wire.endTransmission())
		return -1;
	if (Wire.requestFrom(I2C_SLAVE_TOUCH, uint8_t(1)) != 1)
		return -1;
	if (!Wire.available())
		return -1;
	*out = Wire.read();
	return 0;
}

static int writeFT5316TouchRegister(uint8_t reg, uint8_t val)
{
	Wire.beginTransmission(I2C_SLAVE_TOUCH);
	if (Wire.write(reg) != 1)
		return -1;
	if (Wire.write(val) != 1)
		return -1;
	if (Wire.endTransmission())
		return -1;
	return 0;
}
#endif

int readFT5316TouchAddr(uint8_t regAddr, uint8_t *pBuf, uint8_t len)
{
	int i;

	Wire.beginTransmission(I2C_SLAVE_TOUCH);

	if (Wire.write(regAddr) != 1)
		return -1;
	if (Wire.endTransmission())
		return -2;
	if (Wire.requestFrom(I2C_SLAVE_TOUCH, len) != len)
		return -3;
	for (i = 0; i < len && Wire.available(); i++)
		pBuf[i] = Wire.read();
	return i;
}

void setup_touch()
{
	Wire.begin();
}
