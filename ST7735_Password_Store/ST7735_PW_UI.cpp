#include "ST7735_PW_UI.h"

// Constructor for Key
ST7735_PW_Menu_Item::ST7735_PW_Menu_Item(Adafruit_ST7735* display, const char* header){
    this->tft = display;
    this->title = header;
}

// Display the key
void ST7735_PW_Menu_Item::display(int position){
    tft->setCursor(10, 7 + position*20);
    tft->setTextColor(SCHEME_TEXT_LIGHT);
    tft->fillRoundRect(5, 5 + position*20 - 2, tft->width()-30, 14, 4, SCHEME_MAIN);
    tft->print(this->getTitle());
}

void ST7735_PW_Menu_Item::displaySelected(int position){
    tft->setCursor(10, 7 + position*20);
    tft->fillRoundRect(5, 5 + position*20 - 2, tft->width()-30, 14, 4, ST77XX_WHITE);
    tft->setTextColor(SCHEME_BG);
    tft->print(this->getTitle());
}

// Display the key
void ST7735_PW_Menu_Item::displayAdd(int position){
    tft->setCursor(tft->width()-15, 7 + position*20);
    tft->setTextColor(SCHEME_TEXT_LIGHT);
    tft->fillRoundRect(tft->width()-20, 5 + position*20 - 2, 14, 14, 4, SCHEME_MAIN);
    tft->print('+');
}

void ST7735_PW_Menu_Item::displayAddSelected(int position){
    tft->setCursor(tft->width()-15, 7 + position*20);
    tft->fillRoundRect(tft->width()-20, 5 + position*20 - 2, 14, 14, 4, ST77XX_WHITE);
    tft->setTextColor(SCHEME_BG);
    tft->print('+');
}


const char* ST7735_PW_Menu_Item::getTitle(){
    return this->title;
}

// Constructor for Key
ST7735_PW_Menu::ST7735_PW_Menu(Adafruit_ST7735* display){
    this->tft = display;
    this->items = (ST7735_PW_Menu_Item*) malloc(sizeof(ST7735_PW_Menu_Item) * 3);
    this->selected_index = 0;
    this->items[0] = ST7735_PW_Menu_Item(display, (const char *) "Passwords");
    this->items[1] = ST7735_PW_Menu_Item(display, "Recovery Keys");
    this->items[2] = ST7735_PW_Menu_Item(display, "Backup Codes");
}

void ST7735_PW_Menu::interact(uint32_t* ir_data, Password_Manager* manager){
  if (this->entered){
    if (selected_index == 0){
        manager->interact(ir_data);

        if (manager->escaped){
            manager->escaped = false;
            this->entered = false;
            tft->fillScreen(SCHEME_BG);
            this->display();
        }
    }
  } else {
    // DOWN
    if (*ir_data == IR_DOWN){
        int old = selected_index;
        selected_index = selected_index + 1 == 3 ? 0 : selected_index + 1;
        this->items[old].display(old);
        this->items[old].displayAdd(old);
        this->items[selected_index].displaySelected(selected_index);
        this->items[selected_index].addSelected = false;
    }

    // UP
    if (*ir_data == IR_UP){
        int old = selected_index;
        selected_index = selected_index - 1 == -1 ? 2 : selected_index - 1;
        this->items[old].display(old);
        this->items[old].displayAdd(old);
        this->items[selected_index].displaySelected(selected_index);
        this->items[selected_index].addSelected = false;
    }

    // UP
    if (*ir_data == IR_LEFT || *ir_data == IR_RIGHT){
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
                manager->setStage(2);
            }
            manager->display();
        }
    }
  }
}

// Display the key
void ST7735_PW_Menu::display(){
    tft->setCursor(0, 5);
    
    for (int i = 0; i < 3; i++){
        tft->setCursor(10, 7 + i*20);
        if (i == this->selected_index){
            this->items[i].displaySelected(i);
        } else {
            this->items[i].display(i);
        }
        this->items[i].displayAdd(i);
    }
}

// Password manager class functions
Password_Manager::Password_Manager(Adafruit_ST7735* display, AES128* aes, ST7735_PW_Keyboard* keyboard, EEPROM_Manager* eeprom_manager){
    this->tft = display;
    this->aes128 = aes;

    this->entries = (Password_Entry*) malloc(sizeof(Password_Entry) * 50);
    this->keyboard = keyboard;

    this->eeprom_manager = eeprom_manager;

    // for (int i = 0; i < 50; i++){
    //     entries[i].setName("TEST NAME");
    //     entries[i].setPassword("TEST PASSWORD 1");
    //     entries[i].setEmail("TEST EMAIL 1");
    // }

    // this->password_count = 50;
}

