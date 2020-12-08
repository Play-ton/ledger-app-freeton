#!/usr/bin/env python3

from ledgercomm import Transport
import argparse
import os

print('~~ Free TON ~~')
print('Request address')

parser = argparse.ArgumentParser()
parser.add_argument('-d', '--debug', action='store_true', help='Enable debug logs')
parser.add_argument('-a', '--account', default=0, type=int, help='BIP32 account to retrieve')
parser.add_argument('-t', '--tvc', required=True, help='TVC file path')
parser.add_argument('-c', '--confirm', action='store_true', help='Enable confirmation on device')
args = parser.parse_args()

print('Contract: {}'.format(os.path.basename(args.tvc)))

INS_GET_ADDRESS = 0x04
P1_FIRST = 0x00
P1_MORE = 0x80
P1_LAST = 0x81
P2_CONFIRM = args.confirm
CLA = 0xe0
SUCCESS = 0x9000

with open(args.tvc, 'rb') as f:
    tvc = f.read()

transport = Transport(interface='hid', debug=args.debug)
payload = len(tvc).to_bytes(2, byteorder='big')
sw, response = transport.exchange(cla=CLA, ins=INS_GET_ADDRESS, p1=P1_FIRST, cdata=payload)
if sw != SUCCESS:
    print('Error: {:X}'.format(sw))
    exit(1)

n = 192 # 64 byte aligned data
parts = [tvc[i:i + n] for i in range(0, len(tvc), n)]
for i, part in enumerate([tvc[i:i + n] for i in range(0, len(tvc), n)]):
    sw, response = transport.exchange(cla=CLA, ins=INS_GET_ADDRESS, p1=P1_MORE, cdata=part)
    if sw != SUCCESS:
        print('Error: {:X}'.format(sw))
        exit(1)

payload = args.account.to_bytes(4, byteorder='big')
sw, response = transport.exchange(cla=CLA, ins=INS_GET_ADDRESS, p1=P1_LAST, p2=P2_CONFIRM, cdata=payload)
if sw != SUCCESS:
    print('Error: {:X}'.format(sw))
    exit(1)

print('Address: {}'.format(response[1:].hex()))
