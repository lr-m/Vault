#include <Adafruit_GFX.h> // core graphics library
#include <Adafruit_ST7735.h> // hardware-specific library
#include <SPI.h>
#include <IRremote.h>
#include <AES.h>
#include <CTR.h>
#include <SHA512.h>
#include <string.h>
#include <ST7735_PW_Keyboard.h>
#include <ST7735_PW_UI.h>
#include <EEPROM.h>
#include <EEPROM_Manager.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <VAULT_IR_CODES.h>
#include "base64.hpp"

#define BUFF_SIZE 1840
#define MAX_BUFF_TXT_LEN 1376

// Define PIN config
#define RECV_PIN  2 // D4
#define TFT_SCL   3 // RX
#define TFT_DC    12 // D6
#define TFT_RES   13 // D7
#define TFT_CS    14 // D5
#define TFT_SDA   15 // D8

bool remote_mode = false;

extern unsigned char epd_bitmap_logo[];

AES128 aes128; // For aes encryption
SHA512 sha512;
#define BLOCK_SIZE 128
#define HASH_SIZE 64
#define WIFI_CREDS_START 100
#define MAX_MASTER_LEN 15
#define MAX_WIFI_CRED_LEN 24

char* password;
byte* session_key;
byte nonce[32];
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

int perform_setup = 3;

char ssid[32];
char wifi_pwd[32];

WiFiServer wifiServer(2222);

DynamicJsonDocument resp_doc(BUFF_SIZE);
char resp_buffer[BUFF_SIZE];

DynamicJsonDocument inp_doc(BUFF_SIZE);
char inp_buffer[BUFF_SIZE];

boolean expecting_cmd = false;

void setup(){
  EEPROM.begin(512);
  Serial.begin(115200);

  // Initialise keyboard
  keyboard = (ST7735_PW_Keyboard *)malloc(sizeof(ST7735_PW_Keyboard));
  *keyboard = ST7735_PW_Keyboard(&tft);

  keyboard->setLengthLimit(MAX_MASTER_LEN); // Set max master pwd len

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

  password = (char*) malloc(sizeof(char) * (MAX_MASTER_LEN + 1));

//  // For clearing the master password
//  EEPROM.write(0, 0);
//  EEPROM.commit();

  printEEPROM();

  // EEPROM magic number is 43 53 50 82 66
  if (EEPROM.read(0) != byte(45) || EEPROM.read(1) != byte(53) || 
      EEPROM.read(2) != byte(50) || EEPROM.read(3) != byte(82) || 
      EEPROM.read(4) != byte(66)){
    perform_setup = 0;
  }
  
  irrecv.enableIRIn(); // Enable IR reciever

  // For clearing the saved credentials
  // eeprom_manager->init();
  eeprom_manager->checkInit();
  // eeprom_manager->fixBusted();
  
  tft.initR(INITR_BLACKTAB); // Init ST7735S chip
  tft.fillScreen (SCHEME_BG);
  tft.setTextColor(SCHEME_MAIN);
  tft.fillScreen(SCHEME_BG);
  
  if (perform_setup == 0){
    keyboard->displayPrompt("Enter new master key:");
  } else {
    keyboard->displayPrompt("Enter master key:");
  }
  
  keyboard->display();
}

