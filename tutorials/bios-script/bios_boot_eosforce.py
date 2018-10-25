#!/usr/bin/env python

import argparse
import json
import os
import subprocess
import sys
import time

args = None
logFile = None

unlockTimeout = 999999999

relayPriKey = '5JbykKocbS8Hj3s4xokv5Ej3iXqrSqdwxBcHQFXf5DwUmGELxTi'
relayPubKey = 'EOS5NiqXiggrB5vyfedTFseDi6mW4U74bBhR7S2KSq181jHdYMNVY'

relayAccounts = [
    'r.token.in',
    'r.token.out',
    'r.acc.map',
    'r.t.exchange'
]

def jsonArg(a):
    return " '" + json.dumps(a) + "' "

def run(args):
    print('bios-boot-tutorial.py:', args)
    logFile.write(args + '\n')
    if subprocess.call(args, shell=True):
        print('bios-boot-eosforce.py: exiting because of error')
        sys.exit(1)

def retry(args):
    while True:
        print('bios-boot-eosforce.py:', args)
        logFile.write(args + '\n')
        if subprocess.call(args, shell=True):
            print('*** Retry')
        else:
            break

def background(args):
    print('bios-boot-eosforce.py:', args)
    logFile.write(args + '\n')
    return subprocess.Popen(args, shell=True)

def sleep(t):
    print('sleep', t, '...')
    time.sleep(t)
    print('resume')

def makeGenesis():
    run('rm -rf ' + os.path.abspath(args.config_dir))
    run('mkdir -p ' + os.path.abspath(args.config_dir))
    run('mkdir -p ' + os.path.abspath(args.config_dir) + '/keys/' )

    run('cp ' + args.contracts_dir + '/eosio.token/eosio.token.abi ' + os.path.abspath(args.config_dir))
    run('cp ' + args.contracts_dir + '/eosio.token/eosio.token.wasm ' + os.path.abspath(args.config_dir))
    run('cp ' + args.contracts_dir + '/System/System.abi ' + os.path.abspath(args.config_dir))
    run('cp ' + args.contracts_dir + '/System/System.wasm ' + os.path.abspath(args.config_dir))
    run('cp ' + args.contracts_dir + '/System01/System01.abi ' + os.path.abspath(args.config_dir))
    run('cp ' + args.contracts_dir + '/System01/System01.wasm ' + os.path.abspath(args.config_dir))
    run('cp ' + args.contracts_dir + '/eosio.bios/eosio.bios.abi ' + os.path.abspath(args.config_dir))
    run('cp ' + args.contracts_dir + '/eosio.bios/eosio.bios.wasm ' + os.path.abspath(args.config_dir))
    run('cp ' + args.contracts_dir + '/eosio.msig/eosio.msig.abi ' + os.path.abspath(args.config_dir))
    run('cp ' + args.contracts_dir + '/eosio.msig/eosio.msig.wasm ' + os.path.abspath(args.config_dir))

    run('echo "System01-contract-block-num=5" > ' + os.path.abspath(args.config_dir) + '/config.ini')

    run(args.root + 'build/programs/genesis/genesis')
    run('mv ./genesis.json ' + os.path.abspath(args.config_dir))

    run('mv ./key.json ' + os.path.abspath(args.config_dir) + '/keys/')
    run('mv ./sigkey.json ' + os.path.abspath(args.config_dir) + '/keys/')

def addB1Account():
    run(args.cleos + 'create account eosforce b1 ' + initAccounts[len(initAccounts) - 1]['key'] + " -p eosforce")

def addRelayAccount():
    for a in relayAccounts:
        run(args.cleos + 'create account eosforce ' + a + ' ' + relayPubKey)
        run(args.cleos + 'push action eosio transfer \'{"from":"eosforce","to":"%s","quantity":"100000.0000 EOS","memo":""}\' -p eosforce' % a)


def startWallet():
    run('rm -rf ' + os.path.abspath(args.wallet_dir))
    run('mkdir -p ' + os.path.abspath(args.wallet_dir))
    background(args.keosd + ' --unlock-timeout %d --http-server-address 0.0.0.0:6666 --wallet-dir %s' % (unlockTimeout, os.path.abspath(args.wallet_dir)))
    sleep(.4)
    run(args.cleos + 'wallet create --file ./pw')

def importKeys():
    keys = {}
    run(args.cleos + 'wallet import --private-key ' + relayPriKey)
    for a in initAccountsKeys:
        key = a[1]
        if not key in keys:
            keys[key] = True
            # note : new develop eosforce change this command to wallet import--private-key pk
            # so need change this cmd
            run(args.cleos + 'wallet import --private-key ' + key)

