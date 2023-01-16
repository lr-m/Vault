#include "EEPROM_Manager.h"

EEPROM_Manager::EEPROM_Manager(){
    Wire.begin();
}

void EEPROM_Manager::fixBusted(){
  // Set the magic numbers
  this->writeExternalEEPROM(0, 45);
  this->writeExternalEEPROM(1, 53);
  this->writeExternalEEPROM(2, 50);
  this->writeExternalEEPROM(3, 82);
  this->writeExternalEEPROM(4, 66);

  // Init the password entry bitmasks
  for (int i = 0; i < MASK_BYTE_COUNT; i++){
    this->writeExternalEEPROM(PWD_BITMASK_START + i, 0);
    Serial.println(this->readExternalEEPROM(PWD_BITMASK_START + i));
  }

  this->writeExternalEEPROM(WALLET_COUNT_ADDRESS, 0);
}

void EEPROM_Manager::checkInit(){
  for (int i = 0; i < 37; i++){
    Serial.print(this->readExternalEEPROM(i));
    Serial.print(", ");
  }

  // this->writeExternalEEPROM(WALLET_COUNT_ADDRESS, 0);

  if (this->readExternalEEPROM(0) != byte(45) || this->readExternalEEPROM(1) != byte(53) || 
      this->readExternalEEPROM(2) != byte(50) || this->readExternalEEPROM(3) != byte(82) || 
      this->readExternalEEPROM(4) != byte(66)){
    this->init();
  }
}

void EEPROM_Manager::init(){
  this->wipe();

  // Set the magic numbers
  this->writeExternalEEPROM(0, 45);
  this->writeExternalEEPROM(1, 53);
  this->writeExternalEEPROM(2, 50);
  this->writeExternalEEPROM(3, 82);
  this->writeExternalEEPROM(4, 66);

  // Init the password entry bitmasks
  for (int i = 0; i < MASK_BYTE_COUNT; i++){
    this->writeExternalEEPROM(PWD_BITMASK_START + i, 0);
    Serial.println(this->readExternalEEPROM(PWD_BITMASK_START + i));
  }

  this->writeExternalEEPROM(WALLET_COUNT_ADDRESS, 0);
}

void EEPROM_Manager::wipe(){
  this->writeExternalEEPROM(0, 123);
  for (int i = 1; i < EEPROM_SIZE; i++){
    this->writeExternalEEPROM(i, 255);
    if (this->readExternalEEPROM(0) != 123){
      Serial.print("ERROR HERE: ");
      Serial.println(i);
    }
  }
  Serial.println("Done!");
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

// Returns the next available address for an entry
int EEPROM_Manager::getNextFreeAddress(){
  // Check over all the bytes to see if there are any free blocks
  for (int i = 0; i < MASK_BYTE_COUNT; i++){
    // Get the byte that stores the address
    byte mask_byte = this->readExternalEEPROM(PWD_BITMASK_START + i);
    for (int j = 0; j < 8; j++){
      // Check each bit to see if it is free
      if (bitRead(mask_byte, j) == 0){
        // At this point, safe to assume that entry will fill this block, so write it to 1
        bitSet(mask_byte, j);
        this->writeExternalEEPROM(PWD_BITMASK_START + i, mask_byte);

        // Calculate and return the start address of the free block
        return PASSWORD_ENTRY_START + ((i*8 + j) * EEPROM_PW_ENTRY_SIZE);
      }
    }
  }
  return -1; // Indicates storage is full
}

int EEPROM_Manager::getNextFreeWalletAddress(){
  // Get number of wallet entries
  int count = this->readExternalEEPROM(WALLET_COUNT_ADDRESS);

  // Get next free address
  int write_address = WALLET_START_ADDRESS;
  int size; // Entry size in terms of phrases

  // Iterate over entries until end reached
  for (int i = 0; i < count; i++){
      size = this->readExternalEEPROM(write_address);

      // Increment address by count byte, phrases, and name
      write_address += 1 + (size*WALLET_MAX_PHRASE_SIZE) + WALLET_MAX_PHRASE_SIZE;
  }

  return write_address;
}

// Clears the EEPROM from the start_position by size
void EEPROM_Manager::deleteWalletEntry(int start_position, int size){
  int next_free = getNextFreeWalletAddress(); // Get end

  // Shift memory down
  for (int i = start_position + size; i < next_free; i++){
    byte byte_ahead = this->readExternalEEPROM(i);
    this->writeExternalEEPROM(i - size, byte_ahead);
  }

  // Clear new unused memory from the end
  for (int i = next_free - size; i < next_free; i++){
    this->writeExternalEEPROM(i, 255);
  }

  // Reduce the entry counter after deletion
  int current_count = this->readExternalEEPROM(WALLET_COUNT_ADDRESS);
  this->writeExternalEEPROM(WALLET_COUNT_ADDRESS, current_count-1);
}

// Clears the EEPROM from the start_position by size
void EEPROM_Manager::clear(int start_position, int size){
  for (int i = start_position; i < start_position + size; i++){
    this->writeExternalEEPROM(i, 255);
  }
}

void EEPROM_Manager::clearEntryBit(int position){
  // Get index in bitmask
  int index = (position - PASSWORD_ENTRY_START)/EEPROM_PW_ENTRY_SIZE;
  int byte_index = floor(index/8);
  int bit_index = index%8;

  int mask_byte = this->readExternalEEPROM(PWD_BITMASK_START + byte_index);
  bitClear(mask_byte, bit_index);
  this->writeExternalEEPROM(PWD_BITMASK_START + byte_index, mask_byte);
}
