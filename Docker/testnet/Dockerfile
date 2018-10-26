FROM eosforce/builder as builder
ARG branch=master

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get -y install openssl ssh && rm -rf /var/lib/apt/lists/*

RUN git clone https://github.com/eosforce/eosforce.git --recursive \
    && cd eosforce && echo "$branch:$(git rev-parse HEAD)" > /etc/eosforce-version
    
RUN cd eosforce && git submodule update --init --recursive

RUN cd eosforce && cmake -H. -B"/tmp/build" -GNinja -DCMAKE_BUILD_TYPE=Release -DWASM_ROOT=/opt/wasm -DCMAKE_CXX_COMPILER=clang++ \
       -DCMAKE_C_COMPILER=clang -DCMAKE_INSTALL_PREFIX=/tmp/build  -DSecp256k1_ROOT_DIR=/usr/local -DBUILD_MONGO_DB_PLUGIN=true \
    && cmake --build /tmp/build --target install

FROM ubuntu:18.04

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get -y install openssl && rm -rf /var/lib/apt/lists/*
COPY --from=builder /usr/local/lib/* /usr/local/lib/
COPY --from=builder /tmp/build /opt/eosforce/build

COPY bios_boot_eosforce.py  /opt/eosforce/
COPY start.sh  /opt/eosforce/

RUN chmod +x /opt/eosforce/start.sh
RUN chmod +x /opt/eosforce/bios_boot_eosforce.py
ENV LD_LIBRARY_PATH /usr/local/lib
VOLUME /opt/eosforce
ENV PATH /opt/eosforce:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get -y install python