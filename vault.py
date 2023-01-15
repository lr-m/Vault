import socket
import time
from termcolor import colored
from pyfiglet import Figlet
import argparse
import hashlib
from Crypto.Cipher import AES
from enum import IntEnum
import json
import binascii
from Crypto.Util.Padding import pad, unpad

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

def constructBaseJson(master, session):
    command = {}
    command["master"] = hashlib.sha512(bytes(master, 'utf-8')).hexdigest().upper()
    command["type"] = 2

    return command

def sendCommand(json_message):
    print(json_message)
    try:
        client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        client.settimeout(5)
    except socket.error:
        bad('Failed to create socket')
        sys.exit()

    good('Socket Created')

    ip = "192.168.0.58"
    port = 2222
    # Connect the socket object to the robot using IP address (string) and port (int)
    try:
        client.connect((ip,port))
    except Exception as e:
        return -1

    good(f"Connected to {ip} on port {port}")

    client.sendall(bytes(json.dumps(json_message), 'utf-8'))

    info("Sent command to device")

    # Get all sent data until none left
    response = b''
    while True:
        data = client.recv(2048)

        if not data:
            break

        response += data

    good("Received response")

    client.close()

    info("Closing connection")

    return response

def handleCommand(command):
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

def handlePwdCommand(command):
    # Make sure the command is in the command dict
    if command not in pwd_cmds.keys():
        bad("Invalid command")
        return

    pwd_cmds[command]()

def readPassword():
    # print(encrypt_aes_ecb(b"hello world", get_master_key_bytes()))
    # print(decrypt_aes_ecb(encrypt_aes_ecb(b"hello world", get_master_key_bytes()), get_master_key_bytes()))
    entry_name = getInput("Enter name of entry:")
    print()

    cmd = constructBaseJson(auth["master"], auth["session"])

    cmd["type"] = 0
    cmd["name"] = entry_name

    raw_response = sendCommand(cmd)

    if raw_response == -1:
        bad("Connection to vault failed")

    # Check for empty response
    if raw_response == b'':
        bad("Empty response")
        return

    # Sends '0' if password not available
    if raw_response == b'0':
        bad("Password does not exist on the device")

    # Parse the response
    response = json.loads(raw_response)

    key_bytes = get_master_key_bytes()

    username = decrypt_aes_ecb(response["username"].lower(), key_bytes)
    password = decrypt_aes_ecb(response["password"].lower(), key_bytes)

    good("Decrypted response\n")

    print(f"[\033[93;1;3m{'$'}\033[0m] \033[;1;3m{'Username/Email'}\033[0m:\t {username[:username.index(0)].decode()}")
    print(f"[\033[93;1;3m{'$'}\033[0m] \033[;1;3m{'Password'}\033[0m:\t\t {password[:password.index(0)].decode()}")

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

def decrypt_aes_ecb(ciphertext, key):
    if (isinstance(ciphertext, str)):
        ciphertext = binascii.unhexlify(ciphertext)
    cipher = AES.new(key, AES.MODE_ECB)
    plaintext = cipher.decrypt(ciphertext)
    return plaintext

def encrypt_aes_ecb(plaintext, key):
    cipher = AES.new(key, AES.MODE_ECB)
    return cipher.encrypt(pad(plaintext, BLOCK_SIZE))

def getInput(prompt):
    return input(f"[\033[36;1;3m{'?'}\033[0m] \033[1;1;3m{prompt}\033[0m\n    ")

def addPassword():
    name = getInput("Enter entry name:")
    user = getInput("Enter username/email:")
    pwd = getInput("Enter password:")
    print()

    cmd = constructBaseJson(auth["master"], auth["session"])

    cmd["type"] = 1
    cmd["name"] = name
    cmd["user"] = user
    cmd["pwd"] = pwd

    raw_response = sendCommand(cmd)

    if raw_response == -1:
        bad("Connection to vault failed")

    # Check for empty response
    if raw_response == b'':
        bad("Empty response")
        return

    response = json.loads(raw_response)

    if response["response"] == 0:
        good("Password added successfully")

def editPassword():
    old_name = getInput("Enter old entry name:")
    new_name = getInput("Enter new entry name:")
    new_user = getInput("Enter new username/email:")
    new_pwd = getInput("Enter new password:")

    print(old_name == '')

    print()

    cmd = constructBaseJson(auth["master"], auth["session"])

    cmd["type"] = 2
    cmd["name"] = old_name

    if not new_name == '':
        cmd["new_name"] = new_name
    
    if not new_user == '':
        cmd["new_user"] = new_user

    if not new_pwd == '':
        cmd["new_pwd"] = new_pwd

    raw_response = sendCommand(cmd)

    if raw_response == -1:
        bad("Connection to vault failed")

    # Check for empty response
    if raw_response == b'':
        bad("Empty response")
        return

    response = json.loads(raw_response)

    if response["response"] == 0:
        good("Password edited successfully")

def deletePassword():
    name = getInput("Enter entry name:")

    print()

    cmd = constructBaseJson(auth["master"], auth["session"])

    cmd["type"] = 3
    cmd["name"] = name

    raw_response = sendCommand(cmd)

    if raw_response == -1:
        bad("Connection to vault failed")

    # Check for empty response
    if raw_response == b'':
        bad("Empty response")
        return

    response = json.loads(raw_response)

    if response["response"] == 0:
        good("Password removed successfully")

def handleWalletCommand(command):
    if command not in wallet_cmds.keys():
        bad("Invalid command")
        return

    wallet_cmds[command]()

def readWallet():
    entry_name = getInput("Enter name of entry:")
    print()

    cmd = constructBaseJson(auth["master"], auth["session"])

    cmd["type"] = 4
    cmd["name"] = entry_name

    raw_response = sendCommand(cmd)

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

    key_bytes = get_master_key_bytes()

    phrases = []

    for phrase in response:
        phrases.append(decrypt_aes_ecb(phrase.lower(), key_bytes))

    good("Decrypted response\n")

    ind = 0
    for phrase in phrases:
        print(f"[\033[93;1;3m{'$'}\033[0m] \033[;1;3mPhrase {ind}\033[0m:\t {phrase[:phrase.index(0)].decode()}")
        ind+=1

    # 
    # print(f"[\033[93;1;3m{'$'}\033[0m] \033[;1;3m{'Password'}\033[0m:\t\t {password[:password.index(0)].decode()}")

def addWallet():
    print("Adding wallet")

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
    \033[33;1;3m{'wlt add'}\033[0m \t Adds wallet entry to device")

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