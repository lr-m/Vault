#include "Password_Manager.h"

// Password manager class functions
Password_Manager::Password_Manager(Adafruit_ST7735* display, AES128* aes, 
        ST7735_PW_Keyboard* keyboard, EEPROM_Manager* eeprom_manager){
    this->tft = display;
    this->aes128 = aes;

    this->entries = (Password_Entry*) malloc(sizeof(Password_Entry) * PASSWORD_STORAGE_LIMIT);
    this->keyboard = keyboard;

    this->eeprom_manager = eeprom_manager;
}

// Sets the currenty stage of the password manager
void Password_Manager::setStage(int stage){
    this->stage = stage;
}

// Sorts the entries alphabetically (simple bubble sort)
void Password_Manager::sortEntries(){
    Password_Entry temp;
    for (int i = 0; i < password_count; ++i){
        for (int j = 0; j < password_count; ++j){
            if (entries[i].getName()[0] < entries[j].getName()[0]){
                temp.start_address = entries[i].start_address;

                memcpy(temp.getName(), entries[i].getName(), 32);
                memcpy(temp.getEncryptedEmail(), entries[i].getEncryptedEmail(), 32);
                memcpy(temp.getEncryptedPassword(), entries[i].getEncryptedPassword(), 32);

                entries[i].start_address = entries[j].start_address;

                memcpy(entries[i].getName(), entries[j].getName(), 32);
                memcpy(entries[i].getEncryptedEmail(), entries[j].getEncryptedEmail(), 32);
                memcpy(entries[i].getEncryptedPassword(), entries[j].getEncryptedPassword(), 32);

                entries[j].start_address = temp.start_address;

                memcpy(entries[j].getName(), temp.getName(), 32);
                memcpy(entries[j].getEncryptedEmail(), temp.getEncryptedEmail(), 32);
                memcpy(entries[j].getEncryptedPassword(), temp.getEncryptedPassword(), 32);
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

        // Selected triangle indicator
        int base = PASSWORD_START_Y + (selected_pw_index - this -> start_pw_display_index) * PASSWORD_SEP;
        tft->fillTriangle(tft->width() - 7, base, tft->width() - 7, base + PASSWORD_SEP / 2, tft->width() - 15, base + PASSWORD_SEP / 4, SCHEME_MAIN);

        // Display passwords
        tft->setTextColor(ST77XX_WHITE);
        for (int i = this->start_pw_display_index; 
                i < minval(this->password_count, this->start_pw_display_index + PASSWORDS_PER_PAGE); i++){
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

        // Username/email
        tft->setCursor(5, 18);
        tft->setTextColor(SCHEME_MAIN);
        tft->println("\nUsername/Email:\n");
        tft->setTextColor(ST77XX_WHITE);
        char decrypted_email[32];
        this->decrypt(this->entries[this->selected_pw_index].getEncryptedEmail(), decrypted_email);
        tft->println(decrypted_email);

        // Password
        tft->setTextColor(SCHEME_MAIN);
        tft->println("\n\nPassword:\n");
        tft->setTextColor(ST77XX_WHITE);
        char decrypted_pwd[32];
        this->decrypt(this->entries[this->selected_pw_index].getEncryptedPassword(), decrypted_pwd);
        tft->println(decrypted_pwd);

        // Interaction options
        tft->setTextColor(SCHEME_MAIN);
        tft->setCursor(0, tft->height()-12);
        tft->println("1:Overwrite");
        tft->setCursor(tft->width()/2 + 10, tft->height()-12);
        tft->println("2:Delete");
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
                ? start_pw_display_index + minval(PASSWORDS_PER_PAGE - 1, this->password_count - start_pw_display_index - 1)
                : selected_pw_index - 1;

            base = PASSWORD_START_Y + (selected_pw_index - this -> start_pw_display_index) * PASSWORD_SEP;
            tft->fillTriangle(tft->width() - 7, base, tft->width() - 7, base + PASSWORD_SEP / 2, tft->width() - 15, base + PASSWORD_SEP / 4, SCHEME_MAIN);
        } else if (*ir_data == IR_DOWN && this->password_count > 0){ // Move cursor down

            int base = PASSWORD_START_Y + (selected_pw_index - this -> start_pw_display_index) * PASSWORD_SEP;
            tft->fillTriangle(tft->width() - 7, base, tft->width() - 7, base + PASSWORD_SEP / 2, tft->width() - 15, base + PASSWORD_SEP / 4, SCHEME_BG);

            selected_pw_index = selected_pw_index + 1 == minval(start_pw_display_index + PASSWORDS_PER_PAGE, this->password_count)
                ? start_pw_display_index
                : selected_pw_index + 1;

            base = PASSWORD_START_Y + (selected_pw_index - this -> start_pw_display_index) * PASSWORD_SEP;
            tft->fillTriangle(tft->width() - 7, base, tft->width() - 7, base + PASSWORD_SEP / 2, tft->width() - 15, base + PASSWORD_SEP / 4, SCHEME_MAIN);
        } else if (*ir_data == IR_LEFT && this->password_count > 0){ // Scroll to previous page
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
            this->setEscaped(true);
        }
     } else if (this->stage == 1){ // Display details of selected password
        if (*ir_data == IR_HASHTAG){ // Exit selected password
            stage--;
            this->display();
        } else if (*ir_data == IR_ONE){ // Overwrite entry
            stage = 2;
            overwriting = true;
            this->display();
        } else if (*ir_data == IR_TWO && password_count > 1){ // Delete entry
            // Delete entry from memory
            eeprom_manager->clearEntryBit(this->entries[selected_pw_index].start_address);
            eeprom_manager->clear(this->entries[selected_pw_index].start_address, EEPROM_PW_ENTRY_SIZE);

            // Remove the entry from the loaded entries
            memcpy(entries + selected_pw_index, entries + (password_count - 1), sizeof(Password_Entry));

            password_count--;
            selected_pw_index = 0;
            stage--;

            this->display();
        }
    } else if (this->stage == 2){ // Allow user to enter new password name
        keyboard->interact(ir_data);

        if (keyboard -> enterPressed()){
            if (!this->overwriting){
                memcpy(this->entries[password_count].getName(), keyboard->getCurrentInput(), 32);
            } else {
                memcpy(this->entries[selected_pw_index].getName(), keyboard->getCurrentInput(), 32);
            }
            stage++;
            this->display();
        }
    } else if (this->stage == 3){ // Allow user to enter new email/username
        keyboard->interact(ir_data);

        if (keyboard -> enterPressed()){
            byte encrypted[32];
            this->encrypt(keyboard->getCurrentInput(), encrypted);
            if (!this->overwriting){
                memcpy(this->entries[password_count].getEncryptedEmail(), encrypted, 32);
            } else {
                memcpy(this->entries[selected_pw_index].getEncryptedEmail(), encrypted, 32);
            }
            stage++;
            this->display();
        }
    } else if (this->stage == 4){ // Allow user to enter new password
        keyboard->interact(ir_data);

        if (keyboard -> enterPressed()){
            byte encrypted[32];
            this->encrypt(keyboard->getCurrentInput(), encrypted);
            if (!this->overwriting){
                memcpy(this->entries[password_count].getEncryptedPassword(), encrypted, 32);

                password_count++;

                this->save(&this->entries[password_count-1]);
            } else {
                memcpy(this->entries[selected_pw_index].getEncryptedPassword(), encrypted, 32);

                this->save(&this->entries[selected_pw_index]); // Don't need to clear entry bit as local overwrite mode
            }

            this->sortEntries();

            // Write added password store to EEPROM
            stage = 0;
            this->display();
        }
    }
} 

void Password_Manager::sanitiseInput(const char* input, char* result, int buf_size){
    // Sanitise the inputs with intermediate buffer, ensures no overflows, ensures terminator, random padding
    for (int i = 0; i < buf_size; i++){
        if (input[i] != 0){
            result[i] = input[i];
        } else {
            result[i] = 0;
            for (int j = i+1; j < buf_size; j++){
                result[j] = random(0, 255);
            }
            break;
        }
        result[buf_size-1] = 0;
    }
}

Password_Entry* Password_Manager::getEntry(byte* name){
    char decrypted_name[32];
    this->decrypt(name, decrypted_name);
    
    for (int i = 0; i < this->password_count; i++){
        if (strcmp(this->entries[i].getName(), decrypted_name) == 0){
            return this->entries + i;
        }
    }
    return NULL;
}

boolean Password_Manager::getEscaped(){
    return this->escaped;
}

void Password_Manager::setEscaped(boolean val){
    this->escaped = val;
}

int Password_Manager::addEntry(byte* enc_name, byte* enc_user, byte* enc_pwd){
    if (password_count < PASSWORD_STORAGE_LIMIT){
        // Decrypt and save name
        char decrypted_name[32];
        this->decrypt(enc_name, decrypted_name);
        memcpy(this->entries[password_count].getName(), decrypted_name, 32);

        // Copy email
        memcpy(this->entries[password_count].getEncryptedEmail(), enc_user, 32);

        // Copy password
        memcpy(this->entries[password_count].getEncryptedPassword(), enc_pwd, 32);

        this->save(&this->entries[password_count]);
        password_count++;  

        this->sortEntries();

        return 0;
    }
    return -1;
}

int Password_Manager::deleteEntry(byte* name){
    char decrypted_name[32];
    this->decrypt(name, decrypted_name);

    // Get index of entry
    int ind = -1;
    for (int i = 0; i < this->password_count; i++){
        if (strcmp(this->entries[i].getName(), decrypted_name) == 0){
            ind = i;
            break;
        }
    }

    // Return -1 if not found
    if (ind == -1)
        return -1;

    // Delete entry from memory
    eeprom_manager->clearEntryBit(this->entries[ind].start_address);
    eeprom_manager->clear(this->entries[ind].start_address, EEPROM_PW_ENTRY_SIZE);

    // Remove the entry from the loaded entries
    if (ind < password_count-1){
        memcpy(entries + ind, entries + (password_count - 1), sizeof(Password_Entry));
    }

    password_count--;
    selected_pw_index = 0;

    this->sortEntries();

    return 0;
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

    // Random padding salt (helps hide identical passwords)
    while (i < 32){
        data_bytes[i] = random(255);
        i++;
    }

    // Perform encryption
    for (int i = 0; i < 32; i += 16)
        aes128->encryptBlock(encrypted + i, data_bytes + i);
}

// Decrypt the passed data (loaded from SD), store in aes_buffer for storage in entry
void Password_Manager::decrypt(byte* encrypted, char* original){
    byte decrypted[32];
    // Decrypt the encrypted array
    for (int i = 0; i < 32; i += 16)
        aes128->decryptBlock(decrypted + i, encrypted + i);

    // Populate the destination char array
    for (int i = 0; i < 32; i++)
        original[i] = (char) decrypted[i];
}

// Starting from position passed, read and decrypt password entry into entry class
int Password_Manager::load(){
    for (int i = 0; i < 32; i++){
        byte mask_byte = this->eeprom_manager->readExternalEEPROM(PWD_BITMASK_START + i);
        for (int j = 0; j < 8; j++){
            if (bitRead(mask_byte, j) == byte(1)){
                int position = PASSWORD_ENTRY_START + ((i*8 + j)*EEPROM_PW_ENTRY_SIZE);

                char decrypted[32];
                byte encrypted[32];
                int write_address = position;

                entries[this->password_count].start_address = position; // For overwriting

                // Load the name from eeprom and decrypt
                for (int i = 0; i < 32; i++){
                    encrypted[i] = this->eeprom_manager->readExternalEEPROM(write_address);
                    write_address++;
                }

                this->decrypt(encrypted, decrypted);

                memcpy(entries[this->password_count].getName(), decrypted, 32);

                // Load the username from eeprom and save encrypted version
                for (int i = 0; i < 32; i++){
                    encrypted[i] = this->eeprom_manager->readExternalEEPROM(write_address);
                    write_address++;
                }

                memcpy(entries[this->password_count].getEncryptedEmail(), encrypted, 32);

                // Load the password name from eeprom and save encrypted version
                for (int i = 0; i < 32; i++){
                    encrypted[i] = this->eeprom_manager->readExternalEEPROM(write_address);
                    write_address++;
                }

                memcpy(entries[this->password_count].getEncryptedPassword(), encrypted, 32);

                this->password_count++;
            }
        }
    }
    return this->password_count;
}

byte* Password_Entry::getEncryptedEmail(){
    return this->encryptedEmail;
}

byte* Password_Entry::getEncryptedPassword(){
    return this->encryptedPassword;
}

// Save the passed password entry to the passed file in the passed position
void Password_Manager::save(Password_Entry* entry){
    int write_address;

    if (!this->overwriting){ // Add new password
        write_address = this->eeprom_manager->getNextFreeAddress();

        entry->start_address = write_address;
    } else { // For overwriting selected password
        write_address = this->entries[selected_pw_index].start_address;

        this->overwriting = false;
    }

    // Write the encrypted name
    byte encrypted[32];
    this->encrypt(entry->getName(), encrypted);

    for (int i = 0; i < 32; i++){
        this->eeprom_manager->writeExternalEEPROM(write_address, encrypted[i]);
        write_address++;
    }

    // Write the encrypted email/username
    for (int i = 0; i < 32; i++){
        this->eeprom_manager->writeExternalEEPROM(write_address, entry->getEncryptedEmail()[i]);
        write_address++;
    }

    // Write the password to the eeprom
    for (int i = 0; i < 32; i++){
        this->eeprom_manager->writeExternalEEPROM(write_address,entry->getEncryptedPassword()[i]);
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
void Password_Entry::setEncryptedEmail(char* buffer){
    memcpy(this->getEncryptedEmail(), buffer, 32);
}

// Password entry class functions
void Password_Entry::setEncryptedPassword(char* buffer){
    memcpy(this->getEncryptedPassword(), buffer, 32);
}

char* Password_Entry::getName(){
    return this->name;
}