void Password_Manager::setStage(int stage){
    this->stage = stage;
}

// Display the currently visible passwords
void Password_Manager::display(){
    tft->fillScreen(SCHEME_BG);
    if (this->stage == 0){ // Display password names to select
        tft->fillRect(0, 0, tft->width(), 13, SCHEME_MAIN);

        // Title
        tft->setCursor(3, 3);
        tft->setTextColor(ST77XX_WHITE);
        tft->print("Passwords");

        // Page number
        tft->setCursor(tft->width()-20, 3);
        tft->print(int(floor(selected_pw_index/PASSWORDS_PER_PAGE)) + 1);
        tft->print('/');
        tft->print(int(ceil((float) password_count/PASSWORDS_PER_PAGE)));

        int base = PASSWORD_START_Y + (selected_pw_index - this -> start_pw_display_index) * PASSWORD_SEP;
        tft->fillTriangle(tft->width() - 7, base, tft->width() - 7, base + PASSWORD_SEP / 2, tft->width() - 15, base + PASSWORD_SEP / 4, SCHEME_MAIN);

        tft->setTextColor(ST77XX_WHITE);
        for (int i = this->start_pw_display_index; 
                i < min(this->password_count, this->start_pw_display_index + PASSWORDS_PER_PAGE); i++){
            tft->setCursor(5, PASSWORD_START_Y + (i - this->start_pw_display_index) * PASSWORD_SEP);
            tft->println(this->entries[i].getName());
        }
    } else if (this->stage == 1){ // Display details of selected password
        tft->fillRect(0, 0, tft->width(), 13, SCHEME_MAIN);

        // Title
        tft->setTextSize(1);
        tft->setCursor(3, 3);
        tft->setTextColor(ST77XX_WHITE);
        tft->println(this->entries[this->selected_pw_index].getName());

        tft->setCursor(5, 18);
        tft->setTextColor(SCHEME_MAIN);
        tft->println("\nUsername/Email:\n");
        tft->setTextColor(ST77XX_WHITE);
        tft->println(this->entries[this->selected_pw_index].getEmail());

        tft->setTextColor(SCHEME_MAIN);
        tft->println("\n\nPassword:\n");
        tft->setTextColor(ST77XX_WHITE);
        tft->println(this->entries[this->selected_pw_index].getPassword());
    } else if (this->stage == 2){ // Allow user to enter new password name
        keyboard->reset();
        keyboard->setLengthLimit(16);
        keyboard->displayPrompt("Enter new name:");
        keyboard->display();
    } else if (this->stage == 3){ // Allow user to enter new email/username
        keyboard->reset();
        keyboard->setLengthLimit(30);
        keyboard->displayPrompt("Enter new account:");
        keyboard->display();
    } else if (this->stage == 4){ // Allow user to enter new password
        keyboard->reset();
        keyboard->setLengthLimit(30);
        keyboard->displayPrompt("Enter new password:");
        keyboard->display();
    }
}

