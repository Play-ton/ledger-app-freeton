#!/usr/bin/env python3

import asyncio
import os
import json
import argparse
import logging
import base64
import time
from ledgercomm import Transport
from tonclient.client import TonClient, DEVNET_BASE_URL
from tonclient.types import Abi, DeploySet, CallSet, Signer, AccountForExecutor

WALLET_DIR = 'wallet'
WALLET_NAME = 'SetcodeMultisigWallet'

INS_GET_APP_CONFIGURATION = 0x01
INS_GET_PUBLIC_KEY = 0x02
INS_SIGN = 0x03
INS_GET_ADDRESS = 0x04
CLA = 0xe0
P1_FIRST = 0x00
P1_MORE = 0x80
P1_LAST = 0x81
SUCCESS = 0x9000


def get_tvc():
    with open(os.path.join(WALLET_DIR, WALLET_NAME + '.tvc'), 'rb') as f:
        return f.read()


class Ledger:
    def __init__(self, account: int, workchain: int, debug=False):
        self.account = account
        self.transport = Transport(interface='hid', debug=debug)
        self.public_key = ''
        self.address = ''
        self.workchain = workchain

    def get_public_key(self, confirm = False) -> str:
        if self.public_key:
            return self.public_key
        payload=(self.account).to_bytes(4, byteorder='big')
        sw, response = self.transport.exchange(cla=CLA, ins=INS_GET_PUBLIC_KEY, p1=confirm, cdata=payload)
        if sw != SUCCESS:
            raise Exception('get_public_key error: {:X}'.format(sw))

        self.public_key = response[1:].hex()
        return self.public_key

    def get_address(self, confirm = False) -> str:
        if self.address:
            return self.address

        tvc = get_tvc()
        payload = len(tvc).to_bytes(2, byteorder='big')
        sw, response = self.transport.exchange(cla=CLA, ins=INS_GET_ADDRESS, p1=P1_FIRST, cdata=payload)
        if sw != SUCCESS:
            raise Exception('get_address error: {:X}'.format(sw))

        n = 192 # 64 byte aligned data
        parts = [tvc[i:i + n] for i in range(0, len(tvc), n)]
        for i, part in enumerate([tvc[i:i + n] for i in range(0, len(tvc), n)]):
            sw, response = self.transport.exchange(cla=CLA, ins=INS_GET_ADDRESS, p1=P1_MORE, cdata=part)
        if sw != SUCCESS:
            raise Exception('get_address error: {:X}'.format(sw))

        payload = self.account.to_bytes(4, byteorder='big')
        sw, response = self.transport.exchange(cla=CLA, ins=INS_GET_ADDRESS, p1=P1_LAST, p2=confirm, cdata=payload)
        if sw != SUCCESS:
            raise Exception('get_address error: {:X}'.format(sw))

        self.address = '{}:{}'.format(self.workchain, response[1:].hex())
        return self.address

    def sign(self, to_sign: bytes) -> str:
        payload = self.account.to_bytes(4, byteorder='big') + to_sign 
        sw, response = self.transport.exchange(cla=CLA, ins=INS_SIGN, cdata=payload)
        if sw != SUCCESS:
            raise Exception('sign error: {:X}'.format(sw))

        return response[1:].hex()


