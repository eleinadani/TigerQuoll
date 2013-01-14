#!/bin/bash

# 
# DIR is usually MC/js/src
DIR=$PWD/$(dirname $0)/..
MC=${DIR}/../..
BUILD=${DIR}/_build
INSTALL=${BUILD}/install

# 
# creates MC/js/src/_build
mkdir -p _build
mkdir -p ${BUILD}/js
mkdir -p ${BUILD}/nspr
mkdir -p ${BUILD}/install

# 
# build&install nspr
cd _build/nspr
cd ${BUILD}/nspr
${MC}/nsprpub/configure --enable-debug --enable-64bit --prefix="${INSTALL}"
make
make install

# 
# build&install mozilla-central
cd ${BUILD}/js
${MC}/js/src/configure --enable-debug --disable-optimize --enable-threadsafe --with-system-nspr --with-nspr-prefix="${INSTALL}" --prefix="${INSTALL}"
cp dist/include/js/HashTable.h dist/include/HashTable.h
cp dist/include/js/Vector.h dist/include/Vector.h
make
make install

cd ${DIR}
${MC}/js/src/configure --enable-debug --disable-optimize --enable-threadsafe --with-system-nspr --with-nspr-prefix="${INSTALL}" --prefix="${INSTALL}"
