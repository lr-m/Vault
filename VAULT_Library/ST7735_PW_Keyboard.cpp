/*
  ST7735_PW_Keyboard.h - Keyboard for ST7735
  Copyright (c) 2021 Luke Mills.  All right reserved.
*/

#include "ST7735_PW_Keyboard.h"
#include "HardwareSerial.h"
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library for ST7735

// Constructor for Keyboard
ST7735_PW_Keyboard::ST7735_PW_Keyboard(Adafruit_ST7735* display)
{
	tft = display;
	
	// Allocate space for buttons
	letters = (Key*) malloc(sizeof(Key) * 26);
	numbers = (Key*) malloc(sizeof(Key) * 10);
	specials = (Key*) malloc(sizeof(Key) * 32);
	bottom_keys = (Key*) malloc(sizeof(Key) * 3);
	
	// Allocate selected key
	selected = (Key*) malloc(sizeof(Key));
	
	// Initialise variables
	selected_index = 0;
	mode = 0;
	current_input_length = 0;
	current_string = (char*) malloc(sizeof(char) * (MAX_INPUT_LENGTH + 1));
	current_string[0] = 0;
	enter_pressed = 0;
	length_limit = MAX_INPUT_LENGTH;
	
	last_mode = 0;
	last_key = 0;
	
	// Define bottom keys
	int tab_width = 36;
	bottom_keys[0] = Key(tft, EDGE_BORDER, tft -> height() - 15, 
		tab_width, KEY_HEIGHT, " <-- ", 1);
	bottom_keys[1] = Key(tft, tft -> width() / 2 - tab_width / 2, 
		tft -> height() - 15, tab_width, KEY_HEIGHT, "Space", 1);
	bottom_keys[2] = Key(tft, tft -> width() - EDGE_BORDER - tab_width, 
		tft -> height() - 15, tab_width, KEY_HEIGHT, "Enter", 1);
	
	int current_letter = 0;
	
	// Define the letters
	char* key_chars[26] = {"q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "a", "s", 
		"d", "f", "g", "h", "j", "k", "l", "z", "x", "c", "v", "b", "n", "m"};
	
	for (int i = 0; i < 10; i++){
		letters[current_letter] = Key(
			tft, EDGE_BORDER + i * (NORMAL_KEY_WIDTH+1), 
				tft -> height() - 4 * (KEY_HEIGHT + 4), NORMAL_KEY_WIDTH, 
				KEY_HEIGHT, key_chars[current_letter], 0);
		current_letter++;
	}
	
	for (int i = 0; i < 9; i++){
		letters[current_letter] = Key(tft, EDGE_BORDER + (0.66 * NORMAL_KEY_WIDTH) + 
			i * (NORMAL_KEY_WIDTH + 1), 
			tft -> height() - 3 * (KEY_HEIGHT + 4), NORMAL_KEY_WIDTH, 
			KEY_HEIGHT, key_chars[current_letter], 0);
		current_letter++;
	}
	
	for (int i = 0; i < 7; i++){
		letters[current_letter] = Key(tft, 
			EDGE_BORDER + (1.8*NORMAL_KEY_WIDTH) + 
			i * (NORMAL_KEY_WIDTH + 1), 
			tft -> height() - 2 * (KEY_HEIGHT + 4), NORMAL_KEY_WIDTH, 
			KEY_HEIGHT, key_chars[current_letter], 0);
		current_letter++;
	}
	
	// Define the numbers
	char* key_nums[10] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"};
	
	for (int i = 0; i < 10; i++){
		numbers[i] = Key(tft, EDGE_BORDER + i * (NORMAL_KEY_WIDTH+1), 
				tft -> height() - 4 * (KEY_HEIGHT + 4), NORMAL_KEY_WIDTH, 
				KEY_HEIGHT, key_nums[i], 0);
	}
	
	// Define the specials
	char* key_specials[32] = {"!", "\"", "#", "$", "%", "&", "\"", "(", ")", 
		"*", "+", ",", "-", ".", "/", ":", ";", "<", "=", ">", "?", "@", "[", 
		"\\", "]", "^", "_", "`", "{", "|", "}", "~"};
	current_letter = 0;
	for (int i = 0; i < 11; i++){
		specials[current_letter] = Key(
			tft, EDGE_BORDER + i * (SPECIAL_KEY_WIDTH+1), 
			tft -> height() - 4 * (KEY_HEIGHT + 4), SPECIAL_KEY_WIDTH, 
			KEY_HEIGHT,	key_specials[current_letter], 0);
		current_letter++;
	}
	
	for (int i = 0; i < 11; i++){
		specials[current_letter] = Key(tft, EDGE_BORDER + i * (SPECIAL_KEY_WIDTH+1), 
			tft -> height() - 3 * (KEY_HEIGHT + 4), SPECIAL_KEY_WIDTH, 
			KEY_HEIGHT, key_specials[current_letter], 0);
		current_letter++;
	}
	
	for (int i = 0; i < 10; i++){
		specials[current_letter] = Key(tft, 
			EDGE_BORDER + SPECIAL_KEY_WIDTH/2 + i * (SPECIAL_KEY_WIDTH+1), 
			tft -> height() - 2 * (KEY_HEIGHT + 4), SPECIAL_KEY_WIDTH, 
			KEY_HEIGHT, key_specials[current_letter], 0);
		current_letter++;
	}
	
	selected = letters;
}

