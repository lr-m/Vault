#include <Adafruit_GFX.h> // core graphics library
#include <Adafruit_ST7735.h> // hardware-specific library
#include <SPI.h>
#include <IRremote.h>
#include <sha256.h>
#include <AES.h>
#include <string.h>
#include <ST7735_PW_Keyboard.h>
#include <ST7735_PW_UI.h>
#include <EEPROM.h>
#include <colours.h>
#include <EEPROM_Manager.h>

// Define PIN config
#define TFT_SCL   3
#define RECV_PIN  D4
#define TFT_RES   D7
#define TFT_CS    D5
#define TFT_DC    D6
#define TFT_SDA   D8

#define MAX_PASSWORD_LENGTH 32

AES128 aes128; // For aes encryption

char* password;
int password_length;

boolean master_key_succeed = false;

// For ST7735-based displays, we will use this call
Adafruit_ST7735 tft =
    Adafruit_ST7735(TFT_CS, TFT_DC, TFT_SDA, TFT_SCL, TFT_RES);

ST7735_PW_Menu* menu;
Password_Manager* password_manager;
Wallet_Manager* wallet_manager;
EEPROM_Manager* eeprom_manager;

IRrecv irrecv(RECV_PIN);

// Instantiate keyboard
ST7735_PW_Keyboard *keyboard;

boolean perform_setup = false;

void setup(){
  EEPROM.begin(512);

  // Initialise keyboard
  keyboard = (ST7735_PW_Keyboard *)malloc(sizeof(ST7735_PW_Keyboard));
  *keyboard = ST7735_PW_Keyboard(&tft);

  // Initialise menu
  menu = (ST7735_PW_Menu*) malloc(sizeof(ST7735_PW_Menu));
  *menu = ST7735_PW_Menu(&tft);

  // Initialise external eeprom manager
  eeprom_manager = (EEPROM_Manager*) malloc(sizeof(EEPROM_Manager));
  *eeprom_manager = EEPROM_Manager();

  // Initialise password manager
  password_manager = (Password_Manager*) malloc(sizeof(Password_Manager));
  *password_manager = Password_Manager(&tft, &aes128, keyboard, eeprom_manager);

  // Initialise wallet manager
  wallet_manager = (Wallet_Manager*) malloc(sizeof(Wallet_Manager));
  *wallet_manager = Wallet_Manager(&tft, &aes128, keyboard, eeprom_manager);

  password = (char*) malloc(sizeof(char) * MAX_PASSWORD_LENGTH);

  // For clearing the master password
//  EEPROM.write(0, 0);
//  EEPROM.commit();

  // EEPROM magic number is 43 53 50 82 66
  if (EEPROM.read(0) != byte(45) || EEPROM.read(1) != byte(53) || 
      EEPROM.read(2) != byte(50) || EEPROM.read(3) != byte(82) || 
      EEPROM.read(4) != byte(66)){
    perform_setup = true;
  }
  
  irrecv.enableIRIn(); // Enable IR reciever

  // For clearing the saved credentials
//  for (int i = 0; i < 2000; i++){
//    eeprom_manager->writeExternalEEPROM(i, 255);
//  }
  
  tft.initR(INITR_BLACKTAB); // Init ST7735S chip
  tft.fillScreen (SCHEME_BG);
  tft.setTextColor(ST77XX_GREEN);

  tft.fillScreen(SCHEME_BG);
  if (perform_setup){
    keyboard->displayPrompt("Enter new master key:");
  } else {
    keyboard->displayPrompt("Enter master key:");
  }
  
  keyboard->display();
}

