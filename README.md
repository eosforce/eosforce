# EOS Force

<!-- vim-markdown-toc GFM -->

* [Supported Operating Systems](#supported-operating-systems)
* [Getting Started](#getting-started)
    * [Build from source](#build-from-source)
        * [Notes](#notes)
        * [Command reference](#command-reference)
    * [Docker](#docker)
    * [Seed list](#seed-list)
        * [P2P node](#p2p-node)
        * [Wallet node](#wallet-node)
* [Resources](#resources)

<!-- vim-markdown-toc -->

## Supported Operating Systems

EOS Force currently supports the following operating systems:

1. Ubuntu 16.04 (Ubuntu 16.10 recommended)
2. macOS 10.12 and higher (macOS 10.13.x recommended)

## Getting Started

### Build from source

```bash
# clone this repository and build
$ git clone https://github.com/eosforce/eosforce.git eosforce
$ cd eosforce
$ git submodule update --init --recursive
$ ./eosio_build.sh

# once you've built successfully
$ cd build && make install

# also copy the generated abi and wasm contract file to ~/.local/share/eosio/nodeos/config
$ cp build/contracts/eosio.token/eosio.token.abi build/contracts/eosio.token/eosio.token.wasm ~/.local/share/eosio/nodeos/config
$ cp build/contracts/System/System.abi build/contracts/System/System.wasm ~/.local/share/eosio/nodeos/config

# place some p2p nodes in here, see the following seed list section
$ vim ~/.local/share/eosio/nodeos/config/config.ini

# download genesis.json to ~/.local/share/eosio/nodeos/config
$ curl https://raw.githubusercontent.com/eosforce/genesis/master/genesis.json -o ~/.local/share/eosio/nodeos/config/genesis.json

$ cd build/programs/nodeos && ./nodeos
```

#### Notes

:warning:

- Make sure your config directory has these 6 files:

    ```bash
    config
    ├── System.abi
    ├── System.wasm
    ├── eosio.token.abi
    ├── eosio.token.wasm
    ├── config.ini
    └── genesis.json
    ```

    - `config.ini` with `p2p-peer-address` added. You can find some seed nodes in the following P2P node section.

    - `genesis.json` from [github.com/eosforce/genesis](https://raw.githubusercontent.com/eosforce/genesis/master/genesis.json).

    - compiled contract files: `System.abi`, `System.wasm`, `eosio.token.abi` and `eosio.token.wasm` from `build/contracts/System` and `build/contracts/eosio.token`.

- Ensure your chain id is correct when your node is up: `bd61ae3a031e8ef2f97ee3b0e62776d6d30d4833c8f7c1645c657b149151004b`

#### Command reference

- [RPC interface](https://documenter.getpostman.com/view/4394576/RWEnobze#17704cbd-13cd-4d21-a7dd-ab19287dd1fe)

- [CLI command reference](https://github.com/eosforce/contracts/tree/master/System#command-reference)

### Docker

[Run a node via docker](https://github.com/eosforce/genesis#run-a-node-via-docker)

### Seed list

#### P2P node

These IPs could be used as `p2p-peer-address` in your `config.ini`:

IP                  | Domain Name                 | By
:----:              | :----:                      | :----:
47.99.47.190:10321  | N/A                         | mathwallet
47.52.126.100:9123  | N/A                         | eospro
47.93.38.143:19951  | N/A                         | coinlord.one:
132.232.104.44:8001 | N/A                         | jiqix
47.91.175.9:15888   | p.eosco.top:15888           | eosco
112.74.179.235:9876 | N/A                         | eosgod
47.75.138.177:9876  | N/A                         | eosjedi
47.52.54.232:18933  | N/A                         | eosshuimu
47.104.255.49:65511 | http://p1.eosforce.cn:65511 | eosforce
47.254.71.89:8222   | http://p2.eosforce.cn:8222  | eosforce
47.75.126.7:19876   | http://p3.eosforce.cn:19876 | eosforce
47.96.232.211:8899  | http://p4.eosforce.cn:8899  | eosforce
47.97.122.109:7894  | http://p5.eosforce.cn:7894  | eosforce
101.132.77.22:9066  | http://p6.eosforce.cn:9066  | eosforce

#### Wallet node

The following IPs provide HTTP service for wallet.

IP                  | Domain Name            | By
:----:              | :----:                 | :----:
N/A                 | https://w1.eosforce.cn | eosfroce
N/A                 | https://w2.eosforce.cn | eosforce
N/A                 | https://w3.eosforce.cn | eosforce
47.52.54.232:8888   | N/A                    | eosshuimu
47.75.138.177:8888  | N/A                    | eosjedi
112.74.179.235:8888 | N/A                    | eosgod
47.91.175.9:8888    | p.eosco.top:8888       | eosco
132.232.104.44:8000 | N/A                    | jiqix
47.93.38.143:20188  | N/A                    | coinlord.one
47.52.126.100:8888  | N/A                    | eospro

## Resources

1. [Eosfore.io Website](http://eosforce.io)
2. [Community Telegram Group](https://t.me/eosforce_en)
3. [Developer Telegram Group](https://t.me/EOSForce)
