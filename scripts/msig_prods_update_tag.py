#!/usr/bin/env python3

import argparse
import json
import os
import re
import subprocess
import sys
import time

enable_push = True # True to push on chain
cleos = '../build/programs/cleos/cleos --wallet-url http://127.0.0.1:6666 --url http://127.0.0.1:8001 '
wallet_password = ''
wallet_name = 'testc'
active_account = 'testc'

funcs_to_open = [
    ( 'f.cprod',   10000000 ),
    ( 'f.votagen', 10000010 )
]

tx_expire_hours = 120   # 5days

def jsonArg(a):
    return " '" + json.dumps(a) + "' "


def run(args):
    print('', args)
    if subprocess.call(args, shell=True):
        print(' exiting because of error')
        sys.exit(1)


def runone(args):
    print('', args)
    subprocess.call(args, shell=True)


def getOutput(args):
    print('', args)
    proc = subprocess.Popen(args, shell=True, stdout=subprocess.PIPE)
    return proc.communicate()[0].decode('utf-8')


def getJsonOutput(args):
    return json.loads(getOutput(args))


def getbps():
    bpsa = []
    bpsj = getJsonOutput(cleos + " get schedule -j ")
    for bp in bpsj["active"]["producers"]:
        bpsa.append(bp["producer_name"])
    return bpsa

def msigProposeUpdateTag(proposer, bps, func_name, open_block_num, expirehours):
    requestedPermissions = []
    for i in range(0, len(bps)):
        requestedPermissions.append({'actor': bps[i], 'permission': 'active'})
    trxPermissions = [{'actor': 'eosio', 'permission': 'active'}]

    action_name = 'setconfig'
    data = {
        'typ': func_name, 
        'num': open_block_num,
        'key': '',
        'fee': '0.0000 EOS'
    }

    run(cleos + 'multisig propose ' 
              + func_name + jsonArg(requestedPermissions) + jsonArg(trxPermissions) 
              + 'eosio ' + action_name + jsonArg(data) + ' ' 
              + proposer + ' ' + str(expirehours) + ' -p ' + proposer)

# ---------------------------------------------------------------------------------------------------
# msig to update system contract

# unlock wallet
unlockwallet_str = 'cleos wallet unlock -n ' + wallet_name + ' --password ' + wallet_password
runone(unlockwallet_str)

# get schedule active bps
active_bps = getbps()

for ( func_name, func_block_num ) in funcs_to_open:
    msigProposeUpdateTag(active_account, active_bps, func_name, func_block_num, tx_expire_hours)
    time.sleep(3)