class Wallet:
    def __init__(self, client: TonClient, ledger: Ledger):
        self.client = client
        self.ledger = ledger

    def call(self, address, function_name, inputs, deploy_set=None):
        public_key = self.ledger.get_public_key()
        call_set = CallSet(function_name=function_name, header={'pubkey': public_key, 'expire': int(time.time()) + 60}, inputs=inputs)
        abi = Abi.from_path(os.path.join(WALLET_DIR, WALLET_NAME + '.abi.json'))
        signer = Signer(public=public_key)

        unsigned_msg = self.client.abi.encode_message(abi=abi, signer=signer, address=address, deploy_set=deploy_set, call_set=call_set)
        to_sign = base64.b64decode(unsigned_msg['data_to_sign'])
        print('Please confirm signature on device:', to_sign.hex())
        signature = self.ledger.sign(to_sign)
        msg = self.client.abi.attach_signature(abi=abi, public_key=public_key, message=unsigned_msg['message'], signature=signature)
        
        shard_block_id = self.client.processing.send_message(message=msg['message'], send_events=False, abi=abi)
        result = self.client.processing.wait_for_transaction(message=msg['message'], shard_block_id=shard_block_id, send_events=False, abi=abi)
        if result['transaction']['aborted'] == False:
            print('Transaction succeeded, id:', result['transaction']['id'])
        else:
            print('Transaction aborted')

    def send(self, src: str, dest: str, value: int):
        if not src:
            src = self.ledger.get_address()
        print('Send {} tokens from {} to {}'.format(value / 1000000000, src, dest))
        self.call(src, 'submitTransaction', {'dest': dest, 'value': value, 'bounce': False, 'allBalance': False, 'payload': ''})

    def confirm(self, address: str, transaction_id: str):
        print('Confirm transaction {} at {}'.format(transaction_id, address))
        self.call(address, 'confirmTransaction', {'transactionId': transaction_id})

    def deploy(self):
        print('Deploying {} to {}'.format(WALLET_NAME, self.ledger.get_address()))
        deploy_set = DeploySet(tvc=base64.b64encode(get_tvc()).decode())
        self.call(None, 'constructor', {'owners':['0x' + self.ledger.get_public_key()], 'reqConfirms':1}, deploy_set)

    def run_tvm(self, address, function_name, inputs):
        keypair = self.client.crypto.generate_random_sign_keys()
        call_set = CallSet(function_name=function_name, inputs=inputs)
        abi = Abi.from_path(os.path.join(WALLET_DIR, WALLET_NAME + '.abi.json'))
        signer = Signer.from_keypair(keypair=keypair)

        msg = self.client.abi.encode_message(abi=abi, signer=signer, address=address, call_set=call_set)
        account = self.client.net.wait_for_collection(collection='accounts', filter={'id': {'eq': address}}, result='id boc')
        result = self.client.tvm.run_tvm(message=msg['message'], account=account['boc'], abi=abi)

        json_formatted_str = json.dumps(result['decoded']['output'], indent=2)
        print(json_formatted_str)
        
    def get_transactions(self, address: str):
        print('Get transactions at {}'.format(address))
        self.run_tvm(address, 'getTransactions', {})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-a', '--account', default=0, type=int, help='BIP32 account to retrieve (default 0)')
    parser.add_argument('-d', '--debug', action='store_true', help='Enable debug logs')
    parser.add_argument('-u', '--url', default=DEVNET_BASE_URL, help='Server address')
    parser.add_argument('--wc', default=0, type=int, help='Workchain id (default 0)')
    parser.add_argument('--getaddr', action='store_true', help='Get address given account')
    parser.add_argument('--getpubkey', action='store_true', help='Get public key given account')
    parser.add_argument('--confirm', action='store_true', help='Confirm action on device')

    subparsers = parser.add_subparsers(title='subcommands')
    send_parser = subparsers.add_parser('send', help='Send tokens to address')
    send_parser.set_defaults(subcommand='send')
    send_parser.add_argument('--dest', required=True, help='Destination address')
    send_parser.add_argument('--value', required=True, default=0, type=int, help='Value to send (in nanotokens)')
    send_parser.add_argument('--msig', default='', help='Source address (for multisig)')

    confirm_parser = subparsers.add_parser('confirm', help='Confirm multisig transaction')
    confirm_parser.set_defaults(subcommand='confirm')
    confirm_parser.add_argument('--msig', required=True, help='Multisig address')
    confirm_parser.add_argument('--id', required=True, help='Transaction id to confirm')

    deploy_parser = subparsers.add_parser('deploy', help='Deploy multisig wallet')
    deploy_parser.set_defaults(subcommand='deploy')

    gettxs_parser = subparsers.add_parser('gettxs', help='Get multisig wallet transactions')
    gettxs_parser.set_defaults(subcommand='gettxs')
    gettxs_parser.add_argument('--msig', required=True, help='Multisig address')

    args = parser.parse_args()

    if args.account < 0 or args.account > 4294967295:
        raise Exception('account number must be between 0 and 4294967295')
    
    ledger = Ledger(account=args.account, workchain=args.wc, debug=args.debug)
    if args.getaddr:
        print('Address:', ledger.get_address(args.confirm))
        return
    if args.getpubkey:
        print('Public key:', ledger.get_public_key(args.confirm))
        return
    if 'subcommand' in vars(args):
        server_address = args.url
        print('Server address:', server_address)
        client = TonClient(network={'server_address': server_address})
        wallet = Wallet(client, ledger)
        if args.subcommand == 'send':
            wallet.send(args.msig, args.dest, args.value)
            return
        if args.subcommand == 'confirm':
            wallet.confirm(args.msig, args.id)
            return
        if args.subcommand == 'deploy':
            wallet.deploy()
            return
        if args.subcommand == 'gettxs':
            wallet.get_transactions(args.msig)
            return


if __name__ == "__main__":
    try:
        main()
    except Exception:
        logging.error('Fatal error', exc_info=True)
