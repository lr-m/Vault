#include "Wallet_Manager.h"

Wallet_Manager::Wallet_Manager(Adafruit_ST7735* display, AES128* aes, ST7735_PW_Keyboard* keyboard, EEPROM_Manager* eeprom_manager){
    this->tft = display;
    this->aes128 = aes;

    this->entries = (Wallet_Entry*) malloc(sizeof(Wallet_Entry) * MAX_WALLETS);
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
           
            // Decrypt phrase when needed
            char decrypted[32];
            this->decrypt(this->entries[selected_wallet_index].getEncryptedPhrases()[i], decrypted);
            
            int length = strlen(decrypted);
            tft->setCursor(64 - (length*6)/2, 28 + (i - start_wallet_display_index) * PHRASE_SEP);
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
        keyboard->setLengthLimit(MAX_NAME_LEN);
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

        if (new_phrase_count < MAX_PHRASE_COUNT){
            // Draw triangle at top
            tft->fillTriangle(tft->width()/2, tft->height()/2 + 30, tft->width()/2 - 4, tft->height()/2 + 25, tft->width()/2 + 4, tft->height()/2 + 25, SCHEME_MAIN);
        }
    } else if (stage == 3){ // Let user enter the phrases
        keyboard->reset();
        keyboard->setLengthLimit(MAX_PHRASE_LEN);
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
            if (wallet_count > 1){
                this->remove(this->entries + selected_wallet_index);

                // Reload the display
                this->display();
            }
        }
    } else if (stage == 1){
        // Allow user to cancel
        if (*ir_code == IR_ASTERISK){
            current_phrases_added = 0;
            stage = 0;
            this->display();
            return;
        }

        keyboard->interact(ir_code);

        if (keyboard -> enterPressed()){
            memcpy(this->entries[wallet_count].getName(), keyboard->getCurrentInput(), 32);
            stage++;
            this->new_phrase_count = 12;
            this->display();
        }
    } else if (stage == 2){
        // Allow user to cancel
        if (*ir_code == IR_ASTERISK){
            current_phrases_added = 0;
            stage = 0;
            this->display();
            return;
        }

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

        if (*ir_code == IR_UP && new_phrase_count + 1 <= MAX_PHRASE_COUNT){
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
        // Allow user to cancel
        if (*ir_code == IR_ASTERISK){
            current_phrases_added = 0;
            stage = 0;
            this->display();
            return;
        }

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

void Wallet_Manager::clear(){
    // Clear all the wallet entries from memory
  for (int i = 0; i < MAX_WALLETS; i++){
      for (int j = 0; j < this->entries[j].phrase_count; j++){
          free(this->entries[i].getEncryptedPhrases()[j]);
      }
      this->entries[i].phrase_count = 0;
      this->entries[i].start_address = 0;
  }
}

int Wallet_Manager::getWalletCount(){
    return this->wallet_count;
}
        
void Wallet_Manager::setWalletCount(int new_count){
    this->wallet_count = new_count;
}

Wallet_Entry* Wallet_Manager::getFreeEntry(){
    if (this->wallet_count < MAX_WALLETS) {
        return this->entries + this->wallet_count;
    } else {
        return NULL;
    }
}

int Wallet_Manager::deleteEntry(const char* name){

}

boolean Wallet_Manager::getEscaped(){
    return this->escaped;
}

void Wallet_Manager::setEscaped(boolean val){
    this->escaped = val;
}

void Wallet_Manager::encrypt(char* data, byte* encrypted){
    // Make sure the string has some random padding before its split and encrypted
    int nullterm_index = 0;
    while(data[nullterm_index] != 0)
        nullterm_index++;

    for (int i = nullterm_index + 1; i < 32; i++)
        data[i] = random(255);

    // Do the splitting
    byte to_encrypt_bytes[32];
    int i = 0;
    // First chunk
    for (; i < 12; i++)
        to_encrypt_bytes[i] = byte(data[i]);

    // Random padding for first chunk
    for (; i < 16; i++)
        to_encrypt_bytes[i] = random(255);

    // Second chunk
    for (; i < 28; i++)
        to_encrypt_bytes[i] = byte(data[i-4]);

    // Random padding for second chunk
    for (; i < 32; i++)
        to_encrypt_bytes[i] = random(255);

    // Perform encryption
    for (int i = 0; i < 32; i += 16)
        aes128->encryptBlock(encrypted + i, to_encrypt_bytes + i);
}

void Wallet_Manager::decrypt(byte* encrypted, char* original){
    byte decrypted[32];
    // Decrypt the encrypted array
    for (int i = 0; i < 32; i += 16)
        aes128->decryptBlock(decrypted + i, encrypted + i);

    // Populate the destination char array
    for (int i = 0; i < 12; i++)
        original[i] = (char) decrypted[i];

    for (int i = 16; i < 28; i++)
        original[i-4] = (char) decrypted[i];

    // Pad with null terminators
    for (int i = 24; i < 32; i++)
        original[i] = 0;
}

int Wallet_Manager::load(){
    int count = this->eeprom_manager->readExternalEEPROM(WALLET_COUNT_ADDRESS);
    wallet_count = 0;

    // Iterate over the wallet layouts and load them into entries
    int read_addr = WALLET_LAYOUT_START;
    for (int i = 0; i < count; i++){
        this->entries[i].start_address = read_addr;
        int phrase_count = this->eeprom_manager->readExternalEEPROM(read_addr);
        int name_addr = WALLET_BLOCKS_START + this->eeprom_manager->readExternalEEPROM(read_addr + 1) * WALLET_BLOCK_SIZE;

        char decrypted[32];
        byte encrypted[32];

        // Load the name and decrypt
        for (int j = 0; j < WALLET_BLOCK_SIZE; j++){
            encrypted[j] = this->eeprom_manager->readExternalEEPROM(name_addr);
            name_addr++;
        }

        this->decrypt(encrypted, decrypted);
        memcpy(entries[i].getName(), decrypted, WALLET_BLOCK_SIZE);

        // Load the phrases
        for (int j = 0; j < phrase_count; j++){
            int phrase_addr = WALLET_BLOCKS_START + this->eeprom_manager->readExternalEEPROM(read_addr + 2 + j) * WALLET_BLOCK_SIZE;

            // Load the phrase from EEPROM
            for (int k = 0; k < 32; k++){
                encrypted[k] = this->eeprom_manager->readExternalEEPROM(phrase_addr);
                phrase_addr++;
            }

            // Add to the entry
            this->entries[i].addPhrase(encrypted);
        }

        read_addr += (2 + phrase_count); // Incr read addr by count + name + phrase count places
    }

    wallet_count = count;
}

void Wallet_Manager::save(Wallet_Entry* entry){
    int count = this->eeprom_manager->readExternalEEPROM(WALLET_COUNT_ADDRESS);

    // Create a new entry in the blocks
    int layout_addr = WALLET_LAYOUT_START;
    for (int i = 0; i < count; i++)
        layout_addr += this->eeprom_manager->readExternalEEPROM(layout_addr) + 2; // Go to next phrase count

    entry -> start_address = layout_addr;

    // Start the layout block with the phrase count
    this->eeprom_manager->writeExternalEEPROM(layout_addr, entry->phrase_count);
    layout_addr++;

    // Find free blocks for the name and phrases using the bitmask
    int written_count = 0;
    for (int i = 0; i < WALLET_BITMASK_SIZE; i++){
        // Get the byte that stores the address
        byte mask_byte = this -> eeprom_manager -> readExternalEEPROM(WALLET_BLOCK_BITMASK_START + i);
        for (int j = 0; j < 8; j++){
            // Check each bit to see if it is free
            if (bitRead(mask_byte, j) == 0){
                // At this point, safe to assume that entry will fill this block, so write it to 1
                bitSet(mask_byte, j);
                this->eeprom_manager->writeExternalEEPROM(WALLET_BLOCK_BITMASK_START + i, mask_byte);

                int write_addr = WALLET_BLOCKS_START + ((i*8 + j) * WALLET_BLOCK_SIZE);

                // If written count is 0, need to write the name
                if (written_count == 0){
                    // Write the encrypted name to the block
                    byte encrypted[32];
                    this -> encrypt(entry -> getName(), encrypted);
                    for (int k = 0; k < WALLET_BLOCK_SIZE; k++){
                        this -> eeprom_manager -> writeExternalEEPROM(write_addr, encrypted[k]);
                        write_addr++;
                    }
                } else {
                    // Write the encrypted entry to the block
                    for (int k = 0; k < WALLET_BLOCK_SIZE; k++){
                        this -> eeprom_manager -> writeExternalEEPROM(write_addr, entry -> getEncryptedPhrases()[written_count - 1][k]);
                        write_addr++;
                    }
                }

                // Indicate this block is for this entry
                this->eeprom_manager->writeExternalEEPROM(layout_addr, (byte) i*8 + j);
                layout_addr++;

                // Increment written count, and check if everything has been written (also incr wallet entry count when done)
                written_count++;
                if (written_count == entry->phrase_count + 1){
                    this->eeprom_manager->writeExternalEEPROM(WALLET_COUNT_ADDRESS, count+1);

                    Serial.println("Count:");
                    Serial.println(this->eeprom_manager->readExternalEEPROM(WALLET_COUNT_ADDRESS));
                    Serial.println("\nLayout:");
                    for (int f = 0; f < 100; f++){
                        Serial.print(this->eeprom_manager->readExternalEEPROM(WALLET_LAYOUT_START + f));
                        Serial.print(' ');
                    }
                    Serial.println("\nBlock mask:");
                    for (int f = 0; f < WALLET_BITMASK_SIZE; f++){
                        Serial.print(this->eeprom_manager->readExternalEEPROM(WALLET_BLOCK_BITMASK_START + f));
                        Serial.print(' ');
                    }
                    Serial.println("\nBlocks:");
                    for (int d = 0; d < 20; d++){
                        Serial.print(d);
                        Serial.print(". ");
                        for (int r = 0; r < WALLET_BLOCK_SIZE; r++){
                            Serial.print(this->eeprom_manager->readExternalEEPROM(WALLET_BLOCKS_START + (d * WALLET_BLOCK_SIZE) + r));
                            Serial.print(' ');
                        }
                        Serial.print('\n');
                    }

                    return;
                }
            }
        }
    }
}

void Wallet_Manager::remove(Wallet_Entry* entry){
    int wallet_count = this->eeprom_manager->readExternalEEPROM(WALLET_COUNT_ADDRESS);
    
    // Free the blocks
    int phrase_count = this->eeprom_manager->readExternalEEPROM(entry->start_address);
    for (int i = 0; i < phrase_count + 1; i++){
        // Clear the bitmask element
        int block = this->eeprom_manager->readExternalEEPROM(entry->start_address + 1 + i);
        int mask_byte = this->eeprom_manager->readExternalEEPROM(WALLET_BLOCK_BITMASK_START + (int) block / 8);
        bitClear(mask_byte, block % 8);
        this->eeprom_manager->writeExternalEEPROM(WALLET_BLOCK_BITMASK_START + (int) block / 8, mask_byte);
    }

    // Free the layout space (and move everything after it back down)
    int layout_end_addr = WALLET_LAYOUT_START;
    int entry_size = phrase_count + 2;
    for (int i = 0; i < wallet_count; i++)
        layout_end_addr += this->eeprom_manager->readExternalEEPROM(layout_end_addr) + 2; // Go to next phrase count

    // Shift everything after down
    for (int i = 0; i < layout_end_addr - entry->start_address; i++){
        byte ahead = this->eeprom_manager->readExternalEEPROM(entry->start_address + entry_size + i);
        this->eeprom_manager->writeExternalEEPROM(entry->start_address + i, ahead);
    }

    // Clear the end
    for (int i = layout_end_addr - entry_size; i < layout_end_addr; i++)
        this->eeprom_manager->writeExternalEEPROM(i, 0xff);

    // Reduce the entry counter after deletion
    this->eeprom_manager->writeExternalEEPROM(WALLET_COUNT_ADDRESS, wallet_count-1);

    // Clear and reload entries / set new index
    this->clear();
    this->selected_wallet_index = maxval(0, this->selected_wallet_index-1);
    this->load();
}

void Wallet_Manager::setStage(int stage){
    this->stage = stage;
}

Wallet_Entry* Wallet_Manager::getEntry(byte* name){
    char decrypted_name[32];
    this->decrypt(name, decrypted_name);

    // Get number of wallets stored
    int count = this->eeprom_manager->readExternalEEPROM(WALLET_COUNT_ADDRESS);

    for (int i = 0; i < count; i++){
        if (strcmp(this->entries[i].getName(), decrypted_name) == 0)
            return this->entries + i;
    }
    return NULL;
}

void Wallet_Entry::addPhrase(byte* phrase){
    byte* new_phrase = (byte*) malloc(sizeof(byte) * 32);
    memcpy(new_phrase, phrase, 32);
    this->encrypted_phrases[phrase_count] = new_phrase;
    this->phrase_count++;
}

void Wallet_Entry::addPhrase(const char* phrase){
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

void Wallet_Entry::nullifyPhrases(){
    *this->encrypted_phrases = NULL;
}

void Wallet_Entry::setName(char* buffer){
    int i = 0;
    do {
        this->name[i] = buffer[i];
        i++;
    } while(buffer[i] != 0);
    this->name[i] = 0;
}

void Wallet_Entry::setName(const char* buffer){
    int i = 0;
    do {
        this->name[i] = buffer[i];
        i++;
    } while(buffer[i] != 0);
    this->name[i] = 0;
}

void Wallet_Entry::setEncryptedPhrase(int index, byte* phrase){
    this->encrypted_phrases[index] = phrase;
}