void loop(){  
  // Stage 1: Password input (uses the sha512 hash on the EEPROM)
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

      keyboard->interact(&irrecv.decodedIRData.decodedRawData);
      irrecv.resume();
    }

    if (keyboard->enterPressed() == 1){
      if (perform_setup == 0){
        // Save the entered password to the internal eeprom
        writeMasterPasswordToEEPROM(keyboard->getCurrentInput(), keyboard->getCurrentInputLength());

        // Copy into buffer for encrypting wifi creds
        for (int i = 0; i < keyboard->getCurrentInputLength(); i++){
          password[i] = keyboard->getCurrentInput()[i];
        }
        password[keyboard->getCurrentInputLength()] = 0;

        // Copy the password bytes and load into the aes128 cipher
        byte key_bytes[16];
        int j = 0;
        int copy_index = 0;

        while(j < 16){
            // Fill the key with more key material
            if (password[copy_index] == 0)
                copy_index = 0;

            key_bytes[j] = byte(password[copy_index]);
            j++;
            copy_index++;
        }

        // Set the key to the loaded key bytes
        aes128.setKey(key_bytes, aes128.keySize());

        perform_setup = 1;
        keyboard->reset();
        keyboard->displayPrompt("Enter WiFi SSID:");
        keyboard->display();
        keyboard->setLengthLimit(MAX_WIFI_CRED_LEN);
      } else if (perform_setup == 1){
        // Save the entered wifi ssid to internal eeprom (encrypted with master key)
        writeWiFiSSIDToEEPROM(keyboard->getCurrentInput(), keyboard->getCurrentInputLength());
        perform_setup = 2;
        keyboard->reset();
        keyboard->displayPrompt("Enter password:");
        keyboard->display();
        Serial.println(perform_setup);
      } else if (perform_setup == 2){
        // Save the entered wifi password to internal eeprom (encrypted with master key)
        writeWiFiPasswordToEEPROM(keyboard->getCurrentInput(), keyboard->getCurrentInputLength());
        perform_setup = 3;
        keyboard->reset();
        
        // Once all creds written, write the magic to indicate set up done
        writeMagicToEEPROM();
        ESP.restart();
      } else {
        // Compare hash of entered passcode against the stored hash
        if (entryCheck(keyboard->getCurrentInput(), keyboard->getCurrentInputLength())){
          // Indicate successful check
          tft.setTextColor(SCHEME_MAIN);
          tft.print("\n\nEntry Granted");
          master_key_succeed = true;
          delay(500);

          password_length = keyboard->getCurrentInputLength();

          // Load password into heap for further encryption/decryption
          int i = 0;
          for (; i < password_length; i++)
            password[i] = keyboard->getCurrentInput()[i];

          // Force null terminator
          if (i > 0)
            password[i] = '\0';

          // Copy the password bytes and load into the aes128 cipher
          byte key_bytes[16];
          int j = 0;
          int copy_index = 0;

          while(j < 16){
              // Fill the key with more key material
              if (password[copy_index] == 0)
                  copy_index = 0;

              key_bytes[j] = byte(password[copy_index]);
              j++;
              copy_index++;
          }

          // Set the key to the loaded key bytes
          aes128.setKey(key_bytes, aes128.keySize());

          // Load the passwords and wallets
          loadCredentialsFromEEPROM();

          // Draw the home screen
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
      if (irrecv.decodedIRData.decodedRawData == IR_HASHTAG){
        remote_mode = false;
        menu->setEntered(false);
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
      // Read available data from client
      if (client.connected()) {
        while (client.available() > 0 && i < BUFF_SIZE - 1) {
          char c = client.read();
          inp_buffer[i] = c;
          i++;
        }
        inp_buffer[i] = '\0';

        // Handle the socket request
        if (!expecting_cmd){
          handleAuth(client, inp_buffer);
        } else {
          handleCommand(client, inp_buffer, i);
        }
      }

      client.stop();
      Serial.println("\nClient disconnected");
    }
  } else {
    // Menu interaction
    if (irrecv.decode()) {
      char interact_return = menu->interact(&irrecv.decodedIRData.decodedRawData, password_manager, wallet_manager);
      if (interact_return == 2){
          remote_mode = true;

          // Load ssid from internal eeprom
          byte encrypted[32];
          for (int i = 0; i < 32; i++)
            encrypted[i] = EEPROM.read(WIFI_CREDS_START + i);

          for (int i = 0; i < 32; i += 16)
            aes128.decryptBlock((byte*) ssid + i, encrypted + i);

          // Load password from internal eeprom
          for (int i = 0; i < 32; i++)
            encrypted[i] = EEPROM.read(WIFI_CREDS_START + 32 + i);

          for (int i = 0; i < 32; i += 16)
            aes128.decryptBlock((byte*) wifi_pwd + i, encrypted + i);

          tft.setTextColor(ST77XX_WHITE);
          tft.fillScreen(SCHEME_BG);
          tft.setCursor(0, 5);

          tft.setTextColor(SCHEME_MAIN);
          tft.print("SSID:");
          tft.setTextColor(ST77XX_WHITE);
          tft.println(ssid);

          tft.setTextColor(SCHEME_MAIN);
          tft.print("\nPwd:");
          tft.setTextColor(ST77XX_WHITE);
          tft.println(wifi_pwd);

          tft.print("\nConnecting");

          // wifi setup
          WiFi.begin(ssid, wifi_pwd);
          while (WiFi.status() != WL_CONNECTED) {
            delay(1000);
            tft.print('.');
          }
          tft.println();
         
          wifiServer.begin();

          displayRemoteInfo();
      } else if (interact_return == 1){
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

int checkAuth(const char* token){
  // Check token is of correct length
  for (int i = 0; i < 64; i++){
    if (token[i] < 0x30 || token[i] > 0x5A){
      return -1;
    }
  }

  // // Check null terminator at last byte
  // if (token[65] != 0)
  //   return -1;

  // Decrypt the token and compare against nonce
  byte decrypted_token_bytes[32];
  byte encrypted_token_bytes[32];

  hexStringToBytes(token, encrypted_token_bytes, 32);

  // Decrypt the encrypted array
  for (int i = 0; i < 32; i += 16)
      aes128.decryptBlock(decrypted_token_bytes + i, encrypted_token_bytes + i);

  // Check that the nonces match
  for (int i = 0; i < 32; i++){
    if (decrypted_token_bytes[i] != nonce[i])
      return -1;
  }

  return 0;
}

void handleAuth(WiFiClient client, char* incoming){
  expecting_cmd = false;

  // Create and deserialise incoming json, check for errors
  DeserializationError error = deserializeJson(inp_doc, incoming);

  switch (error.code()) {
    // If this the json deseralized correctly, then its not encrypted so is an auth request
    case DeserializationError::Ok:
        Serial.print(F("Deserialization succeeded"));
        break;
    case DeserializationError::InvalidInput:
        Serial.print(F("Invalid input!"));
        return;
    case DeserializationError::NoMemory:
        Serial.print(F("Not enough memory"));
        return;
    default:
        Serial.print(F("Deserialization failed"));
        return;
  }

  // Check if requesting nonce
  if (!inp_doc["req"].isNull() && strcmp(inp_doc["req"], "auth") == 0){
    // Generate nonce and encrypt it with the session key
    for (int i = 0; i < 32; i++)
      nonce[i] = random(0, 255);

    // Create session key AES instance to encrypt the nonce
    AES128 aes128session;
    aes128session.setKey(session_key, aes128session.keySize());
    
    byte encryptedNonce[32];
    for (int i = 0; i < 32; i += 16)
        aes128session.encryptBlock(encryptedNonce + i, nonce + i);

    // Convert the nonce to a hex string and put in json doc
    char encryptedNonceString [65];
    bytesToHexString(encryptedNonce, encryptedNonceString, 32);
    resp_doc["nonce"] = encryptedNonceString;

    // Serialise the doc and clear
    serializeJson(resp_doc, resp_buffer, BUFF_SIZE);
    resp_doc.clear();

    // Send the encrypted nonce to the client
    client.print(resp_buffer);

    expecting_cmd = true;

    return;
  }
}

void handleCommand(WiFiClient client, char* incoming, int len){
  expecting_cmd = false;

  // Init the AES CTR cipher for encrypting with the session key
  CTR<AES128> aes_ctr_session;
  aes_ctr_session.setKey(session_key, aes_ctr_session.keySize());
  aes_ctr_session.setIV(nonce, 16);
  aes_ctr_session.setCounterSize(8);

  // Convert the incoming base64 token to bytes
  base64ToBytes(incoming, (byte*) resp_buffer);

  // Decrypt the incoming bytes
  aes_ctr_session.decrypt((byte*) inp_buffer, (byte*) resp_buffer, len);

  // Create and deserialise incoming json, check for errors
  DeserializationError error = deserializeJson(inp_doc, inp_buffer);

  switch (error.code()) {
    // If this the json deseralized correctly, then its not encrypted so is an auth request
    case DeserializationError::Ok:
        Serial.print(F("Deserialization succeeded"));
        break;
    case DeserializationError::InvalidInput:
        Serial.print(F("Invalid input!"));
        return;
    case DeserializationError::NoMemory:
        Serial.print(F("Not enough memory"));
        return;
    default:
        Serial.print(F("Deserialization failed"));
        return;
  }

  const char* token = inp_doc["token"];

  // Check the incoming password
  if (checkAuth(token) != 0)
    return;

  // Check the message has a type
  if (inp_doc["type"].isNull())
    return;
  
  // Pass to the respective handlers
  uint8_t type = (uint8_t) inp_doc["type"];
  switch(type){
    case 0:
      if (!inp_doc["name"].isNull()){
        delay(random(250, 500));
        if (handlePasswordRead(inp_doc["name"], resp_buffer, &aes_ctr_session) == 0)
          client.print(resp_buffer);
      }
      break;
    case 1:
      if (!inp_doc["name"].isNull() && !inp_doc["user"].isNull() && !inp_doc["pwd"].isNull()){
        delay(random(250, 500));
        if (handlePasswordWrite(inp_doc["name"], inp_doc["user"], inp_doc["pwd"], resp_buffer, &aes_ctr_session) == 0)
          client.print(resp_buffer);
      }
      break;
    case 2:
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
          
        if (handlePasswordEdit(inp_doc["name"], new_name, new_user, new_pwd, resp_buffer, &aes_ctr_session) == 0)
          client.print(resp_buffer);
      }
      break;
    case 3:
      if (!inp_doc["name"].isNull()){
        delay(random(250, 500));
        if (handlePasswordRemove(inp_doc["name"], resp_buffer, &aes_ctr_session) == 0)
          client.print(resp_buffer);
      }
      break;
    case 4:
      if (!inp_doc["name"].isNull()){
        delay(random(250, 500));
        if (handleWalletRead(inp_doc["name"], resp_buffer, &aes_ctr_session) == 0)
          client.print(resp_buffer);
      }
      break;
    case 5:
      if (!inp_doc["name"].isNull()){
        delay(random(250, 500));
        if (handleWalletAdd(inp_doc["name"], inp_doc["phrases"], resp_buffer, &aes_ctr_session) == 0)
          client.print(resp_buffer);
      }
      break;
    case 6:
      if (!inp_doc["name"].isNull()){
        delay(random(250, 500));
        if (handleWalletDel(inp_doc["name"], resp_buffer, &aes_ctr_session) == 0)
          client.print(resp_buffer);
      }
      break;
    default:
      break;
  }

  resp_doc.clear();
  inp_doc.clear();
}

// Handles remote password removal
int handlePasswordRemove(const char* enc_name, char* resp_buffer, CTR<AES128>* aes){
  byte enc_name_bytes[32];
  base64ToBytes(enc_name, enc_name_bytes);
  int response = password_manager->deleteEntry(enc_name_bytes);

  resp_doc["response"] = response;

  serializeAndEncrypt(resp_doc, resp_buffer, aes);

  return response;
}

// Handles remote password information request
int handlePasswordRead(const char* enc_name, char* resp_buffer, CTR<AES128>* aes)
{
  // Get entry details
  byte enc_name_bytes[32];
  base64ToBytes(enc_name, enc_name_bytes);
  Password_Entry* entry = password_manager->getEntry(enc_name_bytes);

  if (entry == NULL)
    return -1;
  
  // Encode and encrypt the entry details
  char encryptedUsername [50];
  bytesToBase64(entry->getEncryptedEmail(), encryptedUsername, 32);
  resp_doc["username"] = encryptedUsername;

  char encryptedPassword [50];
  bytesToBase64(entry->getEncryptedPassword(), encryptedPassword, 32);
  resp_doc["password"] = encryptedPassword;

  serializeAndEncrypt(resp_doc, resp_buffer, aes);

  return 0;
}

void serializeAndEncrypt(DynamicJsonDocument document, char* output_buffer, CTR<AES128>* aes){
  // Clear out the input buffer
  for (int i = 0; i < BUFF_SIZE; i++)
    inp_buffer[i] = 0;

  // Reset the aes session
  aes->setIV(nonce, 16);

  // Serialize to the input buffer
  int written_size = serializeJson(document, inp_buffer, MAX_BUFF_TXT_LEN);

  // Encrypt the contents of the input buffer with AES CTR (only needs the max txt size)
  aes->encrypt((byte*) inp_buffer, (byte*) inp_buffer, written_size + (BLOCK_SIZE - written_size%BLOCK_SIZE));

  // Base64 encode the buffer, and write this to the response buffer
  int base64len = bytesToBase64((byte*) inp_buffer, output_buffer, written_size + (BLOCK_SIZE - written_size%BLOCK_SIZE));
  resp_buffer[base64len] = 0;
}

// Handles remote password write
int handlePasswordWrite(const char* name, const char* user, const char* pwd, char* resp_buffer, CTR<AES128>* aes){
  byte encrypted_name[32];
  byte encrypted_user[32];
  byte encrypted_pwd[32];

  base64ToBytes(name, encrypted_name);
  base64ToBytes(user, encrypted_user);
  base64ToBytes(pwd, encrypted_pwd);
  
  int response = password_manager->addEntry(encrypted_name, encrypted_user, encrypted_pwd);

  resp_doc["response"] = response;

  serializeAndEncrypt(resp_doc, resp_buffer, aes);
  
  return response;
}

// Handles remote password write
int handlePasswordEdit(const char* old_name, const char* name, const char* user, const char* pwd, char* resp_buffer, CTR<AES128>* aes){
  Serial.println("Handling password edit");
  
  // Get the entry
  byte enc_name_bytes[32];
  base64ToBytes(old_name, enc_name_bytes);
  Password_Entry* entry = password_manager->getEntry(enc_name_bytes);
  if (entry == NULL){
    return -1;
  }

  // If name provided, update the name
  if (name != NULL){
    char decrypted_name[32];
    byte encrypted_name_bytes[32];

    base64ToBytes(name, encrypted_name_bytes);
    password_manager->decrypt(encrypted_name_bytes, decrypted_name);
    memcpy(entry->getName(), decrypted_name, 32);
  }

  // If username provided, update the username
  if (user != NULL){
    byte encrypted_user[32];
    base64ToBytes(user, encrypted_user);
    memcpy(entry->getEncryptedEmail(), encrypted_user, 32);
  }

  // If password provided, update the password
  if (password != NULL){
    byte encrypted_pwd[32];
    base64ToBytes(pwd, encrypted_pwd);
    memcpy(entry->getEncryptedPassword(), encrypted_pwd, 32);
  }

  eeprom_manager->clearEntryBit(entry->start_address);
  password_manager->save(entry);
  password_manager->sortEntries();

  resp_doc["response"] = 0;

  serializeAndEncrypt(resp_doc, resp_buffer, aes);
  
  return 0;
}

// Handles remote password information request
int handleWalletRead(const char* name, char* resp_buffer, CTR<AES128>* aes)
{
  // Init the AES CTR cipher for encrypting with the session key
  CTR<AES128> aes_ctr_session;
  aes_ctr_session.setKey(session_key, aes_ctr_session.keySize());
  aes_ctr_session.setIV(nonce, 16);
  aes_ctr_session.setCounterSize(8);

  byte enc_name_bytes[32];
  base64ToBytes(name, enc_name_bytes);
  Wallet_Entry* entry = wallet_manager->getEntry(enc_name_bytes);
  if (entry == NULL)
    return -1;

  JsonArray arr = resp_doc.to<JsonArray>();
  
  for (int i = 0; i < entry->phrase_count; i++){
    char to_hex [50];
    bytesToBase64(entry->getEncryptedPhrases()[i], to_hex, 32);
    arr.add(to_hex);
  }

  // Clear out the input buffer
  for (int i = 0; i < BUFF_SIZE; i++)
    inp_buffer[i] = 0;

  serializeAndEncrypt(resp_doc, resp_buffer, aes);

  return 0;
}

int handleWalletAdd(const char* name, const char* phrases, char* resp_buffer, CTR<AES128>* aes){
  byte enc_name_bytes[32];
  char decrypted_name[32];
  base64ToBytes(name, enc_name_bytes);
  wallet_manager->decrypt(enc_name_bytes, decrypted_name);

  // Borrow the resp_doc for parsing the array of phrases
  deserializeJson(resp_doc, phrases);

  Wallet_Entry* entry = wallet_manager->getFreeEntry();

  if (entry == NULL){
    resp_doc["response"] = -1;
    return -1;
  }

  entry->setName(decrypted_name);

  // extract the values
  JsonArray array = resp_doc.as<JsonArray>();
  for(JsonVariant v : array) {
    byte encrypted_phrase[32];
    base64ToBytes(v.as<char*>(), encrypted_phrase);
    entry->addPhrase(encrypted_phrase);
  }

  wallet_manager->save(entry);
  wallet_manager->setWalletCount(wallet_manager->getWalletCount()+1);
  
  resp_doc.clear();
  resp_doc["response"] = 0;

  serializeAndEncrypt(resp_doc, resp_buffer, aes);

  return 0;
}

int handleWalletDel(const char* name, char* resp_buffer, CTR<AES128>* aes){
  byte enc_name_bytes[32];
  base64ToBytes(name, enc_name_bytes);
  Wallet_Entry* entry = wallet_manager->getEntry(enc_name_bytes);
  if (entry == NULL)
    return -1;

  wallet_manager->remove(entry);

  resp_doc["response"] = 0;

  serializeAndEncrypt(resp_doc, resp_buffer, aes);

  return 0;
}

// Converts a string of bytes, to a base64 char array
int bytesToBase64(byte* bytes, char* base64String, int input_len){
  return encode_base64(bytes, input_len, (unsigned char*) base64String);
}

// Converts a base64 string, to an array of bytes
int base64ToBytes(const char* base64String, byte* bytes){
  return decode_base64((unsigned char*) base64String, bytes);
}

// Converts an array of bytes to a hexstring
void bytesToHexString(byte* bytes, char* hexString, int input_len) {
  for (int i = 0; i < input_len; i++) 
    sprintf(hexString + i * 2, "%02X", bytes[i]);
}

// Converts a hexstring to an array of bytes
void hexStringToBytes(const char* hexString, byte* bytes, int output_len){
  for (int i = 0; i < output_len; i++) 
      sscanf(hexString + 2 * i, "%2hhx", &bytes[i]);
}

// Displays the remote mode info on the display
void displayRemoteInfo(){
  tft.setTextColor(SCHEME_MAIN);
  tft.print("\nIP:");
  tft.setTextColor(ST77XX_WHITE);
  tft.println(WiFi.localIP());
  
  // Generate random session key
  tft.setTextColor(SCHEME_MAIN);
  tft.println("\nSession Key:\n");
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);

  session_key = (byte*) malloc(sizeof(byte)*16);
  generateSessionKey(session_key, 16);
  for (int i = 0; i < 16; i++)
    tft.print((char) session_key[i]);

  tft.setTextSize(1);
}

// Generates a random key for the session
void generateSessionKey(byte* key, int len){
  char* possGen = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ?!=";
  for (int i = 0; i < len; i++)
    key[i] = (byte) possGen[random(64)];
}

// Generates sha512
void sha512Gen(Hash *hash, char* data, int input_len, uint8_t* value) {
    hash->reset();
    hash->update(data, input_len);
    hash->finalize(value, HASH_SIZE);
}

// Checks if the hash of the passed password matches that stored in EEPROM
boolean entryCheck(char* password, int entry_length) { 
  uint8_t saved_hash[HASH_SIZE];
  loadHashFromEEPROM(saved_hash);
  
  uint8_t hash[HASH_SIZE];
  sha512Gen(&sha512, password, entry_length, hash);

  delay(random(500)); // No timing attacks here ;)

  for (int i = 0; i < HASH_SIZE; i++){
    if (saved_hash[i] != hash[i])
      return false;
  }
  return true;
}

// Loads the existing sha256 hash from the EEPROM
void loadHashFromEEPROM(uint8_t* hash){
  for (int i = 5; i < 5 + HASH_SIZE; i++)
    hash[i-5] = (char) EEPROM.read(i);
}

void writeMagicToEEPROM(){
  // Write magic phrase
  EEPROM.write(0, 45);
  EEPROM.write(1, 53);
  EEPROM.write(2, 50);
  EEPROM.write(3, 82);
  EEPROM.write(4, 66);

  EEPROM.commit();
}

void writeWiFiSSIDToEEPROM(char* ssid, int length){
  byte encrypted[32];
  byte to_encrypt[32];

  // Copy into buffer and pad with random stuff
  int i = 0;
  for (i = 0; i < length+1; i++)
    to_encrypt[i] = ssid[i];

  for (; i < 32; i++)
    to_encrypt[i] = random(0, 255);

  // Perform encryption
  for (int i = 0; i < 32; i += 16)
    aes128.encryptBlock(encrypted + i, (byte*) to_encrypt + i);

  // Write to EEPROM
  for (int j = 0; j < 32; j++)
    EEPROM.write(WIFI_CREDS_START + j, encrypted[j]);

  EEPROM.commit();
}

void writeWiFiPasswordToEEPROM(char* password, int length){
  byte encrypted[32];
  byte to_encrypt[32];

  // Copy into buffer and pad with random stuff
  int i = 0;
  for (i = 0; i < length+1; i++)
    to_encrypt[i] = password[i];

  for (; i < 32; i++)
    to_encrypt[i] = random(0, 255);

  // Perform encryption
  for (int i = 0; i < 32; i += 16)
    aes128.encryptBlock(encrypted + i, (byte*) to_encrypt + i);

  // Write to EEPROM
  for (int j = 0; j < 32; j++)
    EEPROM.write(WIFI_CREDS_START + 32 + j, encrypted[j]);

  EEPROM.commit();
}

// Writes new master password to EEPROM
void writeMasterPasswordToEEPROM(char* password, int entry_length){
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
      if (i%32 == 0)
        Serial.println();
    
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
  // Load in the passwords
  int pwd_count = password_manager->load();
  int wallet_count = wallet_manager->load();

  // Sorts the entries into alphabetical order (ish)
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