void loop(){
  // Stage 1: Password input (uses the sha256 hash on the EEPROM)
  if (!master_key_succeed){
    if (irrecv.decode()) {
      if (irrecv.decodedIRData.decodedRawData == 0xE619FF00){
        EEPROM.write(0, 0);
        EEPROM.commit();
        ESP.restart();
      }

      if (irrecv.decodedIRData.decodedRawData == 0xF20DFF00) {
        ESP.restart();
      }

      // Need to limit keyboard input for password to 24
      keyboard->interact(&irrecv.decodedIRData.decodedRawData);
      irrecv.resume();
    }

    if (keyboard->enterPressed() == 1){
      if (perform_setup){
        writePasswordToEEPROM(keyboard->getCurrentInput(), keyboard->getCurrentInputLength());
        ESP.restart();
      } else {
        // Compare hash of entered passcode against the stored hash
        if (entryCheck(keyboard->getCurrentInput(), keyboard->getCurrentInputLength())){
          tft.setTextColor(ST77XX_GREEN);
          tft.print("\n\nEntry Granted");
          master_key_succeed = true;
          delay(1000);

          // Load password into heap for further encryption/decryption
          int i = 0;
          for (; i < keyboard->getCurrentInputLength(); i++){
            password[i] = keyboard->getCurrentInput()[i];
          }

          if (i > 0)
            password[i] = '\0';

          password_length = keyboard->getCurrentInputLength();

          password_manager->setKey(password); // This also sets the key for the wallet manager as they both use the same AES instance

          loadCredentialsFromEEPROM();

          tft.fillScreen(SCHEME_BG);

          menu->display();
        } else {
          // Refuse entry and let user try again
          tft.setTextColor(ST77XX_RED);
          tft.print("\n\nEntry Refused");
          delay(1000);
          tft.fillScreen(SCHEME_BG);
          keyboard->reset();
          keyboard->displayPrompt("Try again:");
          keyboard->display();
        }
      }
    }
    
    return;
  }

  // Menu interaction
  if (irrecv.decode()) {
     menu->interact(&irrecv.decodedIRData.decodedRawData, password_manager, wallet_manager);
     irrecv.resume();
   }
  
  delay(100);
}

// Performs the sha256 hash
void sha256Gen(const char* data, int input_size, BYTE* hash_buffer){
  Sha256* sha256Instance=new Sha256();
  sha256Instance->update((const BYTE*) data, input_size);
  sha256Instance->final(hash_buffer);

  delete sha256Instance;

  tft.setTextColor(ST77XX_GREEN);
  tft.print("\n\nSHA256: ");
  tft.setTextColor(ST77XX_RED);
  
  for (int i = 0; i < SHA256_BLOCK_SIZE; i++){
    tft.print(hash_buffer[i], HEX);
  }
}

// Checks if the hash of the passed password matches that stored in EEPROM
boolean entryCheck(char* password, int entry_length){
  BYTE saved_hash[SHA256_BLOCK_SIZE];
  loadHashFromEEPROM(saved_hash);
  
  BYTE hash[SHA256_BLOCK_SIZE];
  sha256Gen(password, entry_length, hash);

  for (int i = 0; i < SHA256_BLOCK_SIZE; i++){
    if (saved_hash[i] != hash[i]){
      return false;
    }
  }
  return true;
}

// Loads the existing sha256 hash from the EEPROM
void loadHashFromEEPROM(BYTE* hash){
  for (int i = 5; i < 5 + SHA256_BLOCK_SIZE; i++){
    hash[i-5] = (char) EEPROM.read(i);
  }
}

// Writes new master password to EEPROM
void writePasswordToEEPROM(char* password, int entry_length){
  // Write magic phrase
  EEPROM.write(0, 45);
  EEPROM.write(1, 53);
  EEPROM.write(2, 50);
  EEPROM.write(3, 82);
  EEPROM.write(4, 66);

  BYTE hash[SHA256_BLOCK_SIZE];
  sha256Gen(password, entry_length, hash);

  // Write password hash
  int eeprom_address = 5;
  for (int i = 0; i < SHA256_BLOCK_SIZE; i++){
    EEPROM.write(eeprom_address, hash[i]);
    eeprom_address++;
  }

  EEPROM.write(eeprom_address, '\0');

  EEPROM.commit();
}

// Prints internal EEPROM contents
void printEEPROM(){
  Serial.print("EEPROM State:");
  int limit = 0;
  for (int i = 0; i < 512; i++){
      if (i%32 == 0){
        Serial.println();
      }
    
      byte read_value = EEPROM.read(i);

      if (read_value < 10){
        Serial.print("  ");
      } else if (read_value < 100){
        Serial.print(' ');
      }
      
      Serial.print(read_value);

      Serial.print(' ');
  }
  Serial.println();
}

// Loads credentials from external i2c eeprom
void loadCredentialsFromEEPROM(){ 
  int count = eeprom_manager->readExternalEEPROM(0);

  if (count > 100){
    eeprom_manager->writeExternalEEPROM(0, 0);
    return;
  }

  int type;
  int size;
  int read_address = 1;
  for (int i = 0; i < count; i++){
    size = eeprom_manager->readExternalEEPROM(read_address);
    type = eeprom_manager->readExternalEEPROM(read_address+1);
    read_address+=2;

    if (type == 0){ // Password
      password_manager->load(read_address);
      read_address += size;
    } else if (type == 1){
      wallet_manager->load(read_address);
      read_address+= 32 + size*32;
    }
  }

  password_manager->sortEntries();
}
