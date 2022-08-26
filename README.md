<p align="center">

  <img src="https://user-images.githubusercontent.com/47477832/186791907-587e704f-8917-4e11-b288-38d5f2dee653.png" width="400">

  

</p>

<div align="center">

**ESP8266 password & wallet key storage device, uses a 1.77" ST7735 display, IR sensor + remote, and an external EEPROM chip.**

</div>

# VAult
This aims to replicate something like LastPass without the need for an internet connection, lying between online storage and pen and paper. Hashing algorithms and encryption are used to store passwords securely on an external EEPROM, using an IR remote to access them/add new credentials. Access to the unencrypted external EEPROM is protected with a 'master password', which must be remembered.
