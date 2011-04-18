avrdude -c dragon_isp -P usb -p atmega328p -U eeprom:w:$1:i

