#include <Adafruit_GFX.h> // core graphics library
#include <Adafruit_ST7735.h> // hardware-specific library
#include <SPI.h>
#include <IRremote.h>
#include <AES.h>
#include <SHA512.h>
#include <string.h>
#include <ST7735_PW_Keyboard.h>
#include <ST7735_PW_UI.h>
#include <EEPROM.h>
#include <EEPROM_Manager.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <VAULT_IR_CODES.h>

#define INPUT_BUFF_SIZE 2048
#define RESPONSE_BUFF_SIZE 2048

// Define PIN config
#define TFT_SCL   3
#define RECV_PIN  D4
#define TFT_RES   D7
#define TFT_CS    D5
#define TFT_DC    D6
#define TFT_SDA   D8

#define MAX_PASSWORD_LENGTH 32

bool remote_mode = false;

extern unsigned char epd_bitmap_logo[];

AES128 aes128; // For aes encryption
SHA512 sha512;
#define BLOCK_SIZE 128
#define HASH_SIZE 64

char* password;
char* session_key;
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

const char* ssid = "T435C10";
const char* wifi_pwd =  "Th15WiFi1554f3";

WiFiServer wifiServer(2222);

DynamicJsonDocument resp_doc(RESPONSE_BUFF_SIZE);
char resp_buffer[RESPONSE_BUFF_SIZE];

DynamicJsonDocument inp_doc(INPUT_BUFF_SIZE);
char inp_buffer[INPUT_BUFF_SIZE];

