#include "ST7735_PW_Keyboard.h"
#include "HardwareSerial.h"
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library for ST7735
#include "VAULT_Colours.h"
#include <AES.h>
#include "VAULT_IR_Codes.h"
#include "EEPROM_Manager.h"

#define PASSWORDS_PER_PAGE 10
#define PASSWORD_START_Y 18
#define PASSWORD_SEP 14
#define PASSWORD_STORAGE_LIMIT 256

// Each entry stored in memory as | 0 | encrypted_name (32 bytes) |
//  encrypted email (32 bytes) | encrypted password (32 bytes)
class Password_Entry
{
    public:
        char* getName();
        void setName(char*);
        void setEncryptedEmail(char*);
        void setEncryptedPassword(char*);

        byte* getEncryptedEmail();
        byte* getEncryptedPassword();

        int start_address;
    private:
        char name[32];
        byte encryptedEmail[32];
        byte encryptedPassword[32];
};

class Password_Manager
{
    public:
        Password_Manager(Adafruit_ST7735*, AES128*, ST7735_PW_Keyboard*, EEPROM_Manager*);

        void display(); // Displays passwords and usernames on screen
        void interact(uint32_t*); // Allows user interaction

        void encrypt(char*, byte*); // Encrypts passed data and stores in aes_buffer
        void decrypt(byte*, char*); // Decrypts the passed data and stores in aes_buffer

        int load(); // Loads and decrypts saved password from file and stores in Password_Entry instance
        void save(Password_Entry*); // Saves encrypted Password_Entry to SD card at given position

        void sortEntries();
        Password_Entry* getEntry(byte* name);
        int addEntry(byte* name, byte* user, byte* pwd);
        int deleteEntry(byte* name);

        void sanitiseInput(const char* input, char* result, int buf_size);
        void setStage(int);

        boolean getEscaped();
        void setEscaped(boolean);

    private:
        Adafruit_ST7735* tft;
        AES128* aes128;
        ST7735_PW_Keyboard* keyboard;
        int stage = 0; // 0 = display names, 1 = display selected password/username, 2 = add name, 3 = add email, 4 = add password
        EEPROM_Manager* eeprom_manager;

        int password_count = 0;
        Password_Entry* entries;

        // For displaying passwords
        int start_pw_display_index = 0;
        int selected_pw_index = 0;

        bool overwriting = false;
        boolean escaped = false;
};