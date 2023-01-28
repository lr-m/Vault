import socket
import time
from termcolor import colored
from pyfiglet import Figlet
from random import randint
import argparse
import hashlib
from Crypto.Cipher import AES
from enum import IntEnum
import json
import binascii
import base64
from Crypto.Util.Padding import pad, unpad
import Crypto.Util.Counter

BLOCK_SIZE = 16

# Cmds
cmds = {}
wallet_cmds = {}
pwd_cmds = {}

# Auth stuff
auth = {}
auth["master"] = ""
auth["session"] = ""

class MessageType(IntEnum):
    AUTH = 0

def good(message):
    print(f"[\u001b[32m+\u001b[0m] {message}")

def bad(message):
    print(f"[\u001b[31m-\u001b[0m] {message}")

def info(message):
    print(f"[\u001b[34m*\u001b[0m] {message}")

def connect():
    try:
        client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        client.settimeout(10)
    except socket.error:
        bad('Failed to create socket')
        sys.exit()

    ip = "192.168.0.58"
    port = 2222
    # Connect the socket object to the robot using IP address (string) and port (int)
    try:
        client.connect((ip,port))
    except Exception as e:
        return -1

    return client

# Gets nonce from the client, decrypts with session key, and encrypts with 
# master to send to the client with the request
def verifyCreds(client):
    # Request a nonce from the client
    cmd = {}
    cmd["req"] = "auth"

    client.sendall(bytes(json.dumps(cmd), 'utf-8'))

    try:
        info(f"Sending auth request")

        # Get all sent data until none left
        response = b''
        while True:
            data = client.recv(2048)

            if not data:
                break

            response += data

        print(response)

        json_response = json.loads(response)
    except Exception as e:
        bad(str(e))

    good(f"Received challenge")

    key_bytes = get_master_key_bytes()
    session_key_bytes = get_session_key_bytes()

    # Get the original nonce by decrypting received data with session key
    original_nonce = norm_decrypt_aes_ecb(json_response["nonce"].lower(), session_key_bytes)

    # Get the response nonce that is the original nonce encrypted with the master key
    encrypted_master_nonce = norm_encrypt_aes_ecb(original_nonce, key_bytes)

    return [original_nonce, encrypted_master_nonce]

def sendCommand(json_message):
    client = connect()

    nonces = verifyCreds(client)

    # Add nonce encrypted with master key to response
    json_message["token"] = nonces[1].hex().upper()

    info(f"Sending command + token")

    client = connect()
    client.sendall(bytes(json.dumps(json_message), 'utf-8'))

    # Get all sent data until none left
    response = b''
    while True:
        data = client.recv(2048)

        if not data:
            break

        response += data

    good(f"Received response")

    # print(response)

    client.close()

    info("Closing connection")

    return [nonces[0], response] # Return the original nonce, and the response

def handleCommand(command):
    try:
        spaces = command.count(' ')
        if spaces == 0:
            if command in ["wlt", "pwd", "set"]:
                bad("Invalid use of command")
                return
            if command in cmds.keys():
                cmds[command]()
                return

        # Make sure the command is in the command dict
        if command.split(' ')[0] not in cmds.keys():
            bad("Invalid command")
            return
        
        cmds[command.split(' ')[0]](command.split(command.split(' ')[0] + ' ')[1])
    except Exception as E:
        bad(f"Error: {E}")

def handlePwdCommand(command):
    # Make sure the command is in the command dict
    if command not in pwd_cmds.keys():
        bad("Invalid command")
        return

    pwd_cmds[command]()

def get_master_key_bytes():
    key = bytearray(0x10)
    index = 0
    key_bytes = auth["master"].encode()
    for i in range(16):
        if index == len(auth["master"]):
            index = 0
        
        key[i] = key_bytes[index]
        index+=1
    return key

def get_session_key_bytes():
    return auth["session"].encode()

def norm_decrypt_aes_ecb(ciphertext, key):
    if (isinstance(ciphertext, str)):
        ciphertext = binascii.unhexlify(ciphertext)
    cipher = AES.new(key, AES.MODE_ECB)
    plaintext = cipher.decrypt(ciphertext)
    return plaintext

def norm_encrypt_aes_ecb(plaintext, key):
    cipher = AES.new(key, AES.MODE_ECB)
    return cipher.encrypt(plaintext)

