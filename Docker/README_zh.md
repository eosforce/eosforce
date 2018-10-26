# 使用Docker运行Eosforce

我们可以使用Docker简便而快速的运行Eosforce。

## 安装依赖项

- [Docker](https://docs.docker.com) Docker 17.05 or higher is required
- [docker-compose](https://docs.docker.com/compose/) version >= 1.10.0

## Docker需求

- At least 7GB RAM (Docker -> Preferences -> Advanced -> Memory -> 7GB or above)
- If the build below fails, make sure you've adjusted Docker Memory settings and try again.

## 构建Eosfroce镜像

```bash
git clone https://github.com/eosforce/eosforce.git --recursive  --depth 1
cd eosforce/Docker
docker build . -t eosforce/eos
```

以上的镜像基于master最新版本，我们也可以指定tag编译：

```bash
docker build -t eosforce/eos:v1.2.3 --build-arg branch=v1.2.3 .
```

## 单节点启动说明

要启动单独的节点，我们首先需要准备config.ini文件，示例文件在 https://github.com/eosforce/eosforce/blob/master/Docker/config.ini

需要修改的项：

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

之后我们需要为nodeos建立config目录和data目录，其中需要把上面的config.ini复制到config路径下。

```bash
mkdir ./nodeos
mkdir ./nodeos/config
mkdir ./nodeos/data
cp config.ini ./nodeos/config/
```

启动:

```bash
sudo docker run -d --name eosforcenode -v ./nodeos/config:/opt/eosforce/config -v ./nodeos/data:/opt/eosforce/data  -p 9001:9001 -p 9876:9876 eosforce/eostest nodeosd.sh
```

如果config对应的目录下没有config.ini文件，容器会复制一个示例配置到config路径下，需要更改配置之后再启动。

## Get chain info

```bash
curl http://127.0.0.1:89001888/v1/chain/get_info
```