// Interact with the password manager
void Password_Manager::interact(uint32_t* ir_data){

    if (this->stage == 0){ // Let user move up and down, and across pages of passwords
        if (*ir_data == IR_UP){ // Move cursor up
            int base = PASSWORD_START_Y + (selected_pw_index - this -> start_pw_display_index) * PASSWORD_SEP;
            tft->fillTriangle(tft->width() - 7, base, tft->width() - 7, base + PASSWORD_SEP / 2, tft->width() - 15, base + PASSWORD_SEP / 4, SCHEME_BG);

            selected_pw_index = selected_pw_index - 1 < start_pw_display_index 
                ? start_pw_display_index + min(PASSWORDS_PER_PAGE - 1, this->password_count - start_pw_display_index - 1)
                : selected_pw_index - 1;

            base = PASSWORD_START_Y + (selected_pw_index - this -> start_pw_display_index) * PASSWORD_SEP;
            tft->fillTriangle(tft->width() - 7, base, tft->width() - 7, base + PASSWORD_SEP / 2, tft->width() - 15, base + PASSWORD_SEP / 4, SCHEME_MAIN);
        } else if (*ir_data == IR_DOWN){ // Move cursor down

            int base = PASSWORD_START_Y + (selected_pw_index - this -> start_pw_display_index) * PASSWORD_SEP;
            tft->fillTriangle(tft->width() - 7, base, tft->width() - 7, base + PASSWORD_SEP / 2, tft->width() - 15, base + PASSWORD_SEP / 4, SCHEME_BG);

            selected_pw_index = selected_pw_index + 1 == min(start_pw_display_index + PASSWORDS_PER_PAGE, this->password_count)
                ? start_pw_display_index
                : selected_pw_index + 1;

            base = PASSWORD_START_Y + (selected_pw_index - this -> start_pw_display_index) * PASSWORD_SEP;
            tft->fillTriangle(tft->width() - 7, base, tft->width() - 7, base + PASSWORD_SEP / 2, tft->width() - 15, base + PASSWORD_SEP / 4, SCHEME_MAIN);
        } else if (*ir_data == IR_LEFT){ // Move cursor up
            if (password_count % PASSWORDS_PER_PAGE != 0){
                start_pw_display_index = start_pw_display_index - PASSWORDS_PER_PAGE < 0 
                    ? password_count - password_count % PASSWORDS_PER_PAGE 
                    : start_pw_display_index - PASSWORDS_PER_PAGE;
            } else {
                start_pw_display_index = start_pw_display_index - PASSWORDS_PER_PAGE < 0 
                    ? password_count - PASSWORDS_PER_PAGE 
                    : start_pw_display_index - PASSWORDS_PER_PAGE;
            }
            

            this -> selected_pw_index = start_pw_display_index;

            this->display();
        } else if (*ir_data == IR_RIGHT){ // Scroll to next page
            start_pw_display_index = start_pw_display_index + PASSWORDS_PER_PAGE >= password_count 
                ? 0 
                : start_pw_display_index + PASSWORDS_PER_PAGE;

            this -> selected_pw_index = start_pw_display_index;

            this->display();
        }  else if (*ir_data == IR_OK){ // Move to next step (display password)
            stage++;
            this->display();
        } else if (*ir_data == IR_HASHTAG){ // Exit selected password
            this->escaped = true;
        }
     } else if (this->stage == 1){ // Display details of selected password
        if (*ir_data == IR_HASHTAG){ // Exit selected password
            stage--;
            this->display();
        }
    } else if (this->stage == 2){ // Allow user to enter new password name
        keyboard->interact(ir_data);

        if (keyboard -> enterPressed()){
            memcpy(this->entries[password_count].getName(), keyboard->getCurrentInput(), 32);
            stage++;
            this->display();
        }
    } else if (this->stage == 3){ // Allow user to enter new email/username
        keyboard->interact(ir_data);

        if (keyboard -> enterPressed()){
            memcpy(this->entries[password_count].getEmail(), keyboard->getCurrentInput(), 32);
            stage++;
            this->display();
        }
    } else if (this->stage == 4){ // Allow user to enter new password
        keyboard->interact(ir_data);

        if (keyboard -> enterPressed()){

            memcpy(this->entries[password_count].getPassword(), keyboard->getCurrentInput(), 32);

            password_count++;
            
            Serial.println("Added: ");
            Serial.println(this->entries[password_count-1].getName());
            Serial.println(this->entries[password_count-1].getEmail());
            Serial.println(this->entries[password_count-1].getPassword());

            this->save(&this->entries[password_count-1]);

            // Write added password store to EEPROM

            stage = 0;
            this->display();
        }
    }
} 

// Encrypt the passed data, and store it in the aes_buffer for writing to SD
void Password_Manager::encrypt(char* data, byte* encrypted){
    // Convert char to byte array
    byte data_bytes[32];

    Serial.print("Encrypting: ");
    Serial.println(data);

    int i = 0;
    while(data[i] != 0 && i < 31){
        data_bytes[i] = byte(data[i]);
        Serial.println(data_bytes[i], HEX);
        i++;
    }

    data_bytes[i] = 0;
    i++;

    // Padding
    while (i < 32){
        data_bytes[i] = random(255);
        i++;
    }

    // Perform encryption
    for (int i = 0; i < 32; i += 16){
        aes128->encryptBlock(encrypted + i, data_bytes + i);
    }
    
    for (int i = 0; i < 32; i++){
        Serial.print(encrypted[i], HEX);
        Serial.print(' ');
    }
}

// Decrypt the passed data (loaded from SD), store in aes_buffer for storage in entry
void Password_Manager::decrypt(byte* encrypted, char* original){
    // Decrypt the encrypted array
    for (int i = 0; i < 32; i += 16){
        aes128->decryptBlock(encrypted + i, encrypted + i);
    }

    // Populate the destination char array
    for (int i = 0; i < 32; i++){
        original[i] = (char) encrypted[i];
    }
}

