#!/usr/bin/perl
use Digest::CRC qw(crc crc8 crc16);
# Адрес начала блока с будильниками.
$base_for_alarm = 3 * hex '0x20';
# Размер одной записи будильника
$record_size = 16;

# Время последнего срабатывания всегда 0;
$last_alarm_time = '0000000000';
print `stty 38400 -F /dev/ttyUSB0`;
open(PORT, ">/dev/ttyUSB0");

open DB, "<regular_alarm.csv";
while(<DB>) {
  if (!(substr($_, 0, 1) eq '#')) {
    ($number, $type, $time, $task, $data1, $data2) = split "\t", $_;
    # Строка инициализции eeprom - AE -адрес чипа + 2 байта адрес ячеки памяти
    $eeprom_init = "AE" . sprintf("%04X", $number * $record_size + $base_for_alarm);

    # Определяем в какой системе исчисления записаны параметры задачи
    if (substr($data1, 0, 1) eq 'b') { $data1 = oct $data1; }
    else {if (substr($data1, 0, 2) eq '0x') {$data1 = hex $data1; }}
    if (substr($data2, 0, 1) eq 'b') {$data2 = oct $data2; }
    else {if (substr($data2, 0, 2) eq '0x') {$data2 = hex $data2; }}

    # Задача - номер функции + два параметра
    if ($task eq 'TOUCH_RELAY_A') {$task_str = '31'; }
    $task_str = $task_str . sprintf("%02X%02X", oct $data1, oct $data2);

    # Для ежедневного будильника время в фомате HH24:MM
    if ($type eq "daily") {
      $flags = sprintf("%02X", oct 'b10000110');
      ($hour, $minut) = split ":", $time;
      $alarm_time = sprintf("%02X%02X000000", $minut, $hour);
    }

    $alarm_crc = crc($alarm_time . $last_alarm_time . $flags . $task_str);

    $result_str = $eeprom_init . $alarm_time . $last_alarm_time . $flags . $task_str . $alarm_crc;
    # Определяем количество байт отправляемых в I2C шину
    $size = length($result_str)/2;
    $result_str = "I" . sprintf("%02X", $size) . $result_str;
    print "$result_str\n";
    print PORT "$result_str\n";
    sleep 1;
  }
}

sub crc {
  my @hex    = ($_[0] =~ /(..)/g);
  my @dec    = map { hex($_) } @hex;
  my @bytes  = map { pack('C', $_) } @dec;
  my $crc = 0;
  for($i = 0; $i < @bytes; $i++ ) {
    $crc = crc16($bytes[$i], $crc);
  }

  $crc = sprintf("%02X", $crc);
  my @hexx    = ($crc =~ /(..)/g);
  return "$hexx[1]$hexx[0]";
}