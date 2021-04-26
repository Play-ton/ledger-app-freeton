#!/bin/bash
find . -path '*tonclient/bin*' -name "*.dll" -exec rm -rf {} \; -o -path '*tonclient/bin*' -name "*.dylib" -exec rm -rf {} \;
pyinstaller --clean -y -F freetoncli.py --collect-all tonclient -i crystal.ico --add-data="wallet/Wallet.tvc:wallet" --add-data="wallet/Wallet.abi.json:wallet"
