#include "Wallet_Manager.h"

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
        for (int i = start_wallet_display_index; i < minval(start_wallet_display_index + PHRASES_PER_PAGE, this->entries[selected_wallet_index].phrase_count); i++){
            tft->setCursor(10, 28 + (i - start_wallet_display_index) * PHRASE_SEP);
            tft->print(i+1);
            tft->print(".");
            tft->setCursor(32, 28 + (i - start_wallet_display_index) * PHRASE_SEP);
            
            // Decrypt phrase when needed
            char decrypted[32];
            this->decrypt(this->entries[selected_wallet_index].getEncryptedPhrases()[i], decrypted);
            tft->print(decrypted);
        }
        
        // Draw bottom triangle if possible
        if (start_wallet_display_index + PHRASES_PER_PAGE < this->entries[selected_wallet_index].phrase_count){
            // Draw triangle at bottom
            // Draw triangle at top
            tft->fillTriangle(tft->width()/4, tft->height()-4, tft->width()/4 - 4, tft->height()-8, tft->width()/4 + 4, tft->height()-8, SCHEME_MAIN);
        }

        tft->setTextColor(SCHEME_MAIN);
        tft->setCursor(tft->width()/2, tft->height()-12);
        tft->print("2: Delete");
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
            this->setEscaped(true);
        }

        if (*ir_code == IR_TWO){
            // Delete wallet from EEPROM
            eeprom_manager->deleteWalletEntry(this->entries[selected_wallet_index].start_address, WALLET_MAX_PHRASE_SIZE + 1 + this->entries[selected_wallet_index].phrase_count * WALLET_MAX_PHRASE_SIZE);

            ESP.restart(); // Must restart as to not fragment the heap
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
            byte encrypted[32];
            this->encrypt(keyboard->getCurrentInput(), encrypted);
            this->entries[wallet_count].addPhrase(encrypted);
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

boolean Wallet_Manager::getEscaped(){
    return this->escaped;
}

void Wallet_Manager::setEscaped(boolean val){
    this->escaped = val;
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
    byte decrypted[32];

    // Decrypt the encrypted array
    for (int i = 0; i < 32; i += 16){
        aes128->decryptBlock(decrypted + i, encrypted + i);
    }

    // Populate the destination char array
    for (int i = 0; i < 32; i++){
        original[i] = (char) decrypted[i];
    }
}

int Wallet_Manager::load(){
    int count = this->eeprom_manager->readExternalEEPROM(WALLET_COUNT_ADDRESS);
    int position = WALLET_START_ADDRESS;

    for (int i = 0; i < count; i++){
        char decrypted[32];
        byte encrypted[32];

        entries[wallet_count].start_address = position;

        int phrase_load_count = this->eeprom_manager->readExternalEEPROM(position);

        // Load the name
        int write_address = position+1;

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

            // Add to the entry
            this->entries[wallet_count].addPhrase(encrypted);
        }

        wallet_count++;

        position += 1 + (phrase_load_count*WALLET_MAX_PHRASE_SIZE) + WALLET_MAX_PHRASE_SIZE;
    }

    return wallet_count;
}

void Wallet_Manager::save(Wallet_Entry* entry){
    int write_address = this->eeprom_manager->getNextFreeWalletAddress(); // Get available address

    entry->start_address = write_address; // Set start address

    // Write the size of this entry (number of phrases)
    this->eeprom_manager->writeExternalEEPROM(write_address, entry->phrase_count);
    write_address++;

    // Write the encrypted name
    byte encrypted[32];
    this->encrypt(entry->getName(), encrypted);

    for (int i = 0; i < 32; i++){
        this->eeprom_manager->writeExternalEEPROM(write_address, encrypted[i]);
        write_address++;
    }

    // Write the encrypted phrases to memory
    for (int i = 0; i < entry->phrase_count; i++){
        for (int j = 0; j < 32; j++){
            this->eeprom_manager->writeExternalEEPROM(write_address, entry->getEncryptedPhrases()[i][j]);
            write_address++;
        }
    }

    int new_count = this->eeprom_manager->readExternalEEPROM(WALLET_COUNT_ADDRESS) + 1;
    this->eeprom_manager->writeExternalEEPROM(WALLET_COUNT_ADDRESS, new_count);
}

void Wallet_Manager::setStage(int stage){
    this->stage = stage;
}

Wallet_Entry* Wallet_Manager::getEntry(const char* name){
    // Get number of wallets stored
    int count = this->eeprom_manager->readExternalEEPROM(WALLET_COUNT_ADDRESS);

    for (int i = 0; i < count; i++){
        if (strcmp(this->entries[i].getName(), name) == 0){
            return this->entries + i;
        }
    }
    return NULL;
}

void Wallet_Entry::addPhrase(byte* phrase){
    byte* new_phrase = (byte*) malloc(sizeof(byte) * 32);
    memcpy(new_phrase, phrase, 32);
    this->encrypted_phrases[phrase_count] = new_phrase;
    this->phrase_count++;
}

char* Wallet_Entry::getName(){
    return this->name;
}

byte** Wallet_Entry::getEncryptedPhrases(){
    return this->encrypted_phrases;
}

void Wallet_Entry::setName(char* buffer){
    int i = 0;
    while(buffer[i] != '\0'){
        this->name[i] = buffer[i];
        i++;
    }
}