def decrypt_aes_ecb(ciphertext, key):
    if (isinstance(ciphertext, str)):
        ciphertext = binascii.unhexlify(ciphertext)
    cipher = AES.new(key, AES.MODE_ECB)
    padded_plaintext = cipher.decrypt(ciphertext)

    plaintext = bytearray(32)

    for i in range(0, 12):
        plaintext[i] = padded_plaintext[i]

    for i in range(16, 28):
        plaintext[i-4] = padded_plaintext[i]

    plaintext[25] = 0

    return plaintext

def encrypt_aes_ecb(plaintext, key):
    cipher = AES.new(key, AES.MODE_ECB)
    
    padded_plaintext = bytearray(32)

    for i in range(0, 12):
        padded_plaintext[i] = plaintext[i]
    
    for i in range(12, 16):
        padded_plaintext[i] = randint(0, 255)

    for i in range(16, 28):
        padded_plaintext[i] = plaintext[i-4]

    for i in range(28, 32):
        padded_plaintext[i] = randint(0, 255)
    
    return cipher.encrypt(padded_plaintext)

def getInput(prompt):
    return input(f"[\033[36;1;3m{'?'}\033[0m] \033[1;1;3m{prompt}\033[0m\n    ")

def readPassword():
    # Get name from the user
    name = rand_pad(getInput("Enter entry name:").encode('utf-8'), 32)
    print()

    # Construct request command
    master_key_bytes = get_master_key_bytes()
    cmd = {}
    cmd["type"] = 0
    cmd["name"] = base64.b64encode(encrypt_aes_ecb(name, master_key_bytes)).decode('ascii')

    # Get the cmd response, and nonce for decryption
    response_details = sendCommand(cmd)
    nonce_iv = response_details[0]
    raw_response = response_details[1]

    if raw_response == -1:
        bad("Connection to vault failed")

    # Check for empty response
    if raw_response == b'':
        bad("Password read failed")
        return

    # Set up the aes ctr instance with the key and nonce
    session_key_bytes = get_session_key_bytes()
    session_ctr_cipher = AES.new(session_key_bytes, AES.MODE_CTR, nonce=nonce_iv[0:8], initial_value=nonce_iv[8:16])
    
    # Decrypt response and load json
    plaintext = session_ctr_cipher.decrypt(base64.b64decode(raw_response)).decode('ascii')
    response = json.loads(plaintext.rstrip('\0'))

    # Perform decoding and decryption of password and username
    username = decrypt_aes_ecb(base64.b64decode(response["username"].encode('ascii')), master_key_bytes)
    password = decrypt_aes_ecb(base64.b64decode(response["password"].encode('ascii')), master_key_bytes)

    # Print response to terminal
    good("Decrypted response\n")

    print(f"[\033[93;1;3m{'$'}\033[0m] \033[;1;3m{'Username/Email'}\033[0m:\t {username[:username.index(0)].decode()}")
    print(f"[\033[93;1;3m{'$'}\033[0m] \033[;1;3m{'Password'}\033[0m:\t\t {password[:password.index(0)].decode()}")

def addPassword():
    key_bytes = get_master_key_bytes()

    name = rand_pad(getInput("Enter entry name:").encode('utf-8'), 32)
    user = rand_pad(getInput("Enter username/email:").encode('utf-8'), 32)
    pwd = rand_pad(getInput("Enter password:").encode('utf-8'), 32)
    print()

    cmd = {}
    cmd["type"] = 1
    cmd["name"] = base64.b64encode(encrypt_aes_ecb(name, key_bytes)).decode('ascii')
    cmd["user"] = base64.b64encode(encrypt_aes_ecb(user, key_bytes)).decode('ascii')
    cmd["pwd"] = base64.b64encode(encrypt_aes_ecb(pwd, key_bytes)).decode('ascii')

    raw_response = sendCommand(cmd)[1]

    if raw_response == -1:
        bad("Connection to vault failed")

    # Check for empty response
    if raw_response == b'':
        bad("Empty response")
        return

    response = json.loads(raw_response)

    if response["response"] == 0:
        good("Password added successfully")
    else:
        bad("Password add failed")

