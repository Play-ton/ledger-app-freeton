#!/usr/bin/env python3

from ledgercomm import Transport
import argparse

print('~~ Free TON ~~')
print('Request app configuration')

parser = argparse.ArgumentParser()
parser.add_argument('-d', '--debug', action='store_true', help='Enable debug logs')
args = parser.parse_args()

INS_GET_APP_CONFIGURATION = 0x01
CLA = 0xe0
SUCCESS = 0x9000

transport = Transport(interface='hid', debug=args.debug)
sw, response = transport.exchange(cla=CLA, ins=INS_GET_APP_CONFIGURATION)

if sw == SUCCESS:
    print('App version: {}'.format('.'.join('{}'.format(b) for b in response)))
else:
    print('Error: {:X}'.format(sw))
    exit(1)