// Sets the key used to encrypt/decrypt the data, stored in key_bytes
void Password_Manager::setKey(char* key){
    // Convert char to byte array
    int i = 0;
    int copy_index = 0;

    while(i < 16){
        if (key[copy_index] == 0){
            copy_index = 0;
        }

        key_bytes[i] = byte(key[copy_index]);
        Serial.print(key_bytes[i], HEX);
        Serial.print(' ');
        i++;
        copy_index++;
    }
    Serial.println();
    
    // Set key
    aes128->setKey(key_bytes, aes128->keySize());
}

// Starting from position passed, read and decrypt password entry into entry class
void Password_Manager::load(int position){
    char decrypted[32];
    byte encrypted[32];
    int write_address = position;

    // Load the name from eeprom and decrypt
    for (int i = 0; i < 32; i++){
        encrypted[i] = this->eeprom_manager->readExternalEEPROM(write_address);
        Serial.print(this->eeprom_manager->readExternalEEPROM(write_address), HEX);
        Serial.print(' ');
        write_address++;
    }
    Serial.println();

    this->decrypt(encrypted, decrypted);

    Serial.println(decrypted);

    memcpy(entries[this->password_count].getName(), decrypted, 32);

    // Load the username from eeprom and decrypt
    for (int i = 0; i < 32; i++){
        encrypted[i] = this->eeprom_manager->readExternalEEPROM(write_address);
        Serial.print(this->eeprom_manager->readExternalEEPROM(write_address), HEX);
        Serial.print(' ');
        write_address++;
    }
    Serial.println();

    this->decrypt(encrypted, decrypted);

    Serial.println(decrypted);

    memcpy(entries[this->password_count].getEmail(), decrypted, 32);

    // Load the password name from eeprom and decrypt
    for (int i = 0; i < 32; i++){
        encrypted[i] = this->eeprom_manager->readExternalEEPROM(write_address);
        Serial.print(this->eeprom_manager->readExternalEEPROM(write_address), HEX);
        Serial.print(' ');
        write_address++;
    }
    Serial.println();

    this->decrypt(encrypted, decrypted);

    Serial.println(decrypted);

    memcpy(entries[this->password_count].getPassword(), decrypted, 32);

    this->password_count++;
}

// Save the passed password entry to the passed file in the passed position
void Password_Manager::save(Password_Entry* entry){
    Serial.println("SAVING ENTRY...");
    // Get password count from EEPROM and increment
    byte count = this->eeprom_manager->readExternalEEPROM(0);
    this->eeprom_manager->writeExternalEEPROM(0, count+1);

    int write_address = count*(EEPROM_PW_ENTRY_SIZE+2)+1; // Add 2 for type and size bytes

    Serial.print("COUNT: ");
    Serial.println(count);

    // Write the size of this entry
    this->eeprom_manager->writeExternalEEPROM(write_address, EEPROM_PW_ENTRY_SIZE);
    write_address++;

    // Write the type
    this->eeprom_manager->writeExternalEEPROM(write_address, 0);
    write_address++;   

    // Write the encrypted name
    byte encrypted[32];
    this->encrypt(entry->getName(), encrypted);

    for (int i = 0; i < 32; i++){
        this->eeprom_manager->writeExternalEEPROM(write_address, encrypted[i]);
        write_address++;
        Serial.println(write_address);
    }

    // Write the encrypted email/username
    this->encrypt(entry->getEmail(), encrypted);

    for (int i = 0; i < 32; i++){
        this->eeprom_manager->writeExternalEEPROM(write_address, encrypted[i]);
        write_address++;
    }

    // Write the password to the eeprom
    this->encrypt(entry->getPassword(), encrypted);

    for (int i = 0; i < 32; i++){
        this->eeprom_manager->writeExternalEEPROM(write_address, encrypted[i]);
        write_address++;
    }
}

// Password entry class functions
void Password_Entry::setName(char* buffer){
    int i = 0;
    while(buffer[i] != '\0'){
        this->name[i] = buffer[i];
        i++;
    }
}

// Password entry class functions
void Password_Entry::setEmail(char* buffer){
    int i = 0;
    while(buffer[i] != '\0'){
        this->email[i] = buffer[i];
        i++;
    }
}

// Password entry class functions
void Password_Entry::setPassword(char* buffer){
    int i = 0;
    while(buffer[i] != '\0'){
        this->password[i] = buffer[i];
        i++;
    }
}

char* Password_Entry::getName(){
    return this->name;
}

char* Password_Entry::getEmail(){
    return this->email;
}

char* Password_Entry::getPassword(){
    return this->password;
}