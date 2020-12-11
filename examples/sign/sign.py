#!/usr/bin/env python3

from ledgercomm import Transport
import argparse
import os

print('~~ Free TON ~~')
print('Sign message')

parser = argparse.ArgumentParser()
parser.add_argument('-d', '--debug', action='store_true', help='Enable debug logs')
parser.add_argument('-a', '--account', default=0, type=int, help='BIP32 account to retrieve')
parser.add_argument('-m', '--message', required=True, help='Bytes to sign')
args = parser.parse_args()

if args.account < 0 or args.account > 4294967295:
    print('Error: account number must be between 0 and 4294967295')
    exit(1)

INS_SIGN = 0x03
CLA = 0xe0
SUCCESS = 0x9000

transport = Transport(interface='hid', debug=args.debug)
payload = args.account.to_bytes(4, byteorder='big') + b''.fromhex(args.message)
sw, response = transport.exchange(cla=CLA, ins=INS_SIGN, cdata=payload)
if sw != SUCCESS:
    print('Error: {:X}'.format(sw))
    exit(1)

print('Signed bytes: {}'.format(response[1:].hex()))
