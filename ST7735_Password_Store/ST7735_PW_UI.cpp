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
    this->items = (ST7735_PW_Menu_Item*) malloc(sizeof(ST7735_PW_Menu_Item) * 2);
    this->selected_index = 0;
    this->items[0] = ST7735_PW_Menu_Item(display, (const char *) "Passwords");
    this->items[1] = ST7735_PW_Menu_Item(display, "Wallet Keys");
}

void ST7735_PW_Menu::interact(uint32_t* ir_data, Password_Manager* password_manager, Wallet_Manager* wallet_manager){
  if (this->entered){
    if (selected_index == 0){
        password_manager->interact(ir_data);

        if (password_manager->escaped){
            password_manager->escaped = false;
            this->entered = false;
            tft->fillScreen(SCHEME_BG);
            this->display();
        }
    } else if (selected_index == 1){
        wallet_manager->interact(ir_data);

        if (wallet_manager->escaped){
            wallet_manager->escaped = false;
            this->entered = false;
            tft->fillScreen(SCHEME_BG);
            this->display();
        }
    }
  } else {
    // DOWN
    if (*ir_data == IR_DOWN){
        int old = selected_index;
        selected_index = selected_index + 1 == 2 ? 0 : selected_index + 1;
        this->items[old].display(old);
        this->items[old].displayAdd(old);
        this->items[selected_index].displaySelected(selected_index);
        this->items[selected_index].addSelected = false;
    }

    // UP
    if (*ir_data == IR_UP){
        int old = selected_index;
        selected_index = selected_index - 1 == -1 ? 1 : selected_index - 1;
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
}

// Display the key
void ST7735_PW_Menu::display(){
    tft->setCursor(0, 5);
    
    for (int i = 0; i < 2; i++){
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
}

void Password_Manager::setStage(int stage){
    this->stage = stage;
}

void Password_Manager::sortEntries(){
    Password_Entry temp;
    for (int i = 0; i < password_count; ++i){
        for (int j = 0; j < password_count; ++j){
            if (entries[i].getName()[0] < entries[j].getName()[0]){
                memcpy(temp.getName(), entries[i].getName(), 32);
                memcpy(temp.getEmail(), entries[i].getEmail(), 32);
                memcpy(temp.getPassword(), entries[i].getPassword(), 32);

                memcpy(entries[i].getName(), entries[j].getName(), 32);
                memcpy(entries[i].getEmail(), entries[j].getEmail(), 32);
                memcpy(entries[i].getPassword(), entries[j].getPassword(), 32);

                memcpy(entries[j].getName(), temp.getName(), 32);
                memcpy(entries[j].getEmail(), temp.getEmail(), 32);
                memcpy(entries[j].getPassword(), temp.getPassword(), 32);
            }
        }
    }
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

        if (this->password_count == 0){
            return;
        }

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
        if (*ir_data == IR_UP && this->password_count > 0){ // Move cursor up
            int base = PASSWORD_START_Y + (selected_pw_index - this -> start_pw_display_index) * PASSWORD_SEP;
            tft->fillTriangle(tft->width() - 7, base, tft->width() - 7, base + PASSWORD_SEP / 2, tft->width() - 15, base + PASSWORD_SEP / 4, SCHEME_BG);

            selected_pw_index = selected_pw_index - 1 < start_pw_display_index 
                ? start_pw_display_index + min(PASSWORDS_PER_PAGE - 1, this->password_count - start_pw_display_index - 1)
                : selected_pw_index - 1;

            base = PASSWORD_START_Y + (selected_pw_index - this -> start_pw_display_index) * PASSWORD_SEP;
            tft->fillTriangle(tft->width() - 7, base, tft->width() - 7, base + PASSWORD_SEP / 2, tft->width() - 15, base + PASSWORD_SEP / 4, SCHEME_MAIN);
        } else if (*ir_data == IR_DOWN && this->password_count > 0){ // Move cursor down

            int base = PASSWORD_START_Y + (selected_pw_index - this -> start_pw_display_index) * PASSWORD_SEP;
            tft->fillTriangle(tft->width() - 7, base, tft->width() - 7, base + PASSWORD_SEP / 2, tft->width() - 15, base + PASSWORD_SEP / 4, SCHEME_BG);

            selected_pw_index = selected_pw_index + 1 == min(start_pw_display_index + PASSWORDS_PER_PAGE, this->password_count)
                ? start_pw_display_index
                : selected_pw_index + 1;

            base = PASSWORD_START_Y + (selected_pw_index - this -> start_pw_display_index) * PASSWORD_SEP;
            tft->fillTriangle(tft->width() - 7, base, tft->width() - 7, base + PASSWORD_SEP / 2, tft->width() - 15, base + PASSWORD_SEP / 4, SCHEME_MAIN);
        } else if (*ir_data == IR_LEFT && this->password_count > 0){ // Move cursor up
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
        } else if (*ir_data == IR_RIGHT && this->password_count > 0){ // Scroll to next page
            start_pw_display_index = start_pw_display_index + PASSWORDS_PER_PAGE >= password_count 
                ? 0 
                : start_pw_display_index + PASSWORDS_PER_PAGE;

            this -> selected_pw_index = start_pw_display_index;

            this->display();
        }  else if (*ir_data == IR_OK && this->password_count > 0){ // Move to next step (display password)
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

            this->save(&this->entries[password_count-1]);

            this->sortEntries();

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

    int i = 0;
    while(data[i] != 0 && i < 31){
        data_bytes[i] = byte(data[i]);
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
        i++;
        copy_index++;
    }
    
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
        write_address++;
    }

    this->decrypt(encrypted, decrypted);

    memcpy(entries[this->password_count].getName(), decrypted, 32);

    // Load the username from eeprom and decrypt
    for (int i = 0; i < 32; i++){
        encrypted[i] = this->eeprom_manager->readExternalEEPROM(write_address);
        write_address++;
    }

    this->decrypt(encrypted, decrypted);

    memcpy(entries[this->password_count].getEmail(), decrypted, 32);

    // Load the password name from eeprom and decrypt
    for (int i = 0; i < 32; i++){
        encrypted[i] = this->eeprom_manager->readExternalEEPROM(write_address);
        write_address++;
    }

    this->decrypt(encrypted, decrypted);

    memcpy(entries[this->password_count].getPassword(), decrypted, 32);

    this->password_count++;
}

// Save the passed password entry to the passed file in the passed position
void Password_Manager::save(Password_Entry* entry){
    // Get password count from EEPROM and increment
    byte count = this->eeprom_manager->readExternalEEPROM(0);
    this->eeprom_manager->writeExternalEEPROM(0, count+1);

    int write_address = this->eeprom_manager->getNextFreeAddress(count);

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


Wallet_Manager::Wallet_Manager(Adafruit_ST7735* display, AES128* aes, ST7735_PW_Keyboard* keyboard, EEPROM_Manager* eeprom_manager){
    this->tft = display;
    this->aes128 = aes;

    this->entries = (Wallet_Entry*) malloc(sizeof(Wallet_Entry) * 8);
    this->keyboard = keyboard;

    this->eeprom_manager = eeprom_manager;
}

void Wallet_Manager::display(){
    tft->fillScreen(SCHEME_BG);
    if (this->stage == 0){ // Display wallet names to select
        tft->fillRect(0, 0, tft->width(), 13, SCHEME_MAIN);

        // Title
        tft->setCursor(3, 3);
        tft->setTextColor(ST77XX_WHITE);

        if (this->wallet_count == 0){
            tft->print("No Wallets Saved");
            return;
        }

        tft->print(this->entries[selected_wallet_index].getName());

        // Draw current wallet number
        tft->setCursor(tft->width()-20, 3);
        tft->print(selected_wallet_index + 1);
        tft->print('/');
        tft->print(wallet_count);

        // Draw triangles to indicate more entries if there are any
        if (start_wallet_display_index > 0){
            // Draw triangle at top
            tft->fillTriangle(tft->width()/2, 18, tft->width()/2 - 4, 22, tft->width()/2 + 4, 22, SCHEME_MAIN);
        }

        // Draw the phrases on the screen
        for (int i = start_wallet_display_index; i < min(start_wallet_display_index + PHRASES_PER_PAGE, this->entries[selected_wallet_index].phrase_count); i++){
            tft->setCursor(10, 28 + (i - start_wallet_display_index) * PHRASE_SEP);
            tft->print(i+1);
            tft->print(".");
            tft->setCursor(32, 28 + (i - start_wallet_display_index) * PHRASE_SEP);
            tft->print(this->entries[selected_wallet_index].getPhrases()[i]);
        }
        
        // Draw bottom triangle if possible
        if (start_wallet_display_index + PHRASES_PER_PAGE < this->entries[selected_wallet_index].phrase_count){
            // Draw triangle at bottom
            // Draw triangle at top
            tft->fillTriangle(tft->width()/2, tft->height()-4, tft->width()/2 - 4, tft->height()-8, tft->width()/2 + 4, tft->height()-8, SCHEME_MAIN);
        }
    } else if (stage == 1){ // Let user enter wallet names
        keyboard->reset();
        keyboard->setLengthLimit(16);
        keyboard->displayPrompt("Enter wallet name:");
        keyboard->display();
    } else if (stage == 2){ // Let user enter number of entries (limit 24)
        tft->fillRect(0, 0, tft->width(), 13, SCHEME_MAIN);

        // Title
        tft->setCursor(3, 3);
        tft->setTextColor(ST77XX_WHITE);
        tft->print("Enter phrase count:");

        // Draw triangles to indicate more entries if there are any
       
        if (new_phrase_count > 0){
            // Draw triangle at bottom
            tft->fillTriangle(tft->width()/2, tft->height()/2 - 30, tft->width()/2 - 4, tft->height()/2 - 25, tft->width()/2 + 4, tft->height()/2 - 25, SCHEME_MAIN);
        }
        
        if (new_phrase_count < 10){
            tft->setCursor(tft->width()/2 - 4, tft->height()/2 - 8);
        } else {
            tft->setCursor(tft->width()/2 - 10, tft->height()/2 - 8);
        }
        
        tft->setTextSize(2);
        tft->setTextColor(ST77XX_WHITE);
        tft->print(new_phrase_count);
        tft->setTextSize(1);

        if (new_phrase_count < 32){
            // Draw triangle at top
            tft->fillTriangle(tft->width()/2, tft->height()/2 + 30, tft->width()/2 - 4, tft->height()/2 + 25, tft->width()/2 + 4, tft->height()/2 + 25, SCHEME_MAIN);
        }
    } else if (stage == 3){ // Let user enter the phrases
        keyboard->reset();
        keyboard->setLengthLimit(15);
        keyboard->displayPrompt("Enter phrase ");
        tft->print(current_phrases_added+1);
        tft->print('/');
        tft->print(new_phrase_count);
        tft->print(':');
        keyboard->display();
    }
}

void Wallet_Manager::interact(uint32_t* ir_code){
    if (this->stage == 0){
        if (*ir_code == IR_DOWN && start_wallet_display_index + PHRASES_PER_PAGE < this->entries[selected_wallet_index].phrase_count){
            start_wallet_display_index+=PHRASES_PER_PAGE;
            this->display();
        }

        if (*ir_code == IR_UP && start_wallet_display_index > 0){
            start_wallet_display_index-=PHRASES_PER_PAGE;
            this->display();
        }

        if (*ir_code == IR_RIGHT){
            start_wallet_display_index = 0;
            selected_wallet_index = selected_wallet_index + 1 == wallet_count ? 0 : selected_wallet_index + 1;
            this->display();
        }

        if (*ir_code == IR_LEFT){
            start_wallet_display_index = 0;
            selected_wallet_index = selected_wallet_index - 1 == -1 ? wallet_count - 1 : selected_wallet_index - 1;
            this->display();
        }

        if (*ir_code == IR_HASHTAG){ // Exit selected password
            this->escaped = true;
        }
    } else if (stage == 1){
        keyboard->interact(ir_code);

        if (keyboard -> enterPressed()){
            memcpy(this->entries[wallet_count].getName(), keyboard->getCurrentInput(), 32);
            stage++;
            this->new_phrase_count = 12;
            this->display();
        }
    } else if (stage == 2){
        if (*ir_code == IR_DOWN && new_phrase_count - 1 > 0){
            new_phrase_count--;

            tft->fillRect(tft->width()/2 - 15, tft->height()/2 - 15, 30, 30, SCHEME_BG);
            if (new_phrase_count < 10){
                tft->setCursor(tft->width()/2 - 4, tft->height()/2 - 8);
            } else {
                tft->setCursor(tft->width()/2 - 10, tft->height()/2 - 8);
            }
            
            tft->setTextSize(2);
            tft->setTextColor(ST77XX_WHITE);
            tft->print(new_phrase_count);
            tft->setTextSize(1);
        }

        if (*ir_code == IR_UP && new_phrase_count + 1 <= 32){
            new_phrase_count++;

            tft->fillRect(tft->width()/2 - 15, tft->height()/2 - 15, 30, 30, SCHEME_BG);
            if (new_phrase_count < 10){
                tft->setCursor(tft->width()/2 - 4, tft->height()/2 - 8);
            } else {
                tft->setCursor(tft->width()/2 - 10, tft->height()/2 - 8);
            }
            
            tft->setTextSize(2);
            tft->setTextColor(ST77XX_WHITE);
            tft->print(new_phrase_count);
            tft->setTextSize(1);
        }

        if (*ir_code == IR_OK){
            stage++;
            this->display();
        }
    } else if (stage == 3){
        keyboard->interact(ir_code);

        if (keyboard -> enterPressed()){
            this->entries[wallet_count].addPhrase(keyboard->getCurrentInput());
            current_phrases_added++;

            if (current_phrases_added < new_phrase_count){
                this->display();
            } else {
                // Save to EEPROM
                this->save(&(this->entries[wallet_count]));
                
                current_phrases_added = 0;
                wallet_count++;
                stage = 0;
                this->display();
            }
        }
    }
}

void Wallet_Manager::encrypt(char* decrypted, byte* encrypted){
    // Convert char to byte array
    byte data_bytes[32];

    int i = 0;
    while(decrypted[i] != 0 && i < 31){
        data_bytes[i] = byte(decrypted[i]);
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
}

void Wallet_Manager::decrypt(byte* encrypted, char* original){
    // Decrypt the encrypted array
    for (int i = 0; i < 32; i += 16){
        aes128->decryptBlock(encrypted + i, encrypted + i);
    }

    // Populate the destination char array
    for (int i = 0; i < 32; i++){
        original[i] = (char) encrypted[i];
    }
}

void Wallet_Manager::load(int position){
    char decrypted[32];
    byte encrypted[32];

    int phrase_load_count = this->eeprom_manager->readExternalEEPROM(position-2);

    // Load the name
    int write_address = position;

    // Load the name from eeprom and decrypt
    for (int i = 0; i < 32; i++){
        encrypted[i] = this->eeprom_manager->readExternalEEPROM(write_address);
        write_address++;
    }

    this->decrypt(encrypted, decrypted);

    memcpy(entries[wallet_count].getName(), decrypted, 32);

    // Load the phrases
    for (int i = 0; i < phrase_load_count; i++){
        // Load the phrase from EEPROM
        for (int j = 0; j < 32; j++){
            encrypted[j] = this->eeprom_manager->readExternalEEPROM(write_address);
            write_address++;
        }

        // Decrypt the phrase
        this->decrypt(encrypted, decrypted);

        // Add to the entry
        this->entries[wallet_count].addPhrase(decrypted);
    }

    wallet_count++;
}

void Wallet_Manager::save(Wallet_Entry* entry){
    // Get password count from EEPROM and increment
    byte count = this->eeprom_manager->readExternalEEPROM(0);
    this->eeprom_manager->writeExternalEEPROM(0, count+1);

    int write_address = this->eeprom_manager->getNextFreeAddress(count);

    // Write the size of this entry (number of phrases)
    this->eeprom_manager->writeExternalEEPROM(write_address, entry->phrase_count);
    write_address++;

    // Write the type
    this->eeprom_manager->writeExternalEEPROM(write_address, 1);
    write_address++;   

    // Write the encrypted name
    byte encrypted[32];
    this->encrypt(entry->getName(), encrypted);

    for (int i = 0; i < 32; i++){
        this->eeprom_manager->writeExternalEEPROM(write_address, encrypted[i]);
        write_address++;
    }

    // Write the encrypted phrases to memory
    byte encrypted_phrase[32];
    for (int i = 0; i < entry->phrase_count; i++){
        this->encrypt(entry->getPhrases()[i], encrypted_phrase);

        for (int i = 0; i < 32; i++){
            this->eeprom_manager->writeExternalEEPROM(write_address, encrypted_phrase[i]);
            write_address++;
        }
    }
}

void Wallet_Manager::setStage(int stage){
    this->stage = stage;
}

void Wallet_Entry::addPhrase(char* phrase){
    char* new_phrase = (char*) malloc(sizeof(char) * 32);
    memcpy(new_phrase, phrase, 32);
    this->phrases[phrase_count] = new_phrase;
    this->phrase_count++;
}

char* Wallet_Entry::getName(){
    return this->name;
}

char** Wallet_Entry::getPhrases(){
    return this->phrases;
}

void Wallet_Entry::setName(char* buffer){
    int i = 0;
    while(buffer[i] != '\0'){
        this->name[i] = buffer[i];
        i++;
    }
}