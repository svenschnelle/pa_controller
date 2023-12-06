#include <stdint.h>
#include <Wire.h>

#define I2C_SLAVE_EEPROM 0x50

int eeprom_read(uint16_t addr, uint8_t *buf, uint8_t len)
{
	int i;

	Wire.beginTransmission(I2C_SLAVE_EEPROM);

	if (Wire.write(addr >> 8) != 1)
		return -1;
	if (Wire.write(addr & 0xff) != 1)
		return -1;
	if (Wire.endTransmission())
		return -2;
	if (Wire.requestFrom((uint8_t)I2C_SLAVE_EEPROM, (uint8_t)len, (uint8_t)true) != len)
		return -3;
	for (i = 0; i < len && Wire.available(); i++)
		buf[i] = Wire.read();
	return i;
}

int eeprom_write(uint16_t addr, uint8_t *buf, uint8_t len)
{
	Wire.beginTransmission(I2C_SLAVE_EEPROM);

	if (Wire.write(addr >> 8) != 1)
		return -1;

	if (Wire.write(addr & 0xff) != 1)
		return -1;

	if (Wire.write(buf, len) != len)
			return -1;

	Wire.endTransmission();
	return 0;
}
