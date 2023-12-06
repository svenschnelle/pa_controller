#ifndef PA_EEPROM_H
#define PA_EEPROM_H

int eeprom_read(uint16_t start, uint8_t *buf, uint8_t len);
int eeprom_write(uint16_t start, uint8_t *buf, uint8_t len);
#endif
