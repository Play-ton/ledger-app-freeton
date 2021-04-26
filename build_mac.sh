#!/bin/bash
pyinstaller --clean -y -F freetoncli.py --collect-all tonclient -i crystal.ico --add-data="wallet/Wallet.tvc:wallet" --add-data="wallet/Wallet.abi.json:wallet"
