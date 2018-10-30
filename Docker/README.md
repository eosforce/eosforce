# Run in docker

[中文](https://github.com/eosforce/eosforce/blob/master/Docker/README_zh.md)

Simple and fast setup of Eosforce on Docker is also available.

## Install Dependencies

- [Docker](https://docs.docker.com) Docker 17.05 or higher is required
- [docker-compose](https://docs.docker.com/compose/) version >= 1.10.0

## Docker Requirement

- At least 7GB RAM (Docker -> Preferences -> Advanced -> Memory -> 7GB or above)
- If the build below fails, make sure you've adjusted Docker Memory settings and try again.

## Build eosforce image

```bash
git clone https://github.com/eosforce/eosforce.git --recursive  --depth 1
cd eosforce/Docker
docker build . -t eosforce/eos
```

The above will build off the most recent commit to the master branch by default. If you would like to target a specific branch/tag, you may use a build argument. For example, if you wished to generate a docker image based off of the v1.4.1 tag, you could do the following:

```bash
docker build -t eosio/eos:v1.4.1 --build-arg branch=v1.4.1 .
```

## Start nodeos docker container only

At First, we need to make a config.ini, A simple config.ini is in https://github.com/eosforce/eosforce/blob/master/Docker/config.ini.

Some config need change:

```ini
# some key config need to change:

# The local IP and port to listen for incoming http connections; set blank to disable. (eosio::http_plugin)
http-server-address = 0.0.0.0:9001 # http server address if use eosio::http_plugin

# The actual host:port used to listen for incoming p2p connections. (eosio::net_plugin)
p2p-listen-endpoint = 0.0.0.0:9876

# The public endpoint of a peer node to connect to. Use multiple p2p-peer-address options as needed to compose a network. (eosio::net_plugin)
# An externally accessible host:port for identifying this node. Defaults to p2p-listen-endpoint. (eosio::net_plugin)
#p2p-server-address = 172.16.196.158:7891
p2p-peer-address = 127.0.0.1:7891

# Key=Value pairs in the form <public-key>=<provider-spec>
# Where:
#    <public-key>    	is a string form of a vaild EOSIO public key
#    <provider-spec> 	is a string in the form <provider-type>:<data>
#    <provider-type> 	is KEY, or KEOSD
#    KEY:<data>      	is a string form of a valid EOSIO private key which maps to the provided public key
#    KEOSD:<data>    	is the URL where keosd is available and the approptiate wallet(s) are unlocked (eosio::producer_plugin)
#signature-provider = EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV=KEY:5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3

# Plugin(s) to enable, may be specified multiple times
plugin = eosio::chain_api_plugin
#plugin = eosio::history_plugin
#plugin = eosio::http_plugin

# other configs

...

```

Then we need to make a config folder and data folder for nodeos run in docker, and copy config.ini to config folder:

```bash
mkdir ./nodeos
mkdir ./nodeos/config
mkdir ./nodeos/data
cp config.ini ./nodeos/config/
```

start nodeos:

```bash
sudo docker run -d --name eosforcenode -v ./nodeos/config:/opt/eosforce/config -v ./nodeos/data:/opt/eosforce/data  -p 9001:9001 -p 9876:9876 eosforce/eostest nodeosd.sh
```

if you have no config.ini in config folder, the docker image will copy a common config to the path and exit, you need change config and start again.

## Get chain info

```bash
curl http://127.0.0.1:89001888/v1/chain/get_info
```

## Start both nodeos and keosd containers

```bash
docker volume create --name=nodeos-data-volume
docker volume create --name=keosd-data-volume
docker-compose up -d
```

After `docker-compose up -d`, two services named `nodeosd` and `keosd` will be started. nodeos service would expose ports 8888 and 9876 to the host. keosd service does not expose any port to the host, it is only accessible to cleos when running cleos is running inside the keosd container as described in "Execute cleos commands" section.

### Execute cleos commands

You can run the `cleos` commands via a bash alias.

```bash
alias cleos='docker-compose exec keosd /opt/eosio/bin/cleos -u http://nodeosd:8888 --wallet-url http://localhost:8900'
cleos get info
cleos get account inita
```

Upload sample exchange contract

```bash
cleos set contract exchange contracts/exchange/
```

If you don't need keosd afterwards, you can stop the keosd service using

```bash
docker-compose stop keosd
```

### Develop/Build custom contracts

Due to the fact that the eosio/eos image does not contain the required dependencies for contract development (this is by design, to keep the image size small), you will need to utilize the eosio/eos-dev image. This image contains both the required binaries and dependencies to build contracts using eosiocpp.

You can either use the image available on [Docker Hub](https://hub.docker.com/r/eosio/eos-dev/) or navigate into the dev folder and build the image manually.

```bash
cd dev
docker build -t eosio/eos-dev .
```

### Change default configuration

You can use docker compose override file to change the default configurations. For example, create an alternate config file `config2.ini` and a `docker-compose.override.yml` with the following content.

```yaml
version: "2"

services:
  nodeos:
    volumes:
      - nodeos-data-volume:/opt/eosio/bin/data-dir
      - ./config2.ini:/opt/eosio/bin/data-dir/config.ini
```

Then restart your docker containers as follows:

```bash
docker-compose down
docker-compose up
```

### Clear data-dir

The data volume created by docker-compose can be deleted as follows:

```bash
docker volume rm nodeos-data-volume
docker volume rm keosd-data-volume
```

### Docker Hub

Docker Hub image available from [docker hub](https://hub.docker.com/r/eosio/eos/).
Create a new `docker-compose.yaml` file with the content below

```bash
version: "3"

services:
  nodeosd:
    image: eosio/eos:latest
    command: /opt/eosio/bin/nodeosd.sh --data-dir /opt/eosio/bin/data-dir -e --http-alias=nodeosd:8888 --http-alias=127.0.0.1:8888 --http-alias=localhost:8888
    hostname: nodeosd
    ports:
      - 8888:8888
      - 9876:9876
    expose:
      - "8888"
    volumes:
      - nodeos-data-volume:/opt/eosio/bin/data-dir

  keosd:
    image: eosio/eos:latest
    command: /opt/eosio/bin/keosd --wallet-dir /opt/eosio/bin/data-dir --http-server-address=127.0.0.1:8900 --http-alias=localhost:8900 --http-alias=keosd:8900
    hostname: keosd
    links:
      - nodeosd
    volumes:
      - keosd-data-volume:/opt/eosio/bin/data-dir

volumes:
  nodeos-data-volume:
  keosd-data-volume:

```

*NOTE:* the default version is the latest, you can change it to what you want

run `docker pull eosio/eos:latest`

run `docker-compose up`

### EOSIO Testnet

We can easily set up a EOSIO local testnet using docker images. Just run the following commands:

Note: if you want to use the mongo db plugin, you have to enable it in your `data-dir/config.ini` first.

```
# create volume
docker volume create --name=nodeos-data-volume
docker volume create --name=keosd-data-volume
# pull images and start containers
docker-compose -f docker-compose-eosio-latest.yaml up -d
# get chain info
curl http://127.0.0.1:8888/v1/chain/get_info
# get logs
docker-compose logs -f nodeosd
# stop containers
docker-compose -f docker-compose-eosio-latest.yaml down
```

The `blocks` data are stored under `--data-dir` by default, and the wallet files are stored under `--wallet-dir` by default, of course you can change these as you want.

### About MongoDB Plugin

Currently, the mongodb plugin is disabled in `config.ini` by default, you have to change it manually in `config.ini` or you can mount a `config.ini` file to `/opt/eosio/bin/data-dir/config.ini` in the docker-compose file.
