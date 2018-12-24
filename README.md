# EOSFORCE

[Main Page](https://www.eosforce.io/?lang=en)

## Vision

To provide a more inclusive platform than any previously developed protocol, we will build a layered architecture with a single settlement layer main chain and multiple computing layer sidechains. The settlement layer main chain serves main functions of the base currency of the settlement layer and support token issuance on the settlement layer and coins on the computing sidechains. The settlement layer will focus on asset accounting and cross-chain interaction to keep it simple, efficient and secure. The computing layer will work to expand the network technology stack to support multiple underlying chain technologies and smart contract, including but not limited to WASM virtual machine-based EOSIO application chains, EVM virtual machine-based Ethereum application chains, IELE virtual machine-based Cardano chain, zero-knowledge proof-based Zcash chain, etc.

The computing layers adopt different development techniques, analyze advantages and disadvantages of various blockchain technologies to support multiple forms of sidechains. We offer options for application developers instead of confining them to a specific chain so that developers don’t have to pay for overpriced RAM. We work to increase performance of application chains, segregate security risk, and experiment new contract technologies.

#deploy

eosforce p2p  [peers](t.eosforce.io/p2p_list)

### 1. install eosforce with docker

	docker pull eosforce/eos:v1.3.2
	mkdir -p /data/eosforce
	wget https://updatewallet.oss-cn-hangzhou.aliyuncs.com/config.ini
	mv 	config.ini /data/eosforce/
	docker run -d --name eosforce-v1.3.2 -v /data/eosforce:/opt/eosio/bin/data-dir -v /data/nodeos:/root/.local/share/eosio/nodeos -p 9876:9876 -p 8888:8888 eosforce/eos:v1.3.2 nodeosd.sh
	
### 2. install with compile
   
   	git clone https://github.com/eosforce/eosforce.git
    cd eosforce
	git fetch
	git checkout force-v1.3.2
	git submodule update --init --recursive
	./eosio_build.sh

then  you must set eosforce's config path

	export configpath='~/eosforce/config'
	export datapath='~/eosforce/data'
	mkdir -p ~/eosforce/config
	cp build/contracts/eosio.lock/eosio.lock.abi  build/contracts/eosio.lock/eosio.lock.wasm $configpath
	cp build/contracts/System/System.abi build/contracts/System/System.wasm $configpath
	cp build/contracts/System01/System01.abi build/contracts/System01/System01.wasm $configpath
	cp build/contracts/eosio.token/eosio.token.abi build/contracts/eosio.token/eosio.token.wasm $configpath
	cp build/contracts/eosio.msig/eosio.msig.abi build/contracts/eosio.msig/eosio.msig.wasm $configpath
	wget https://updatewallet.oss-cn-hangzhou.aliyuncs.com/eosforce/activeacc.json 
	
	wget https://updatewallet.oss-cn-hangzhou.aliyuncs.com/config.ini 
	wget https://raw.githubusercontent.com/eosforce/genesis/master/genesis.json 
	mv activeacc.json config.ini genesis.json $configpath 
	./build/bin/nodeos --config-dir $configpath --data-dir $datapath
	
	
#if you join node of eosforce

### modify config.ini
you must modify config.ini
	
	producer-name = bpname 
	signature-provider = $public_block_key=KEY:$private_block_key

then restart nodeos

### you must have a account name of eosforce

for example account name has a public key and a private key below:

 public_key: Eos1111111111111111111
 
 private_key: 5J1111111111111111111

account name of sign in must is the same with  bpname  

	cleos wallet import $private_key
	cleos -u https://w1.eosforce.cn push action eosio updatebp '{"bpname":"bpname","block_signing_key":"block_signing_key","commission_rate":"commission_rate","url":"https://eosforce.io"}' -p bpname
	

options:

block_signing_key:  the same with public_block_key of config.ini

commission_rate: it is range(0, 10000), commission_rate is 1000 means give bp 10% of dividents


(warn: bpname must is the same with account name)

if you finish step above, then success for deploy 







