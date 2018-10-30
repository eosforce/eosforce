FROM eosforce/builder as builder
ARG branch=master

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get -y install openssl ssh && rm -rf /var/lib/apt/lists/*

RUN git clone https://github.com/eosforce/eosforce.git --recursive \
    && cd eosforce && echo "$branch:$(git rev-parse HEAD)" > /etc/eosforce-version && git submodule update --init --recursive \
    && cmake -H. -B"/tmp/build" -GNinja -DCMAKE_BUILD_TYPE=Release -DWASM_ROOT=/opt/wasm -DCMAKE_CXX_COMPILER=clang++ \
       -DCMAKE_C_COMPILER=clang -DCMAKE_INSTALL_PREFIX=/tmp/build  -DSecp256k1_ROOT_DIR=/usr/local -DBUILD_MONGO_DB_PLUGIN=true \
    && cmake --build /tmp/build --target install

FROM ubuntu:18.04

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get -y install openssl && rm -rf /var/lib/apt/lists/*
COPY --from=builder /usr/local/lib/* /usr/local/lib/
COPY --from=builder /tmp/build/bin /opt/eosforce/bin
COPY --from=builder /tmp/build/contracts  /tmp/contracts
RUN mkdir /tmp/config
COPY nodeosd.sh /opt/eosforce/bin/nodeosd.sh
COPY config.ini  /tmp/config
COPY contracts/genesis.json  /tmp/config
COPY contracts/System.abi  /tmp/config
COPY contracts/System.wasm  /tmp/config
COPY contracts/System01.abi  /tmp/config
COPY contracts/System01.wasm  /tmp/config
COPY contracts/eosio.token.abi  /tmp/config
COPY contracts/eosio.token.wasm  /tmp/config
COPY contracts/eosio.bios.abi  /tmp/config
COPY contracts/eosio.bios.wasm  /tmp/config
COPY contracts/eosio.msig.abi  /tmp/config
COPY contracts/eosio.msig.wasm  /tmp/config
ENV EOSIO_ROOT=/opt/eosforce
RUN chmod +x /opt/eosforce/bin/nodeosd.sh
ENV LD_LIBRARY_PATH /usr/local/lib
VOLUME /opt/eosforce/data
VOLUME /opt/eosforce/config
ENV PATH /opt/eosforce/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin