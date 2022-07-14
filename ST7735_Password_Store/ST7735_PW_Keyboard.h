/*
  ST7735_PW_Keyboard.h - Keyboard for ST7735
  Copyright (c) 2021 Luke Mills.  All right reserved.
*/

#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library for ST7735
#include <Colours.h>

// ensure this library description is only included once
#ifndef ST7735_PW_Keyboard_h
#define ST7735_PW_Keyboard_h

#define KEY_HEIGHT 11
#define NORMAL_KEY_WIDTH 11
#define EDGE_BORDER 4

// #define KEYBOARD_BG_COLOUR 0x9DD3
// #define UNSELECTED_BG_COLOUR 0x2C92
// #define SELECTED_BG_COLOUR 0xF6CD
// #define PROMPT_COLOUR 0xFFFF
// #define INPUT_COLOUR 0x9DD3
// #define PROMPT_BACKGROUND 0x9DD3
// #define SELECTED_LETTER_COLOUR 0x0000
// #define UNSELECTED_LETTER_COLOUR 0xFFFF
#define MAX_INPUT_LENGTH 32

class Key
{
	public:
		Key(Adafruit_ST7735*, int, int, int, int, char*, int);
		void display(int);
		void displaySelected(int);
		
		int x;
		int y;
		int w;
		int h;
		char* action;
		
	private:
		Adafruit_ST7735* tft;
		int bottom_key;
};

// library interface description
class ST7735_PW_Keyboard
{
  // user-accessible "public" interface
  public:
    ST7735_PW_Keyboard(Adafruit_ST7735*);
	void press();
	void displayCurrentString();
    void display();
	void displayPrompt(char* prompt);
	void moveRight();
	void moveLeft();
	void moveUp();
	void moveDown();
	void setMode(int);
	void setMode(int, int);
	void reset();
	void goToTabs();
	void exitTabs();
	int enterPressed();
	char* getCurrentInput();
	void end();
	void setModeClear(int, int);
	void interact(uint32_t*);
	void displayInstructions();
	int getCurrentInputLength();

  // library-accessible "private" interface
  private:
    Adafruit_ST7735* tft;
	Key* letters;
	Key* numbers;
	Key* specials;
	Key* bottom_keys;
	
	Key* selected;
	char* current_string; // Entered string
	int mode; // Keyboard mode
	int selected_index; // Index of selected key in list
	int current_input_length; // Length of current input
	int enter_pressed; // Indicate if enter pressed
	int last_mode; // Mode to return to when exiting tabs
	int last_key; // Key last selected when entering tabs
};

#endif