#include "ST7735_PW_Keyboard.h"
#include "HardwareSerial.h"
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library for ST7735
#include "VAULT_Colours.h"
#include <AES.h>
#include "VAULT_IR_Codes.h"
#include "EEPROM_Manager.h"

#define PHRASES_PER_PAGE 10
#define PHRASE_SEP 12

class Wallet_Entry
{
    public:
        char* getName();
        byte** getEncryptedPhrases();

        void setName(char*);
        void addPhrase(byte*);

        int phrase_count;
        int start_address;

    private:
        char name[32];
        byte* encrypted_phrases[32];
};

class Wallet_Manager
{
    public:
        Wallet_Manager(Adafruit_ST7735*, AES128*, ST7735_PW_Keyboard*, EEPROM_Manager*);

        void display(); // Displays passwords and usernames on screen
        void interact(uint32_t*); // Allows user interaction

        void encrypt(char*, byte*); // Encrypts passed data and stores in aes_buffer
        void decrypt(byte*, char*); // Decrypts the passed data and stores in aes_buffer

        int load(); // Loads and decrypts saved password from file and stores in Password_Entry instance
        void save(Wallet_Entry*); // Saves encrypted Password_Entry to SD card at given position

        void setStage(int);
        Wallet_Entry* getEntry(const char*);

        boolean getEscaped();
        void setEscaped(boolean);
 
    private:
        Adafruit_ST7735* tft;
        AES128* aes128;
        ST7735_PW_Keyboard* keyboard;
        int stage = 0; // 0 = display names, 1 = display selected password/username, 2 = add name, 3 = add email, 4 = add password
        EEPROM_Manager* eeprom_manager;

        Wallet_Entry* entries;
        int wallet_count = 0;
        
        int start_wallet_display_index = 0;
        int selected_wallet_index = 0;

        int new_phrase_count = 0;
        int current_phrases_added = 0;

        boolean escaped = false;
};
