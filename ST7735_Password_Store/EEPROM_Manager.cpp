#include "EEPROM_Manager.h"

EEPROM_Manager::EEPROM_Manager(){
    Wire.begin();
}

// Function to write to EEPROOM
void EEPROM_Manager::writeExternalEEPROM(int address, byte val)
{
  // Begin transmission to I2C EEPROM
  Wire.beginTransmission(EEPROM_I2C_ADDRESS);
 
  // Send memory address as two 8-bit bytes
  Wire.write((int)(address >> 8));   // MSB
  Wire.write((int)(address & 0xFF)); // LSB
 
  // Send data to be stored
  Wire.write(val);
 
  // End the transmission
  Wire.endTransmission();
 
  // Add 5ms delay for EEPROM
  delay(5);
}
 
// Function to read from EEPROM
byte EEPROM_Manager::readExternalEEPROM(int address)
{
  // Define byte for received data
  byte rcvData = 0xFF;
 
  // Begin transmission to I2C EEPROM
  Wire.beginTransmission(EEPROM_I2C_ADDRESS);
 
  // Send memory address as two 8-bit bytes
  Wire.write((int)(address >> 8));   // MSB
  Wire.write((int)(address & 0xFF)); // LSB
 
  // End the transmission
  Wire.endTransmission();
 
  // Request one byte of data at current memory address
  Wire.requestFrom(EEPROM_I2C_ADDRESS, 1);
 
  // Read the data and assign to variable
  rcvData =  Wire.read();
 
  // Return the data as function output
  return rcvData;
}

void EEPROM_Manager::loadCredentialsFromEEPROM(){

}