def editPassword():
    key_bytes = get_master_key_bytes()

    old_name = getInput("Enter old entry name:")
    new_name = getInput("Enter new entry name:")
    new_user = getInput("Enter new username/email:")
    new_pwd = getInput("Enter new password:")

    old_name_bytes = rand_pad(old_name.encode('utf-8'), 32)
    new_name_bytes = rand_pad(new_name.encode('utf-8'), 32)
    new_user_bytes = rand_pad(new_user.encode('utf-8'), 32)
    new_pwd_bytes = rand_pad(new_pwd.encode('utf-8'), 32)

    cmd = {}
    cmd["type"] = 2
    cmd["name"] = base64.b64encode(encrypt_aes_ecb(old_name_bytes, key_bytes)).decode('ascii')
    if not new_name == '':
        cmd["new_name"] = base64.b64encode(encrypt_aes_ecb(new_name_bytes, key_bytes)).decode('ascii')
    
    if not new_user == '':
        cmd["new_user"] = base64.b64encode(encrypt_aes_ecb(new_user_bytes, key_bytes)).decode('ascii')

    if not new_pwd == '':
        cmd["new_pwd"] = base64.b64encode(encrypt_aes_ecb(new_pwd_bytes, key_bytes)).decode('ascii')

    raw_response = sendCommand(cmd)[1]

    if raw_response == -1:
        bad("Connection to vault failed")

    # Check for empty response
    if raw_response == b'':
        bad("Empty response")
        return

    response = json.loads(raw_response)

    if response["response"] == 0:
        good("Password edited successfully")
    else:
        bad("Password edit failed")

def deletePassword():
    key_bytes = get_master_key_bytes()

    name = rand_pad(getInput("Enter entry name:").encode('utf-8'), 32)
    print()

    cmd = {}
    cmd["type"] = 3
    cmd["name"] = base64.b64encode(encrypt_aes_ecb(name, key_bytes)).decode('ascii')

    raw_response = sendCommand(cmd)[1]

    if raw_response == -1:
        bad("Connection to vault failed")

    # Check for empty response
    if raw_response == b'':
        bad("Empty response")
        return

    response = json.loads(raw_response)

    if response["response"] == 0:
        good("Password removed successfully")
    else:
        bad("Password remove failed")

def handleWalletCommand(command):
    if command not in wallet_cmds.keys():
        bad("Invalid command")
        return

    wallet_cmds[command]()

def readWallet():
    key_bytes = get_master_key_bytes()

    entry_name = rand_pad(getInput("Enter entry name:").encode('utf-8'), 32)
    print()

    cmd = {}
    cmd["type"] = 4
    cmd["name"] = base64.b64encode(encrypt_aes_ecb(entry_name, key_bytes)).decode('ascii')

    response_details = sendCommand(cmd)
    nonce_iv = response_details[0]
    raw_response = response_details[1]

    if raw_response == -1:
        bad("Connection to vault failed")

    # Check for empty response
    if raw_response == b'':
        bad("Empty response")
        return

    # Sends '0' if password not available
    if raw_response == b'0':
        bad("Wallet does not exist on the device")

    # Set up the aes ctr instance with the key and nonce
    session_key_bytes = get_session_key_bytes()
    session_ctr_cipher = AES.new(session_key_bytes, AES.MODE_CTR, nonce=nonce_iv[0:8], initial_value=nonce_iv[8:16])
    
    # Decrypt response and load json
    plaintext = session_ctr_cipher.decrypt(base64.b64decode(raw_response)).decode('ascii')
    response = json.loads(plaintext.rstrip('\0'))

    key_bytes = get_master_key_bytes()

    phrases = []

    for phrase in response:
        phrases.append(decrypt_aes_ecb(base64.b64decode(phrase.encode('ascii')), key_bytes))

    good("Decrypted response\n")

    ind = 0
    for phrase in phrases:
        print(f"[\033[93;1;3m{'$'}\033[0m] \033[;1;3mPhrase {ind}\033[0m:\t {phrase[:phrase.index(0)].decode()}")
        ind+=1

def addWallet():
    key_bytes = get_master_key_bytes()

    wlt_name = rand_pad(getInput("Enter wallet name:").encode('utf-8'), 32)
    entry_count = getInput("Enter the number of phrases")

    if int(entry_count) <= 0 or int(entry_count) > 24:
        bad("Invalid entry count")
        return

    key_bytes = get_master_key_bytes()

    phrases = []
    for i in range(0, int(entry_count)):
        str_input = getInput(f"Enter wallet phrase {i}:")
        byte_input = str_input.encode('utf-8')
        padded_input = rand_pad(byte_input, 32)

        # Encrypt and append
        phrases.append(base64.b64encode(encrypt_aes_ecb(padded_input, key_bytes)).decode('ascii'));
    
    cmd = {}
    cmd["type"] = 5
    cmd["name"] = base64.b64encode(encrypt_aes_ecb(wlt_name, key_bytes)).decode('ascii')
    cmd["phrases"] = json.dumps(phrases)

    raw_response = sendCommand(cmd)[1]

    if raw_response == -1:
        bad("Connection to vault failed")

    # Check for empty response
    if raw_response == b'':
        bad("Empty response")
        return

    # Sends '0' if password not available
    if raw_response == b'0':
        bad("Wallet does not exist on the device")

    # Parse the response
    response = json.loads(raw_response)

    if response["response"] == 0:
        good("Wallet added successfully")
    else:
        bad("Wallet add failed")

