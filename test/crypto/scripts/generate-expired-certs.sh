#!/bin/sh -e
#
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2025 Comcast Cable Communications Management, LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
#
#
# ----------------------------------------------------------------------------
#
# Script used to generate the keys and certificates for testing, it generates
# a root CA and then 3 intermediate certs with various expiry times.  It generates
# 3 sets of test certificates, in each set the expiry dates of the intermediate
# certs are juggled so that a different certificate in the chain is picked as
# having the correct time to use for verifying the expiry.
#
# This is here as a reference, it's not intended to be run as part of the
# unit tests.
#


createRootCa()
{
  NAME=$1
  START_DATE=$2
  DAYS=$3

  faketime "${START_DATE}" \
    openssl req -x509 \
      -config root.conf \
      -extensions v3_ca \
      -subj '/CN=Root/O=HelloWorld/C=NZ' \
      -newkey rsa:4096 \
      -keyout ${NAME}.key \
      -out ${NAME}.crt \
      -days ${DAYS} \
      -nodes
}

# createIntermediateCA(NEW_CA_NAME, ROOT_CA_NAME, START_DATE, DAYS)
createIntermediateCA()
{
  NEW_CA_NAME=$1
  ROOT_CA_NAME=$2
  START_DATE=$3
  DAYS=$4

  COMMON_NAME=`echo "${NEW_CA_NAME}" | sed -e "s|/|_|g"`

  openssl genrsa \
    -out ${NEW_CA_NAME}.key \
    2048

  openssl req -new \
    -sha256 \
    -nodes \
    -key ${NEW_CA_NAME}.key \
    -subj "/C=NZ/ST=ON/O=HelloWorld/CN=${COMMON_NAME}" \
    -out ${NEW_CA_NAME}.csr

  faketime "${START_DATE}" \
    openssl x509 -req \
      -passin pass: \
      -extfile inter.conf \
      -extensions ext \
      -in ${NEW_CA_NAME}.csr \
      -CA ${ROOT_CA_NAME}.crt \
      -CAkey ${ROOT_CA_NAME}.key \
      -CAcreateserial \
      -out ${NEW_CA_NAME}.crt \
      -days ${DAYS} \
      -sha256
}

# Working directory
WORKINGDIR="${1:-.}/../data/expired"
cd "$WORKINGDIR"

# Create a root CA valid from 2020 -> 2050
createRootCa root "2020-01-01 01:23:45" 10958


# Create chain for test 1
mkdir -p test1
TEST_DIR="test1"
  createIntermediateCA ${TEST_DIR}/intermediate-1 root                       "2022-01-01 01:23:45" 730
  createIntermediateCA ${TEST_DIR}/intermediate-2 ${TEST_DIR}/intermediate-1 "2021-01-01 01:23:45" 730
  createIntermediateCA ${TEST_DIR}/intermediate-3 ${TEST_DIR}/intermediate-2 "2021-01-01 01:23:45" 730

  cat \
    ${TEST_DIR}/intermediate-1.crt \
    ${TEST_DIR}/intermediate-2.crt \
    > ${TEST_DIR}/signing.crt.chain

  openssl verify \
    -x509_strict \
    -no_check_time \
    -CAfile root.crt \
    -untrusted ${TEST_DIR}/signing.crt.chain \
    ${TEST_DIR}/intermediate-3.crt

  echo "Look at me, I'm signed" | tr -d '\n' > ${TEST_DIR}/root.hash
  openssl smime -sign -noattr -nodetach -binary \
    -in ${TEST_DIR}/root.hash \
    -inkey ${TEST_DIR}/intermediate-3.key \
    -signer ${TEST_DIR}/intermediate-3.crt \
    -certfile ${TEST_DIR}/signing.crt.chain \
    -outform der \
    -out ${TEST_DIR}/root.hash.signed


# Create chain for test 2
mkdir -p test2
TEST_DIR="test2"
  createIntermediateCA ${TEST_DIR}/intermediate-1 root                       "2021-01-01 01:23:45" 730
  createIntermediateCA ${TEST_DIR}/intermediate-2 ${TEST_DIR}/intermediate-1 "2022-01-01 01:23:45" 730
  createIntermediateCA ${TEST_DIR}/intermediate-3 ${TEST_DIR}/intermediate-2 "2021-01-01 01:23:45" 730

  cat \
    ${TEST_DIR}/intermediate-1.crt \
    ${TEST_DIR}/intermediate-2.crt \
    > ${TEST_DIR}/signing.crt.chain

  openssl verify \
    -x509_strict \
    -no_check_time \
    -CAfile root.crt \
    -untrusted ${TEST_DIR}/signing.crt.chain \
    ${TEST_DIR}/intermediate-3.crt

  echo "I'm also signed, so there" | tr -d '\n' > ${TEST_DIR}/root.hash
  openssl smime -sign -noattr -nodetach -binary \
    -in ${TEST_DIR}/root.hash \
    -inkey ${TEST_DIR}/intermediate-3.key \
    -signer ${TEST_DIR}/intermediate-3.crt \
    -certfile ${TEST_DIR}/signing.crt.chain \
    -outform der \
    -out ${TEST_DIR}/root.hash.signed


# Create chain for test 3
mkdir -p test3
TEST_DIR="test3"
  createIntermediateCA ${TEST_DIR}/intermediate-1 root                       "2021-01-01 01:23:45" 730
  createIntermediateCA ${TEST_DIR}/intermediate-2 ${TEST_DIR}/intermediate-1 "2021-01-01 01:23:45" 730
  createIntermediateCA ${TEST_DIR}/intermediate-3 ${TEST_DIR}/intermediate-2 "2022-01-01 01:23:45" 730

  cat \
    ${TEST_DIR}/intermediate-1.crt \
    ${TEST_DIR}/intermediate-2.crt \
    > ${TEST_DIR}/signing.crt.chain

  openssl verify \
    -x509_strict \
    -no_check_time \
    -CAfile root.crt \
    -untrusted ${TEST_DIR}/signing.crt.chain \
    ${TEST_DIR}/intermediate-3.crt

  echo "ROOT_HASH_FOR_TEST_3 - boring" | tr -d '\n' > ${TEST_DIR}/root.hash
  openssl smime -sign -noattr -nodetach -binary \
    -in ${TEST_DIR}/root.hash \
    -inkey ${TEST_DIR}/intermediate-3.key \
    -signer ${TEST_DIR}/intermediate-3.crt \
    -certfile ${TEST_DIR}/signing.crt.chain \
    -outform der \
    -out ${TEST_DIR}/root.hash.signed



mkdir -p test4
TEST_DIR="test4"
  # Create expired root CA
  createRootCa ${TEST_DIR}/expired-root "2020-01-01 01:23:45" 730

  # Create non-expired signing cert
  createIntermediateCA ${TEST_DIR}/signing ${TEST_DIR}/expired-root "2020-01-01 01:23:45" 10958

  echo "I'm signed with an valid certificate but expired root CA" | tr -d '\n' > ${TEST_DIR}/root.hash
  openssl smime -sign -noattr -nodetach -binary \
    -in ${TEST_DIR}/root.hash \
    -inkey ${TEST_DIR}/signing.key \
    -signer ${TEST_DIR}/signing.crt \
    -outform der \
    -out ${TEST_DIR}/root.hash.signed



