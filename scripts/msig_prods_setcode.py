#!/usr/bin/env python3

import argparse
import json
import os
import re
import subprocess
import sys
import time

enable_push = False
cleos = 'cleos --url http://w2.eosforce.cn '
wallet_password = ''
wallet_name = 'force.msig'
active_account = 'force.msig'
proposal_name = 'p.upsyscode'
wasm_path = './eosforce/build/contracts/System02/System02.wasm'
tx_expire_hours = 120


def jsonArg(a):
    return " '" + json.dumps(a) + "' "


def run(args):
    print('', args)
    if enable_push == False:
        return
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


def msigProposeReplaceSystem(proposer, pname, bps, wasmpath, expirehours):
    requestedPermissions = []
    for i in range(0, len(bps)):
        requestedPermissions.append({'actor': bps[i], 'permission': 'active'})
    trxPermissions = [{'actor': 'eosio', 'permission': 'active'}]
    with open(wasmpath, mode='rb') as f:
        setcode = {'account': 'eosio', 'vmtype': 0, 'vmversion': 0, 'code': f.read().hex()}
    run(cleos + 'multisig propose ' + pname + jsonArg(requestedPermissions) + jsonArg(
        trxPermissions) + 'eosio setcode' + jsonArg(
        setcode) + ' ' + proposer + ' ' + expirehours + ' -p ' + proposer)


# ---------------------------------------------------------------------------------------------------
unlockwallet_str = 'cleos wallet unlock -n ' + wallet_name + '--password ' + wallet_password
runone(unlockwallet_str)

active_bps = getbps()
msigProposeReplaceSystem(active_account, proposal_name, active_bps, wasm_path, tx_expire_hours)
