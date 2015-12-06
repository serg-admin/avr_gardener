#include "zs042.h"

byte z042_next_byte(void) {
// Рестартуем шину и забираем следующий байт
  TWDR = DS3231_ADDRESS + 1;
  TWCR = _BV(TWINT) | _BV(TWSTA) | _BV(TWEN);
  while (!(TWCR & (1<<TWINT)));
  // TW_MR_SLA_ACK 0x10  ^^^
  TWCR = _BV(TWINT) | _BV(TWEN);
  while (!(TWCR & (1<<TWINT)));
  // TW_MR_SLA_ACK 0x40  ^^^
  TWCR = _BV(TWINT) | _BV(TWEN);
  while (!(TWCR & (1<<TWINT)));
  // TW_MR_DATA_NACK ^^^
  return TWDR;
}

void zs042_init_time(struct rec_data_time *time) {
  // Старт I2C для записи в RTC модуль
  TWCR = _BV(TWINT) | _BV(TWSTA) | _BV(TWEN);
  while (!(TWCR & (1<<TWINT)));
  TWDR = DS3231_ADDRESS;
  TWCR = (1<<TWINT) | (1<<TWEN);
  while (!(TWCR & (1<<TWINT)));
  // Устанавливаем курсов в RTC на регистр 0
  TWDR = 0x00;
  TWCR = (1<<TWINT) | (1<<TWEN);
  while (!(TWCR & (1<<TWINT)));

  // Считываем данные из RTC DS3231
  zs042_seconds = bcdToDec(z042_next_byte());
  time->minut = bcdToDec(z042_next_byte());
  time->hour = bcdToDec(z042_next_byte());
  time->dayOfWeek = bcdToDec(z042_next_byte());
  time->dayOfMonth = bcdToDec(z042_next_byte());
  time->month = bcdToDec(z042_next_byte());
  
  // Завершение I2C
  TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWSTO);
}