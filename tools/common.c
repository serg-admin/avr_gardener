#include "common.h"
byte decToBcd(byte val){
  return ( (val/10*16) + (val%10) );
}

byte bcdToDec(byte val){
  return ( ((val >> 4)*10) + (val & 0x0F) );
}

uint16_t get_crc16(byte* arr, byte size) {
  uint16_t c = 0;
  byte i = 0;
  for(i = 0; i < size; i++) {
    c = _crc16_update(c, arr[i]);
  }
  return c;
}