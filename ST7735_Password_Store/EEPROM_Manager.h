#include <Arduino.h>
#include "Wire.h"
#include <EEPROM.h>

#define EEPROM_I2C_ADDRESS 0x50

#ifndef EEPROM_Manager_H
#define EEPROM_Manager_H

class EEPROM_Manager{
    public:
        EEPROM_Manager();
        void writeExternalEEPROM(int, byte);
        byte readExternalEEPROM(int);
        void loadCredentialsFromEEPROM();
};

#endif