// Interacts with the keyboard
void ST7735_PW_Keyboard::interact(uint32_t* ir_data){
  if (*ir_data == 0xE31CFF00)
    press();

  // 1
  if (*ir_data == 0xBA45FF00)
    setMode(0);

  // 2
  if (*ir_data == 0xB946FF00)
    setMode(1);

  // 3
  if (*ir_data == 0xB847FF00)
    setMode(2);

  // 4
  if (*ir_data == 0xBB44FF00)
    setMode(3);

  // DOWN
  if (*ir_data == 0xAD52FF00)
    moveDown();

  // UP
  if (*ir_data == 0xE718FF00)
    moveUp();

  // RIGHT
  if (*ir_data == 0xA55AFF00)
    moveRight();

  // LEFT
  if (*ir_data == 0xF708FF00)
    moveLeft();
}

// Resets the keyboard between inputs
void ST7735_PW_Keyboard::reset(){
	mode = 0;
	selected_index = 0;
	selected = letters;

	current_input_length = 0;
	free(current_string);
	current_string = (char*) malloc(sizeof(char) * (MAX_INPUT_LENGTH + 1));
	current_string[0] = 0;
	enter_pressed = 0;
}

void ST7735_PW_Keyboard::setLengthLimit(int limit){
	this->length_limit = limit;
}

void ST7735_PW_Keyboard::end(){
	current_input_length = 0;
	free(current_string);
	current_string = (char*) malloc(sizeof(char) * (MAX_INPUT_LENGTH + 1));
	current_string[0] = 0;
	enter_pressed = 0;
	setModeClear(last_mode, last_key);
}

// Returns the current input
char* ST7735_PW_Keyboard::getCurrentInput(){
	return current_string;
}

// Returns length of current input
int ST7735_PW_Keyboard::getCurrentInputLength(){
	return current_input_length;
}


// Performs the action of the currently selected key
void ST7735_PW_Keyboard::press(){
	if (selected -> action == "Space"){
		if (current_input_length < this->length_limit){
			// Add space to current string
			current_string[current_input_length + 1] = 0;
			current_string[current_input_length] = ' ';
			current_input_length++;
		}
	} else if (selected -> action == " <-- "){
		// Remove last element from entered string
		if (current_input_length > 0){
			current_input_length--;
			current_string[current_input_length] = 0;
		}
	} else if (selected -> action == "Enter"){
		// Signal that enter has been pressed
		if (current_string[0] != 0)
			enter_pressed = 1;
	} else {
		if (current_input_length < this->length_limit){
			// Add pressed key to current string
			current_string[current_input_length + 1] = 0;
			if (mode == 1){
				current_string[current_input_length] = 
					(char) (selected -> action[0] - 32);
			} else {
				current_string[current_input_length] = selected -> action[0];
			}
			current_input_length++;
		}
	}
	this -> displayCurrentString();
}

// Indicates if the enter key has been pressed
int ST7735_PW_Keyboard::enterPressed(){
	return enter_pressed;
}

