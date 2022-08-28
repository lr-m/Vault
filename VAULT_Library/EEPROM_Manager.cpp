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

int EEPROM_Manager::getNextFreeAddress(int count){
  // Get next free address
  int write_address = 1;
  int size;
  int type;

  for (int i = 0; i < count; i++){
      size = this->readExternalEEPROM(write_address);
      type = this->readExternalEEPROM(write_address+1);

      if (type == 0){
        write_address+=(size+2);
      } else if (type == 1){
        write_address += 2+(32 + size*32);
      }
  }

  return write_address;
}

void EEPROM_Manager::deleteEntry(int start_position, int size){
  int next_free = getNextFreeAddress(readExternalEEPROM(0)); // Get end

  int write_address = start_position;

  // Shift memory down
  for (int source_address = start_position + size; source_address < next_free; source_address++){
    this->writeExternalEEPROM(write_address, readExternalEEPROM(source_address));
    write_address++;
  }

  // Clear new unused memory from the end
  for (; write_address < next_free; write_address++){
    this->writeExternalEEPROM(write_address, 255);
  }
}
