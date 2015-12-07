#ifndef __common_h_
#define __common_h_
#define byte unsigned char
#include <util/crc16.h>

byte decToBcd(byte val);
byte bcdToDec(byte val);

uint16_t get_crc16(byte* arr, byte size);

#endif