# Eosforce producer_heart_plugin

producer_heart_plugin提供了向主链发送心跳的功能。

# 配置方法

```confing.ini
plugin = eosio::producer_heartbeat_plugin
heartbeat-period = 1500
heartbeat-contract = eosio
heartbeat-permission = active
heartbeat-user = bpname
heartbeat-signature-provider = PUBKEY=KEY:PRIVATEKEY
```

+ heartbeat-period 是发送心跳操作的间隔，以秒为单位
+ heartbeat-contract 是部署心跳合约的合约帐户，Eosforce是eosio
+ heartbeat-permission 是心跳合约需要的用户的权限，这里使用active
+ heartbeat-user 是发送心跳的帐户名，一般是BP
+ heartbeat-signature-provider 是给心跳签名的公私钥，`PUBKEY`是公钥，`PRIVATEKEY` 是私钥
