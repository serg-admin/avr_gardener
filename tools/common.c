#include "common.h"
byte decToBcd(byte val){
  return ( (val/10*16) + (val%10) );
}

byte bcdToDec(byte val){
  return ( ((val >> 4)*10) + (val & 0x0F) );
}