// Go to the bottom row of buttons
void ST7735_PW_Keyboard::goToTabs(){
	if (mode != 4){
		last_mode = mode;
		last_key = selected_index;
		
		mode = 4;
		selected_index = 0;
		selected = bottom_keys;
		selected -> displaySelected(mode);
	}
}

// Leave the bottom row of buttons
void ST7735_PW_Keyboard::exitTabs(){
	if (mode == 4){
		selected -> display(mode);
		this -> setMode(last_mode, last_key);
	}
}

// Display the keyboard on the screen
void ST7735_PW_Keyboard::display()
{
	tft -> fillRect(0, tft -> height() - 4 * (KEY_HEIGHT + 4) - 4, tft -> width(), 
		4 * (KEY_HEIGHT + 4) + 4, SCHEME_BG);
	
	if (mode == 0 || mode == 1){
		for (int i = 0; i < 26; i++)
			letters[i].display(mode);
	} else if (mode == 2){
		for (int i = 0; i < 10; i++)
			numbers[i].display(mode);
	} else {
		for (int i = 0; i < 32; i++)
			specials[i].display(mode);
	}
	
	for (int i = 0; i < 3; i++)
		bottom_keys[i].display(mode);
	
	selected -> displaySelected(mode);
}

// Set the mode of the keyboard i.e. which set of buttons are displayed/selectable
void ST7735_PW_Keyboard::setMode(int new_mode, int key_index){
	mode = new_mode;
	selected_index = key_index;
	selected -> display(mode);
	
	if (mode == 0 || mode == 1){
		selected = letters + key_index;
	} else if (mode == 2){
		selected = numbers + key_index;
	} else {
		selected = specials + key_index;
	}
}

void ST7735_PW_Keyboard::setModeClear(int new_mode, int key_index){
	mode = new_mode;
	selected_index = key_index;
	
	if (mode == 0 || mode == 1){
		selected = letters + key_index;
	} else if (mode == 2){
		selected = numbers + key_index;
	} else {
		selected = specials + key_index;
	}
}

// Overloaded set mode returns to the first key
void ST7735_PW_Keyboard::setMode(int new_mode){
	mode = new_mode;
	selected_index = 0;
	
	if (mode == 0 || mode == 1){
		selected = letters;
	} else if (mode == 2){
		selected = numbers;
	} else {
		selected = specials;
	}
	
	this -> display();
}

// Displays the given prompt above the keyboard
void ST7735_PW_Keyboard::displayPrompt(char* prompt)
{
	tft -> fillRect(0, 0, tft -> width(), tft -> height() / 2, SCHEME_BG);
	tft -> fillRect(0, 0, tft->width(), 12, SCHEME_MAIN);
	tft -> setTextColor(SCHEME_TEXT_LIGHT);
	tft -> setCursor(2, 2);
	tft -> print(prompt);
}

// Displays the current string that has been entered by the user
void ST7735_PW_Keyboard::displayCurrentString(){
	tft -> fillRect(0, 15, tft -> width(), 40, SCHEME_BG);
	tft -> setTextColor(SCHEME_TEXT_LIGHT);
	tft -> setCursor(0, 15);
	tft -> print(current_string);
}

// Set the selected key to the key on the right
void ST7735_PW_Keyboard::moveDown(){
	selected -> display(mode);
	
	if (mode == 0 || mode == 1){
		if (selected_index < 9){
			selected_index += 10;
		} else if (selected_index == 9){
			selected_index = 18;
		} else if (selected_index < 19){
			selected_index += 8;
			if (selected_index < 19){
				selected_index = 19;
			} else if (selected_index > 25){
				selected_index = 25;
			}
		} else if (selected_index < 26){
			goToTabs();
			selected -> displaySelected(mode);
			return;
		}
		
		selected = letters + selected_index;
	} else if (mode == 2) {
		goToTabs();
	} else if (mode == 3){
		if (selected_index < 21){
			selected_index += 11;
		} else if (selected_index == 21){
			selected_index = 31;
		} else if (selected_index < 32){
			goToTabs();
			selected -> displaySelected(mode);
			return;
		}
		
		selected = specials + selected_index;
	}
	
	selected -> displaySelected(mode);
}