def startNode(nodeIndex, bpaccount, key):
    dir = args.nodes_dir + ('%02d-' % nodeIndex) + bpaccount['name'] + '/'
    run('rm -rf ' + dir)
    run('mkdir -p ' + dir)
    otherOpts = ''.join(list(map(lambda i: '    --p2p-peer-address 0.0.0.0:' + str(9001 + i), range(24 - 1))))
    if not nodeIndex: otherOpts += (
        '    --plugin eosio::history_plugin'
        '    --plugin eosio::history_api_plugin'
    )

    print('bpaccount ', bpaccount)
    print('key ', key, ' ', key[1])

    cmd = (
        args.nodeos +
        '    --blocks-dir ' + os.path.abspath(dir) + '/blocks'
        '    --config-dir ' + os.path.abspath(dir) + '/../../config'
        '    --data-dir ' + os.path.abspath(dir) +
        '    --http-server-address 0.0.0.0:' + str(8000 + nodeIndex) +
        '    --p2p-listen-endpoint 0.0.0.0:' + str(9000 + nodeIndex) +
        '    --max-clients ' + str(maxClients) +
        '    --p2p-max-nodes-per-host ' + str(maxClients) +
        '    --enable-stale-production'
        '    --producer-name ' + bpaccount['name'] +
        '    --signature-provider=' + bpaccount['bpkey'] + '=KEY:' + key[1] +
        '    --plugin eosio::http_plugin'
        '    --plugin eosio::chain_api_plugin'
        '    --plugin eosio::producer_plugin' +
        otherOpts)
    with open(dir + '../' + bpaccount['name'] + '.log', mode='w') as f:
        f.write(cmd + '\n\n')
    background(cmd + '    2>>' + dir + '../' + bpaccount['name'] + '.log')

def startProducers(inits, keys):
    for i in range(0, len(inits)):
        startNode(i + 1, initProducers[i], keys[i])

def listProducers():
    run(args.cleos + 'get table eosio eosio bps')

def stepKillAll():
    run('killall keosd nodeos || true')
    sleep(1.5)

def stepStartWallet():
    startWallet()
    importKeys()

def stepStartProducers():
    startProducers(initProducers, initProducerSigKeys)
    sleep(3)
    addB1Account()
    addRelayAccount()
    sleep(args.producer_sync_delay)

def stepLog():
    run('tail -n 1000 ' + args.nodes_dir + 'biosbpa.log')
    listProducers()




# =======================================================================================================================
# Command Line Arguments
parser = argparse.ArgumentParser()

commands = [
    # ('k', 'kill',           stepKillAll,                True,    "Kill all nodeos and keosd processes"),
    ('w', 'wallet',         stepStartWallet,            True,    "Start keosd, create wallet, fill with keys"),
    ('P', 'start-prod',     stepStartProducers,         True,    "Start producers"),
    ('l', 'log',            stepLog,                    True,    "Show tail of node's log"),
]

parser.add_argument('--root', metavar='', help="EOSIO Public Key", default='../../')
parser.add_argument('--contracts-dir', metavar='', help="Path to contracts directory", default='build/contracts/')
parser.add_argument('--cleos', metavar='', help="Cleos command", default='build/programs/cleos/cleos --wallet-url http://0.0.0.0:6666 ')
parser.add_argument('--nodeos', metavar='', help="Path to nodeos binary", default='build/programs/nodeos/nodeos')
parser.add_argument('--nodes-dir', metavar='', help="Path to nodes directory", default='./nodes/')
parser.add_argument('--keosd', metavar='', help="Path to keosd binary", default='build/programs/keosd/keosd')
parser.add_argument('--log-path', metavar='', help="Path to log file", default='./output.log')
parser.add_argument('--wallet-dir', metavar='', help="Path to wallet directory", default='./wallet/')
parser.add_argument('--config-dir', metavar='', help="Path to config directory", default='./config')
parser.add_argument('--producer-sync-delay', metavar='', help="Time (s) to sleep to allow producers to sync", type=int, default=10)
parser.add_argument('-a', '--all', action='store_true', help="Do everything marked with (*)")
parser.add_argument('-H', '--http-port', type=int, default=8001, metavar='', help='HTTP port for cleos')

for (flag, command, function, inAll, help) in commands:
    prefix = ''
    if inAll: prefix += '*'
    if prefix: help = '(' + prefix + ') ' + help
    if flag:
        parser.add_argument('-' + flag, '--' + command, action='store_true', help=help, dest=command)
    else:
        parser.add_argument('--' + command, action='store_true', help=help, dest=command)

args = parser.parse_args()

args.cleos += '--url http://127.0.0.1:%d ' % args.http_port

args.cleos = args.root + args.cleos
args.nodeos = args.root + args.nodeos
args.keosd = args.root + args.keosd
args.contracts_dir = args.root + args.contracts_dir

logFile = open(args.log_path, 'a')

logFile.write('\n\n' + '*' * 80 + '\n\n\n')

makeGenesis()

with open(os.path.abspath(args.config_dir) + '/genesis.json') as f:
    a = json.load(f)
    initAccounts = a['initial_account_list']
    initProducers = a['initial_producer_list']

with open(os.path.abspath(args.config_dir) + '/keys/sigkey.json') as f:
    a = json.load(f)
    initProducerSigKeys = a['keymap']

with open(os.path.abspath(args.config_dir) + '/keys/key.json') as f:
    a = json.load(f)
    initAccountsKeys = a['keymap']

maxClients = len(initProducers) + 10

haveCommand = False
for (flag, command, function, inAll, help) in commands:
    if getattr(args, command) or inAll and args.all:
        if function:
            haveCommand = True
            function()

if not haveCommand:
    print('bios-boot-eosforce.py: Tell me what to do. -a does almost everything. -h shows options.')
