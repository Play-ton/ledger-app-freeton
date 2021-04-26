Get-ChildItem -Path . -Recurse | Where-Object {$_.extension -in ".so",".dylib" -and $_.FullName -like "*tonclient\bin*"} | Remove-Item
pyinstaller --clean -y -F freetoncli.py --collect-binaries tonclient -i crystal.ico --add-data="wallet/Wallet.tvc;wallet" --add-data="wallet/Wallet.abi.json;wallet"