void setup(){
  EEPROM.begin(512);
  Serial.begin(115200);

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

//  // For clearing the master password
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
  // eeprom_manager->init();
  eeprom_manager->checkInit();
  // eeprom_manager->fixBusted();
  
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
       if (irrecv.decodedIRData.decodedRawData == IR_HASHTAG) {
        eeprom_manager->fixBusted();
        ESP.restart();
       }

      if (irrecv.decodedIRData.decodedRawData == IR_ZERO){
        EEPROM.write(0, 0);
        EEPROM.commit();
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

          tft.fillRoundRect(40, 15, 36, 30, 8, ST77XX_WHITE);
          drawBitmap(0, 0, epd_bitmap_logo, 128, 64, SCHEME_MAIN);

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

  // If in remote mode, handle requests
  if (remote_mode){
    if (irrecv.decode()) {
      if (irrecv.decodedIRData.decodedRawData == IR_ASTERISK){
        remote_mode = false;
        wifiServer.stop();
        wifiServer.close();
        WiFi.disconnect();
        
        tft.fillScreen(SCHEME_BG);
  
        tft.fillRoundRect(40, 15, 36, 30, 8, ST77XX_WHITE);
        drawBitmap(0, 0, epd_bitmap_logo, 128, 64, SCHEME_MAIN);
        
        menu->display();
      }
      irrecv.resume();
    }
    
    // Wifi stuff
    WiFiClient client = wifiServer.available();

    if (client) {
      int i = 0;
      if (client.connected()) {
        while (client.available() > 0 && i < INPUT_BUFF_SIZE - 1) {
          char c = client.read();
          inp_buffer[i] = c;
          i++;
        }
        inp_buffer[i] = '\0';

        handleSocketRequest(client, inp_buffer);
      }

      client.stop();
      Serial.println("\nClient disconnected");
    }
  } else {
    // Menu interaction
    if (irrecv.decode()) {
      if (irrecv.decodedIRData.decodedRawData == IR_ASTERISK){
          remote_mode = true;
          tft.setTextColor(ST77XX_WHITE);
          tft.fillScreen(SCHEME_BG);
          tft.setCursor(0, 5);
          tft.print("Connecting");

          // wifi setup
          WiFi.begin(ssid, wifi_pwd);
          while (WiFi.status() != WL_CONNECTED) {
            delay(1000);
            tft.print('.');
          }
          tft.println();
         
          wifiServer.begin();

          displayRemoteInfo();
      } else if (menu->interact(&irrecv.decodedIRData.decodedRawData, password_manager, wallet_manager) == 1){
          tft.fillScreen(SCHEME_BG);
  
          tft.fillRoundRect(40, 15, 36, 30, 8, ST77XX_WHITE);
          drawBitmap(0, 0, epd_bitmap_logo, 128, 64, SCHEME_MAIN);
  
          menu->display();
       }
       irrecv.resume();
     }
  }
  
  delay(100);
}

int checkAuth(const char* password){
  uint8_t saved_hash[HASH_SIZE];
  loadHashFromEEPROM(saved_hash);

  Serial.print("Free heap: ");
  Serial.println(ESP.getFreeHeap());

  Serial.print("Incoming password hash: ");
  Serial.println(password);
  Serial.print("Saved password hash: ");
  for(int i = 0; i < HASH_SIZE; i++){
    Serial.print(saved_hash[i], HEX);
  }
  Serial.println();

  for(int i = 0; i < HASH_SIZE; i++){
    uint8_t saved = saved_hash[i];

    char upper = hexCharValue(password[i*2]);
    char lower = hexCharValue(password[i*2+1]);
    
    uint8_t seen = (uint8_t) (upper << 4 | lower);

    if (saved != seen){
      Serial.println("Authentication Failed");
      return -1;
    }
  }

  Serial.println("Authentication succeed");
  return 0;
}

void handleSocketRequest(WiFiClient client, char* incoming){

  // Create and deserialise incoming json
  deserializeJson(inp_doc, incoming);

  // Check that a master password has been provided
  if (inp_doc["master"].isNull())
    return;

  const char* password = inp_doc["master"];

  // Check the incoming password
  if (checkAuth(password) != 0)
    return;

  // Check the message has a type
  if (inp_doc["type"].isNull())
    return;

  // Pass to the respective handlers
  uint8_t type = (uint8_t) inp_doc["type"];
  switch(type){
    case 0:
      Serial.println("Reading password entry");
      if (!inp_doc["name"].isNull()){
        delay(random(250, 500));
        if (handlePasswordRead(inp_doc["name"], resp_buffer) == 0){
          client.print(resp_buffer);
        }
      }
      break;
    case 1:
      Serial.println("Adding password entry");
      if (!inp_doc["name"].isNull() && !inp_doc["user"].isNull() && !inp_doc["pwd"].isNull()){
        delay(random(250, 500));
        if (handlePasswordWrite(inp_doc["name"], inp_doc["user"], inp_doc["pwd"], resp_buffer) == 0){
          client.print(resp_buffer);
        }
      }
      break;
    case 2:
      Serial.println("Editing password entry");
      if (!inp_doc["name"].isNull()){
        delay(random(250, 500));
        const char* new_name = NULL;
        if (!inp_doc["new_name"].isNull())
          new_name = inp_doc["new_name"];

        const char* new_user = NULL;
        if (!inp_doc["new_user"].isNull())
          new_user = inp_doc["new_user"];

        const char* new_pwd = NULL;
        if (!inp_doc["new_pwd"].isNull())
          new_pwd = inp_doc["new_pwd"];
          
        if (handlePasswordEdit(inp_doc["name"], new_name, new_user, new_pwd, resp_buffer) == 0){
          client.print(resp_buffer);
        }
      }
      break;
    case 3:
      Serial.println("Deleting password entry");
      if (!inp_doc["name"].isNull()){
        delay(random(250, 500));
        if (handlePasswordRemove(inp_doc["name"], resp_buffer) == 0){
          client.print(resp_buffer);
        }
      }
      break;
    case 4:
      Serial.println("Reading wallet entry");
      if (!inp_doc["name"].isNull()){
        delay(random(250, 500));
        if (handleWalletRead(inp_doc["name"], resp_buffer) == 0){
          client.print(resp_buffer);
        }
      }
      break;
    case 5:
      Serial.println("Adding wallet entry");
      break;
    case 6:
      Serial.println("Deleting wallet entry");
      break;
    default:
      break;
  }

  inp_doc.clear();
}

// Handles remote password removal
int handlePasswordRemove(const char* name, char* resp_buffer){
  int response = password_manager->deleteEntry(name);

  resp_doc["response"] = response;

  serializeJson(resp_doc, resp_buffer, RESPONSE_BUFF_SIZE);

  resp_doc.clear();

  return response;
}

// Handles remote password information request
int handlePasswordRead(const char* name, char* resp_buffer)
{
  Password_Entry* entry = password_manager->getEntry(name);

  if (entry == NULL)
    return -1;
  
  char encryptedUsername [65];

  bytesToHexString(entry->getEncryptedEmail(), encryptedUsername);
  resp_doc["username"] = encryptedUsername;

  char encryptedPassword [65];
  bytesToHexString(entry->getEncryptedPassword(), encryptedPassword);
  resp_doc["password"] = encryptedPassword;

  serializeJson(resp_doc, resp_buffer, RESPONSE_BUFF_SIZE);

  resp_doc.clear();

  return 0;
}

// Handles remote password write
int handlePasswordWrite(const char* name, const char* user, const char* pwd, char* resp_buffer){
  int response = password_manager->addEntry(name, user, pwd);

  resp_doc["response"] = response;

  serializeJson(resp_doc, resp_buffer, RESPONSE_BUFF_SIZE);

  resp_doc.clear();
  
  return response;
}

// Handles remote password write
int handlePasswordEdit(const char* old_name, const char* name, const char* user, const char* pwd, char* resp_buffer){
  Serial.println("Handling password edit");

  int response = password_manager->editEntry(old_name, name, user, pwd);

  resp_doc["response"] = response;

  serializeJson(resp_doc, resp_buffer, RESPONSE_BUFF_SIZE);

  Serial.println(response);

  resp_doc.clear();
  
  return response;
}

// Handles remote password information request
int handleWalletRead(const char* name, char* resp_buffer)
{
  Wallet_Entry* entry = wallet_manager->getEntry(name);
  if (entry == NULL)
    return -1;

  JsonArray arr = resp_doc.to<JsonArray>();
  
  for (int i = 0; i < entry->phrase_count; i++){
    Serial.println("Constructing entry");
    byte tmp[WALLET_MAX_PHRASE_SIZE];
    for (int j = 0; j < WALLET_MAX_PHRASE_SIZE; j++){
      tmp[j] = eeprom_manager -> readExternalEEPROM(entry->start_address + WALLET_MAX_PHRASE_SIZE + 1 + (i * WALLET_MAX_PHRASE_SIZE) + j);
    }

    char to_hex [65];
    bytesToHexString(tmp, to_hex);
    arr.add(to_hex);
  }

  serializeJson(resp_doc, resp_buffer, RESPONSE_BUFF_SIZE);

  resp_doc.clear();

  return 0;
}

uint8_t hexCharValue(char character)
{
    character = character | 32;

    if ( character >= '0' && character <= '9' )
        return character - '0';

    if ( character >= 'a' && character <= 'f' )
        return ( character - 'a' ) + 10;

    return 0xFF;
}

void bytesToHexString(byte bytes[], char hexString[]) {
  for (int i = 0; i < 32; i++) {
    sprintf(hexString + i * 2, "%02X", bytes[i]);
  }
}

// Displays the remote mode info on the display
void displayRemoteInfo(){
  tft.println("\nRemote Info:\n");
  tft.println("IP Address:");
  tft.setTextColor(ST77XX_GREEN);
  tft.println(WiFi.localIP());
  tft.setTextColor(ST77XX_WHITE);

  // Generate random session key
  tft.println("\nSession Key:");
  tft.setTextColor(ST77XX_GREEN);

  session_key = (char*) malloc(sizeof(char)*13);
  session_key[12] = 0;
  generateSessionKey(session_key, 12);

  Serial.println("Generating session key");
  Serial.println(session_key);
  
  tft.println(session_key);
  tft.setTextColor(ST77XX_WHITE);
}

// Generates the key for the session
void generateSessionKey(char* key, int len){
  char* possGen = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ?!=@$%#+£&*><";
  for (int i = 0; i < len; i++){
    key[i] = possGen[random(74)];
  }
}

// Generates sha512
void sha512Gen(Hash *hash, char* data, int input_len, uint8_t* value)
{
    hash->reset();
    hash->update(data, input_len);
    hash->finalize(value, HASH_SIZE);
}

// Checks if the hash of the passed password matches that stored in EEPROM
boolean entryCheck(char* password, int entry_length){ 
  uint8_t saved_hash[HASH_SIZE];
  loadHashFromEEPROM(saved_hash);
  
  uint8_t hash[HASH_SIZE];
  sha512Gen(&sha512, password, entry_length, hash);

  delay(random(500)); // No timing attacks here ;)

  for (int i = 0; i < HASH_SIZE; i++){
    if (saved_hash[i] != hash[i]){
      return false;
    }
  }
  return true;
}

// Loads the existing sha256 hash from the EEPROM
void loadHashFromEEPROM(uint8_t* hash){
  for (int i = 5; i < 5 + HASH_SIZE; i++){
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

  uint8_t hash[HASH_SIZE];
  sha512Gen(&sha512, password, entry_length, hash);

  // Write password hash
  int eeprom_address = 5;
  for (int i = 0; i < HASH_SIZE; i++){
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
  // int count = eeprom_manager->readExternalEEPROM(0);

  // // if (count > 100){
  // //   eeprom_manager->writeExternalEEPROM(0, 0);
  // //   return;
  // // }

  // int type;
  // int size;
  // int read_address = 1;
  // for (int i = 0; i < count; i++){
  //   size = eeprom_manager->readExternalEEPROM(read_address);
  //   type = eeprom_manager->readExternalEEPROM(read_address+1);
  //   read_address+=2;

  //   if (type == 0){ // Password
  //     password_manager->load(read_address);
  //     read_address += size;
  //   } else if (type == 1){
  //     wallet_manager->load(read_address);
  //     read_address+= 32 + size*32;
  //   }
  // }

  // 

  // Load in the passwords
  int pwd_count = password_manager->load();
  int wallet_count = wallet_manager->load();

  password_manager->sortEntries();
}

/**
 * Draws a passed bitmap on the screen at the given position with the given
 * colour.
 */
void drawBitmap(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w,
                int16_t h, uint16_t color) {
  int16_t i, j, byteWidth = (w + 7) / 8;
  uint8_t byte;
  for (j = 0; j < h; j++) {
    for (i = 0; i < w; i++) {
      if (i & 7)
        byte <<= 1;
      else
        byte = pgm_read_byte(bitmap + j * byteWidth + i / 8);
      if (byte & 0x80)
        tft.drawPixel(x + i, y + j, color);
    }
  }
}