// Set the selected key to the key on the right
void ST7735_PW_Keyboard::moveUp(){
	selected -> display(mode);
	
	if (mode == 0 || mode == 1){
		if (selected_index >= 19){
			selected_index -= 8;
		} else if (selected_index >= 10){
			selected_index -= 10;
		}
		
		selected = letters + selected_index;
	} else if (mode == 3) {
		if (selected_index > 10){
			selected_index -= 11;
		} else if (selected_index > 19){
			selected_index -= 10;
		}
		
		selected = specials + selected_index;
	} else if (mode == 4){
		exitTabs();
	}
	
	selected -> displaySelected(mode);
}

// Set the selected key to the key on the right
void ST7735_PW_Keyboard::moveRight(){
	selected -> display(mode);
	
	if (mode == 0 || mode == 1){
		selected_index++;
		if (selected_index == 26)
			selected_index = 0;
		
		selected = letters + selected_index;
	} else if (mode == 2){
		selected_index++;
		if (selected_index == 10)
			selected_index = 0;
		
		selected = numbers + selected_index;
	} else if (mode == 3) {
		selected_index++;
		if (selected_index == 32)
			selected_index = 0;
		
		selected = specials + selected_index;
	} else if (mode == 4){
		selected_index++;
		if (selected_index == 3)
			selected_index = 0;
		
		selected = bottom_keys + selected_index;
	}
	
	selected -> displaySelected(mode);
}

// Set the selected key to the key on the left
void ST7735_PW_Keyboard::moveLeft(){
	selected -> display(mode);
	
	if (mode == 0 || mode == 1){
		selected_index--;
		if (selected_index == -1)
			selected_index = 25;
		
		selected = letters + selected_index;
	} else if (mode == 2){
		
		selected_index--;
		if (selected_index == -1)
			selected_index = 9;
		
		selected = numbers + selected_index;
	} else if (mode == 3) {
		selected_index--;
		if (selected_index == -1)
			selected_index = 31;
		
		selected = specials + selected_index;
	} else if (mode == 4){
		selected_index--;
		if (selected_index == -1)
			selected_index = 2;
		
		selected = bottom_keys + selected_index;
	}
	
	selected -> displaySelected(mode);
}

// Displays the keyboard instructions for 5 seconds
void ST7735_PW_Keyboard::displayInstructions(){
  tft -> fillScreen(SCHEME_BG);
  tft -> setCursor(0, 0);
  tft -> setTextColor(SCHEME_TEXT_LIGHT);
  tft -> println("Keyboard Instructions");
  tft -> println("\n*To see again, unplug device and plug it back in*\n");
  tft -> println("<-- : Select Key on Left");
  tft -> println("--> : Select Key on Right");
  tft -> println("DOWN: Enter Bottom Row");
  tft -> println("UP  : Exit Bottom Row\n");
  tft -> println("1   : Lower Case");
  tft -> println("2   : Upper Case");
  tft -> println("3   : Numbers");
  tft -> println("4   : Special Characters");
  delay(5000);
}



// Constructor for Key
Key::Key(Adafruit_ST7735* display, int x_pos, int y_pos, int width, 
		int height, char* act, int bottom){
	tft = display;
	x = x_pos;
	y = y_pos;
	w = width;
	h = height;
	action = act;
	bottom_key = bottom;
}

// Display the key
void Key::display(int mode){
	tft -> fillRoundRect(x, y, w, h, 2, SCHEME_MAIN);
	tft -> setCursor(x + 3, y + 2);
	tft -> setTextColor(SCHEME_TEXT_LIGHT);
	if (mode == 1 && bottom_key == 0){
		tft -> print((char) (action[0] - 32));
	} else {
		tft -> print(action);
	}
}

// Display the key as selected
void Key::displaySelected(int mode){
	tft -> fillRoundRect(x, y, w, h, 2, ST77XX_WHITE);
	tft -> setCursor(x + 3, y + 2);
	tft -> setTextColor(SCHEME_TEXT_DARK);
	if (mode == 1 && bottom_key == 0){
		tft -> print((char) (action[0] - 32));
	} else {
		tft -> print(action);
	}
}
