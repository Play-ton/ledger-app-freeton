#!/usr/bin/env python3

from ledgercomm import Transport
import argparse

print('~~ Free TON ~~')
print('Request public key')

parser = argparse.ArgumentParser()
parser.add_argument('-d', '--debug', action='store_true', help='Enable debug logs')
parser.add_argument('-a', '--account', default=0, type=int, help='BIP32 account to retrieve')
parser.add_argument('-c', '--confirm', action='store_true', help='Enable confirmation on device')
args = parser.parse_args()

INS_GET_PUBLIC_KEY = 0x02
P1_CONFIRM = args.confirm
CLA = 0xe0
SUCCESS = 0x9000

transport = Transport(interface='hid', debug=args.debug)
payload=(args.account).to_bytes(4, byteorder='big')
sw, response = transport.exchange(cla=CLA, ins=INS_GET_PUBLIC_KEY, p1=P1_CONFIRM, cdata=payload)
if sw == SUCCESS:
    print('Public key: {}'.format(response[1:].hex()))
else:
    print('Error: {:X}'.format(sw))
    exit(1)
