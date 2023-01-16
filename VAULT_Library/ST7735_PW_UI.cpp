#include "ST7735_PW_UI.h"

// Constructor for Key
ST7735_PW_Menu_Item::ST7735_PW_Menu_Item(Adafruit_ST7735* display, const char* header){
    this->tft = display;
    this->title = header;
}

// Display the key
void ST7735_PW_Menu_Item::display(int position){
    tft->setCursor(10, 70 + position*20);
    tft->setTextColor(SCHEME_TEXT_LIGHT);
    tft->fillRoundRect(5, 68 + position*20 - 2, tft->width()-30, 14, 4, SCHEME_MAIN);
    tft->print(this->getTitle());
}

// Display key as selected
void ST7735_PW_Menu_Item::displaySelected(int position){
    tft->setCursor(10, 70 + position*20);
    tft->fillRoundRect(5, 68 + position*20 - 2, tft->width()-30, 14, 4, ST77XX_WHITE);
    tft->setTextColor(SCHEME_BG);
    tft->print(this->getTitle());
}

// Display the key
void ST7735_PW_Menu_Item::displayAdd(int position){
    tft->setCursor(tft->width()-15, 70 + position*20);
    tft->setTextColor(SCHEME_TEXT_LIGHT);
    tft->fillRoundRect(tft->width()-20, 68 + position*20 - 2, 14, 14, 4, SCHEME_MAIN);
    tft->print('+');
}

// Display add component as selected
void ST7735_PW_Menu_Item::displayAddSelected(int position){
    tft->setCursor(tft->width()-15, 70 + position*20);
    tft->fillRoundRect(tft->width()-20, 68 + position*20 - 2, 14, 14, 4, ST77XX_WHITE);
    tft->setTextColor(SCHEME_BG);
    tft->print('+');
}

// Returns the button title
const char* ST7735_PW_Menu_Item::getTitle(){
    return this->title;
}

// Constructor for Key
ST7735_PW_Menu::ST7735_PW_Menu(Adafruit_ST7735* display){
    this->tft = display;
    this->items = (ST7735_PW_Menu_Item*) malloc(sizeof(ST7735_PW_Menu_Item) * 2);
    this->selected_index = 0;
    this->items[0] = ST7735_PW_Menu_Item(display, (const char *) "Passwords");
    this->items[1] = ST7735_PW_Menu_Item(display, "Wallet Keys");
}

// Interacts with the menu using ir data
char ST7735_PW_Menu::interact(uint32_t* ir_data, 
    Password_Manager* password_manager, Wallet_Manager* wallet_manager){
  if (this->entered){
    if (selected_index == 0){ // Interact with password manager
        password_manager->interact(ir_data);

        // Check if escaped from manager
        if (password_manager->getEscaped()){
            password_manager->setEscaped(false);
            this->entered = false;
            return 1;
        }
    } else if (selected_index == 1){ // Interact with wallet manager
        wallet_manager->interact(ir_data);

        // Check if escaped from manager
        if (wallet_manager->getEscaped()){
            wallet_manager->setEscaped(false);
            this->entered = false;
            return 1;
        }
    }
  } else {
    // DOWN
    if (*ir_data == IR_DOWN){ // Go to entry below
        int old = selected_index;
        selected_index = selected_index + 1 == 2 ? 0 : selected_index + 1;
        this->items[old].display(old);
        this->items[old].displayAdd(old);
        this->items[selected_index].displaySelected(selected_index);
        this->items[selected_index].addSelected = false;
    }

    // UP
    if (*ir_data == IR_UP){ // Go to entry above
        int old = selected_index;
        selected_index = selected_index - 1 == -1 ? 1 : selected_index - 1;
        this->items[old].display(old);
        this->items[old].displayAdd(old);
        this->items[selected_index].displaySelected(selected_index);
        this->items[selected_index].addSelected = false;
    }

    // LEFT/RIGHT
    if (*ir_data == IR_LEFT || *ir_data == IR_RIGHT){ // Select add component
        this->items[selected_index].addSelected = !this->items[selected_index].addSelected;

        if (this->items[selected_index].addSelected){
            this->items[selected_index].display(selected_index);
            this->items[selected_index].displayAddSelected(selected_index);
        } else {
            this->items[selected_index].displaySelected(selected_index);
            this->items[selected_index].displayAdd(selected_index);
        }
    }

    // ENTER
    if (*ir_data == IR_OK){
        // do enter stuff
        this->entered = true;

        if (this->selected_index == 0){
            if (this->items[selected_index].addSelected){
                password_manager->setStage(2);
            }
            password_manager->display();
        } else if (this->selected_index == 1){
            if (this->items[selected_index].addSelected){
                wallet_manager->setStage(1);
            }
            wallet_manager->display();
        }
    }
  }
  return 0;
}

// Display the key
void ST7735_PW_Menu::display(){
    tft->setCursor(0, 5);
    
    for (int i = 0; i < 2; i++){
        tft->setCursor(10, 70 + i*20);
        if (i == this->selected_index){
            this->items[i].displaySelected(i);
        } else {
            this->items[i].display(i);
        }
        this->items[i].displayAdd(i);
    }
}

