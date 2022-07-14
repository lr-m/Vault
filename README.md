# ESP8266_Password_Store
This aims to replicate something like LastPass without the need for an internet connection, lying between online storage and written storage. Hashing algorithms and encryption are used to store passwords securely on an external EEPROM, and using an IR remote to access them/add new credentials. Access to the unencrypted external EEPROM is protected with a 'master password', which must be remembered.
