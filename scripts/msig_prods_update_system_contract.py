#!/usr/bin/env python3

import argparse
import json
import os
import re
import subprocess
import sys
import time

enable_push = False # True to push on chain
cleos = '../build/programs/cleos/cleos --wallet-url http://127.0.0.1:6666 --url http://127.0.0.1:8001 '
wallet_password = ''
wallet_name = 'testc'
active_account = 'testc'

proposal_name_upsyscode = 'p.upsyscode'
proposal_name_upsysabi = 'p.upsysabi'

# eosio system contract code in wasm
wasm_path = '../build/contracts/System02/System02.wasm'
abi_path = '../build/contracts/System02/System02.abi'
# eosio system contract abi in hex, gen by ./build/contracts/System02/System02.abi

tx_expire_hours = 120   # 5days

def getAbiData(path):
    run("rm -rf ./system_contract_abi.data")
    run("./abi2hex -abi " + path + " > ./system_contract_abi.data")

    time.sleep(.5)

    f = open("./system_contract_abi.data")
    line = f.readline()
    run("rm -rf ./system_contract_abi.data")
    return line

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

# msig to update system contract code
def msigProposeUpdateSystem(proposer, pname, bps, wasmpath, expirehours):
    requestedPermissions = []
    for i in range(0, len(bps)):
        requestedPermissions.append({'actor': bps[i], 'permission': 'active'})
    trxPermissions = [{'actor': 'eosio', 'permission': 'active'}]

    with open(wasmpath, mode='rb') as f:
        setcode = {'account': 'eosio', 'vmtype': 0, 'vmversion': 0, 'code': f.read().hex()}
    run(cleos + 'multisig propose ' + pname + jsonArg(requestedPermissions) + jsonArg(
        trxPermissions) + 'eosio setcode' + jsonArg(
        setcode) + ' ' + proposer + ' ' + str(expirehours) + ' -p ' + proposer)

# msig to update system contract abi
def msigProposeUpdateSystemAbi(proposer, pname, bps, abidata, expirehours):
    requestedPermissions = []
    for i in range(0, len(bps)):
        requestedPermissions.append({'actor': bps[i], 'permission': 'active'})
    trxPermissions = [{'actor': 'eosio', 'permission': 'active'}]

    data = {'account': 'eosio', 'abi': abidata}
    run(cleos + 'multisig propose ' + pname + jsonArg(requestedPermissions) +
        jsonArg(trxPermissions) + 'eosio setabi' + jsonArg(
        data) + ' ' + proposer + ' ' + str(expirehours) + ' -p ' + proposer)

# ---------------------------------------------------------------------------------------------------
# msig to update system contract

# unlock wallet
unlockwallet_str = 'cleos wallet unlock -n ' + wallet_name + ' --password ' + wallet_password
runone(unlockwallet_str)

# get schedule active bps
active_bps = getbps()

# msig to setcode of eosio
#msigProposeUpdateSystem(active_account, proposal_name_upsyscode, active_bps, wasm_path, tx_expire_hours)

# msig to setabi of eosio
msigProposeUpdateSystemAbi(active_account, proposal_name_upsysabi, active_bps, getAbiData(abi_path), tx_expire_hours)
