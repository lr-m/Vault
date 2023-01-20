#include <Arduino.h>
#include "Wire.h"
#include <EEPROM.h>

#define EEPROM_I2C_ADDRESS 0x50

#define PASSWORD_ENTRY_START 37 // After the magic and mask
#define EEPROM_PW_ENTRY_SIZE 96 // Each entry takes up 96 bytes (IN HEX)
#define MASK_BYTE_COUNT 32
#define PWD_BITMASK_START 5
#define EEPROM_SIZE 32600

#define WALLET_COUNT_ADDRESS 24614
#define WALLET_START_ADDRESS 24615
#define WALLET_MAX_PHRASE_SIZE 32

#define minval(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })

#define maxval(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

#ifndef EEPROM_Manager_H
#define EEPROM_Manager_H

class EEPROM_Manager{
    public:
        EEPROM_Manager();
        void writeExternalEEPROM(int, byte);
        byte readExternalEEPROM(int);
        void loadCredentialsFromEEPROM();
        int getNextFreeAddress();
        int getNextFreeWalletAddress();
        void deleteWalletEntry(int, int);
        void wipe();
        void init();
        void checkInit();
        void clear(int, int);
        void clearEntryBit(int);
        void fixBusted();
};

#endif