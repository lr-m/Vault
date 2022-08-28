<p align="center">

  <img src="https://user-images.githubusercontent.com/47477832/186791907-587e704f-8917-4e11-b288-38d5f2dee653.png" width="400">

</p>

<div align="center">

**ESP8266 password & wallet key storage device, uses a 1.77" ST7735 display, IR sensor + remote, and an external EEPROM chip.**

</div>

# VAult
This aims to replicate something like LastPass without the need for an internet connection, lying between online storage and pen and paper. Hashing algorithms and encryption are used to store passwords securely on an external EEPROM, using an IR remote to access them/add new credentials. Access to the unencrypted external EEPROM is protected with a 'master password', which must be remembered.

# Components
- ESP8266 NodeMCU v1.0
- IR Receiver & Remote
- ST7735 1.77" LCD Display
- ATMEL 24C256 EEPROM

# Configuration
<div align="center">

<img src="https://user-images.githubusercontent.com/47477832/187088430-169f1000-e5c6-4c07-87d7-b491e630fd99.png" width="750">

</div>

# Installation

- Set up ESP8266 drivers for Arduino using the Board Manager (2.7.4 tested)
- Install the necessary libraries:
  - Adafruit ST7735 & ST7789 Library : Version 1.9.3
  - IRremote : Version 3.3.0
  - Adafruit GFX Library : Version 1.11.3
- Place the VAULT_Library directory in your Arduino library directory
- Open the VAULT_Sketch.ino file and install on the ESP8266 once configured as above
