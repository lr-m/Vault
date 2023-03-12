<p align="center">

  <img src="https://user-images.githubusercontent.com/47477832/186791907-587e704f-8917-4e11-b288-38d5f2dee653.png" width="400">

</p>

<div align="center">

**ESP8266 password & wallet key storage device, uses a 1.77" ST7735 display, IR sensor + remote, and an external EEPROM chip.**

</div>

# VAult
This aims to replicate something like LastPass on a small device that can operate independently and with no forced internet connection, lying between online storage and pen and paper. Hashing and encryption are used to store passwords securely on an external EEPROM, using an IR remote to access them/add new credentials. Access to the unencrypted external EEPROM is protected with a 'master password', which must be remembered. The vault also has an optional remote mode to make interacting with entries smoother and less time-consuming.

# Features
- Can store usernames/emails and passwords up to 24 digits
- Can store cryptocurrency wallet key phrases (number of phrases is selected, 1-24)
- Master password hashed with SHA-512
- All components of wallet entries and passwords encrypted with the master password using AES-ECB, entries are padded with random characters so identical passwords have different outputs (ECB should be fine for this application as information about the plaintext cannot be derived from the encrypted version due to the fixed size and the random padding)
- Can delete and overwrite existing entries
- No internet connection necessary, but can be attached to a network for remote mode
- WiFi credentials encrypted with master key on storage

# Components
- ESP8266 NodeMCU v1.0
- IR Receiver & Remote
- ST7735 1.77" LCD Display
- ATMEL 24C256 EEPROM - 32kB

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
  - Crypto : Version 0.4.0
  - ArduinoJSON : Version 6.19.4
  - [base64.hpp](https://github.com/Densaugeo/base64_arduino) - I created a base64 directory in my Arduino library directory, and put the contents of the github in there
- Place the VAULT_Library (and the base64 directory if not done yet) in your Arduino library directory
- Open the VAULT_Sketch.ino file and install on the ESP8266 once configured as above

# Setup/Use

- When the device turns on for the first time, it checks if both of the desired magic values are in the internal and external EEPROM, if these magic's aren't present, it performs the setup
- You are now prompted to enter a 'master password' - DO NOT FORGET THIS
- Use the IR remote and keyboard to enter your desired password:
<p align="center">
  <img src="https://user-images.githubusercontent.com/47477832/224544991-baf23992-dae4-415b-ae16-5dadc5905bd4.jpg" width="250">
</p>

- You can now add your WiFi credentials, if you don't want to use the remote functionality, just leave these empty
- Once this setup is complete, you will not have to do it again, and you can now add your passwords with the small '+' buttons by either the wallet or password buttons
- Once your entries are added, you can use the 'Wallet' and 'Password' buttons to view those entries
- Here is an example password entry:
<p align="center">
  <img src="https://user-images.githubusercontent.com/47477832/224544980-7f7974e7-e93a-4041-90cc-d98cacf07f30.PNG" width="500">
</p>

- Here is an example wallet entry:
<p align="center">
  <img src="https://user-images.githubusercontent.com/47477832/224544963-e6bc8c7d-7c53-4a39-b3a2-b9e6d186774b.PNG" width="500">
</p>

# Remote Mode

- The remote mode uses a Python client to interact with the vault over a network
- When remote mode is entered, two important things will appear on the screen of the vault - the IP address of the vault, and the session key
- On your pc connected to the network, open a terminal, and run the python client with the following command:
`python3 ./vault.py -ip *IP ADDRESS* -s *SESSION KEY* -m *MASTER PASSWORD*`
- This will start the python client, and list the possible commands that can be used to manipulate entries on the vault remotely:
![init](https://user-images.githubusercontent.com/47477832/224545014-1e09835c-351a-4d59-a80c-43ba6efc18e5.PNG)

- If you are getting errors when trying to interact with the vault, double check the keys and IP addresses you used are correct, and make sure the vault is in a place with a strong internet connection

*Use this project at your own risk!*
