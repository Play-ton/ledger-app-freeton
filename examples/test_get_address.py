#!/usr/bin/env python3

# compile batch of contracts
# for file in ./*.sol; do solc $file; tvm_linker compile ${file%.*}.code --abi-json ${file%.*}.abi.json -o ${file%.*}.tvc; done

import os
import argparse
import glob
from pathlib import Path
import re

def popen(cmdline):
  with os.popen(cmdline) as f:
    return f.read().rstrip()

parser = argparse.ArgumentParser()
parser.add_argument('-d', '--dir', required=True, help='Test directory with tvc files')
parser.add_argument('-k', '--keys', required=True, help='Path to keys file')
args = parser.parse_args()

tonos = dict()
ledger = dict()
print('TVC dir:', args.dir)
for file in glob.glob(args.dir + '*.tvc'):
    f = Path(file)
    res = popen('tonos-cli genaddr {} {}.abi.json --setkey {} | grep -P "Raw address|\.tvc"'.format(f, f.parent / f.stem, args.keys))
    address_tonos = re.search('Raw address: 0:([a-f0-9]*)$', res).group(1)
    tonos.update({f.name: address_tonos})

    res = popen('./get_address.py --tvc {}'.format(f))
    address_ledger = re.search('Address: ([a-f0-9]*)$', res).group(1)
    ledger.update({f.name: address_ledger})
    print(f.name, address_tonos, address_ledger, address_tonos == address_ledger)
    if address_tonos != address_ledger:
        raise Exception('Get address failed for {}'.format(f.name))

# print(tonos, ledger)
print(tonos == ledger)
