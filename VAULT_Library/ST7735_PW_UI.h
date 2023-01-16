#include "ST7735_PW_Keyboard.h"
#include "HardwareSerial.h"
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library for ST7735
#include "VAULT_Colours.h"
#include <AES.h>
#include "VAULT_IR_Codes.h"
#include "EEPROM_Manager.h"
#include "Password_Manager.h"
#include "Wallet_Manager.h"

class ST7735_PW_Menu_Item
{
	public:
		ST7735_PW_Menu_Item(Adafruit_ST7735*, const char*);
		void display(int);
        void displaySelected(int);
        void displayAdd(int);
        void displayAddSelected(int);
        const char* getTitle();
        boolean addSelected = false;
		
	private:
		Adafruit_ST7735* tft;
        const char* title;
};

class ST7735_PW_Menu
{
	public:
		ST7735_PW_Menu(Adafruit_ST7735*);
		void display();
        char interact(uint32_t*, Password_Manager*, Wallet_Manager*);
		
	private:
		Adafruit_ST7735* tft;
        ST7735_PW_Menu_Item* items;

        int selected_index;
        boolean entered = false;
};

