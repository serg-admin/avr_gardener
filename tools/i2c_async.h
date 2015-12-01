#include <util/twi.h>
#define I2C_STATE_FREE 0x00
#define I2C_STATE_SEND 0x01
#define I2C_STATE_RECIVE 0x02

unsigned char i2c_send(char addr, unsigned char* buf, unsigned char size, void (*callback)(unsigned char));
unsigned char i2c_recive( char addr, unsigned char* buf, unsigned char size, void (*callback)(unsigned char));
void i2c_init(void);