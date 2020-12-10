# Ledger Free TON app

## Overview
Free TON Ledger app for Ledger Nano S/X

## Building and installing
To build and install the app on your Ledger Nano S you must set up the Ledger Nano S build environments. Please follow the Getting Started instructions at [here](https://ledger.readthedocs.io/en/latest/userspace/getting_started.html).

If you don't want to setup a global environnment, you can also setup one just for this app by sourcing `prepare-devenv.sh` with the right target (`s` or `x`).

install prerequisite and switch to a Nano X dev-env:

```bash
sudo apt install gcc make gcc-multilib g++-multilib libncurses5
sudo apt install python3-venv python3-dev libudev-dev libusb-1.0-0-dev

# (s or x, depending on your device)
source prepare-devenv.sh s 
```

To fix problems connecting to Ledger follow this [guide](https://support.ledger.com/hc/en-us/articles/115005165269-Fix-connection-issues)

Compile and load the app onto the device:
```bash
make load
```

Refresh the repo (required after Makefile edits):
```bash
make clean
```

Remove the app from the device:
```bash
make delete
```

Enable debug mode:
```bash
make clean DEBUG=1 load
```

## Example of Ledger wallet functionality

**Request public key:**
```bash
source prepare-devenv.sh s
cd examples
python get_public_key.py --account 0
```

**Request address:**
```bash
source prepare-devenv.sh s
cd examples
# download the contract .tvc file
wget https://raw.githubusercontent.com/tonlabs/ton-labs-contracts/master/solidity/setcodemultisig/SetcodeMultisigWallet.tvc
python get_address.py --account 1 --tvc SetcodeMultisigWallet.tvc --confirm
# confirm or reject address on device
```

The following examples requires additional installation of [Node.js](https://github.com/nodesource/distributions/blob/master/README.md#installation-instructions) and module [ton-client-node-js](https://www.npmjs.com/package/ton-client-node-js)

**Deploy SetcodeMultisigWallet contract:**

Replace values in `examples/sign/deploy_config.json` with your own
```bash
source prepare-devenv.sh s
cd examples
python get_address.py --account 1 --tvc SetcodeMultisigWallet.tvc # get future address of the contract
# send some tokens to the received address (about 0.5 should enough)
node sign/prepare_deploy.js # get %bytes_to_sign
python sign/sign.py --account 1 --message %bytes_to_sign% # sign bytes on device
# confirm sign on device
node sign/run_deploy.js %signed_bytes%
```
Now you can send some tokens from the newly created address

**Sign SetcodeMultisigWallet sendTransaction message:**

Replace values in `examples/sign/send_config.json` with your own
```bash
source prepare-devenv.sh s
cd examples
node sign/prepare_send.js 
python sign/sign.py --account 1 --message %bytes_to_sign%
# confirm sign on device
node sign/run_send.js %signed_bytes%
```

## Documentation
This follows the specification available in the [`freetonapp.asc`](https://github.com/play-ton/ledger-app-freeton/blob/master/doc/freetonapp.asc)