def delWallet():
    key_bytes = get_master_key_bytes()

    entry_name = rand_pad(getInput("Enter entry name:").encode('utf-8'), 32)
    print()

    cmd = {}
    cmd["type"] = 6
    cmd["name"] = base64.b64encode(encrypt_aes_ecb(entry_name, key_bytes)).decode('ascii')

    raw_response = sendCommand(cmd)[1]

    if raw_response == -1:
        bad("Connection to vault failed")

    # Check for empty response
    if raw_response == b'':
        bad("Empty response")
        return

    # Sends '0' if password not available
    if raw_response == b'0':
        bad("Wallet does not exist on the device")

    # Parse the response
    response = json.loads(raw_response)

    if response["response"] == 0:
        good("Wallet deleted successfully")
    else:
        bad("Wallet delete failed")

def rand_pad(byte_input, target_size):
    padded_input = bytearray(target_size)

    # Copy phrase
    for i in range(len(byte_input)):
        padded_input[i] = byte_input[i]

    # Add null terminator
    padded_input[len(byte_input)] = 0;

    # Random padding
    for i in range(len(byte_input) + 1, 32):
        padded_input[i] = randint(0, 255)

    return padded_input

def initCmdLine():
    cmds["exit"] = endSession
    cmds["quit"] = endSession
    cmds["pwd"] = handlePwdCommand
    cmds["wlt"] = handleWalletCommand

    cmds["set"] = handleSet
    cmds["status"] = printStatus
    cmds["help"] = printHelp
    cmds["info"] = printStatus

    pwd_cmds["read"] = readPassword
    pwd_cmds["add"] = addPassword
    pwd_cmds["edit"] = editPassword
    pwd_cmds["del"] = deletePassword

    wallet_cmds["add"] = addWallet
    wallet_cmds["read"] = readWallet
    wallet_cmds["del"] = delWallet

def handleSet(input):
    if input.count(' ') == 0:
        bad("Invalid command")
        return

    if input.split(' ')[0] not in auth.keys():
        bad("Invalid command")
        return

    good(f"Setting '{input.split(' ')[0]}' to '{input.split(' ')[1]}'")
    
    auth[input.split(' ')[0]] = input.split(' ')[1]

def endSession():
    exit(0)

def printHelp():
    print("help")

def printStatus():
    print(auth)

def addGoldItalicAnsii(toPrint):
    return f"\033[33;1;3m{toPrint}\033[0m"

def printHelp():
    info("Available commands:")
    print(f"\
    \033[33;1;3m{'pwd read'}\033[0m \t Reads stored password with name\n\
    \033[33;1;3m{'pwd add'}\033[0m \t Adds password entry to device\n\
    \033[33;1;3m{'pwd del'}\033[0m \t Delete password entry from device\n\
    \033[33;1;3m{'pwd edit'}\033[0m \t Edit password entry")

    print(f"\n\
    \033[33;1;3m{'wlt read'}\033[0m \t Reads stored wallet with name\n\
    \033[33;1;3m{'wlt add'}\033[0m \t Adds wallet entry to device\n\
    \033[33;1;3m{'wlt del'}\033[0m \t Deletes the stored wallet with name")

def main():
    f = Figlet(font='slant')
    print(colored(f.renderText('VAULT'), 'yellow'), end='')

    printHelp()

    parser = argparse.ArgumentParser(
                    prog = 'Vault Client',
                    description = 'Interact with the ESP8266 Vault device',
                    epilog = 'Text at the bottom of help')

    parser.add_argument("-m", "--master", type=str, help='Master password for vault')
    parser.add_argument("-s", "--session", type=str, help='Session key on vault screen')

    args = parser.parse_args()

    # Set auth if provided
    if args.master != None:
        auth["master"] = args.master
    if args.session != None:
        auth["session"] = args.session

    initCmdLine()

    while True:
        print()
        text = getInput("Enter command:")
        print()
        handleCommand(text)
        print()

if __name__ == "__main__":
    main()