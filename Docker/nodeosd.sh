#!/bin/sh
cd /opt/eosforce/bin

if [ -f '/opt/eosforce/config/config.ini' ]; then
    echo 
  else
    echo "need to config config.ini"
    cp -R /tmp/config/config.ini /opt/eosforce/config/
    exit 1
fi

if [ -f '/opt/eosforce/config/genesis.json' ]; then
    echo
  else
    cp -R /tmp/config/* /opt/eosforce/config/
fi

CONFIG_DIR="--config-dir /opt/eosforce/config/ -d /opt/eosforce/data/"

exec /opt/eosforce/bin/nodeos $CONFIG_DIR $@
