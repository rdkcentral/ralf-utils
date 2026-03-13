#!/bin/sh
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
# ----------------------------------------------------------------------------
#
# Script used to a private key and certificate chain for signing packages.
# This is used to sign the packages created for unit tests in test/core,
# it is not intended to be run as part of the unit tests.
#

# Output directory (default: current directory)
OUTDIR="${1:-.}/../data/certs"
mkdir -p "$OUTDIR"

# File names
ROOT_KEY="$OUTDIR/rootCA.key.pem"
ROOT_CERT="$OUTDIR/rootCA.cert.pem"
ROOT_CONF="$OUTDIR/rootCA.conf"
INT_KEY="$OUTDIR/intermediateCA.key.pem"
INT_CSR="$OUTDIR/intermediateCA.csr.pem"
INT_CERT="$OUTDIR/intermediateCA.cert.pem"
INT_CONF="$OUTDIR/intermediateCA.conf"
SIGN_KEY="$OUTDIR/signing.key.pem"
SIGN_CSR="$OUTDIR/signing.csr.pem"
SIGN_CERT="$OUTDIR/signing.cert.pem"
CHAIN_CERT="$OUTDIR/signing.chain.pem"
PKCS12_FILE="$OUTDIR/signing-creds.p12"

# Generate root CA key and certificate
openssl req -x509 -config "$ROOT_CONF" -extensions v3_ca \
    -subj '/C=NZ/ST=State/L=City/O=ExampleOrg/CN=ExampleRootCA' \
    -newkey rsa:4096 -keyout "$ROOT_KEY" -out "$ROOT_CERT" \
    -sha256 -days 3650 -nodes

# Generate intermediate CA key and CSR
openssl genrsa -out "$INT_KEY" 4096
openssl req -new -key "$INT_KEY" -subj "/C=NZ/ST=State/L=City/O=ExampleOrg/CN=ExampleIntermediateCA" -out "$INT_CSR"

# Sign intermediate CA CSR with root CA
openssl x509 -req -in "$INT_CSR" -CA "$ROOT_CERT" -CAkey "$ROOT_KEY" -CAcreateserial \
    -extfile "$INT_CONF" -extensions ext -out "$INT_CERT" -days 1825 -sha256
rm "$INT_CSR"

# Generate signing key and CSR
openssl genrsa -out "$SIGN_KEY" 4096
openssl req -new -key "$SIGN_KEY" -subj "/C=NZ/ST=State/L=City/O=ExampleOrg/CN=PackageSigner" -out "$SIGN_CSR"

# Sign signing CSR with intermediate CA
openssl x509 -req -in "$SIGN_CSR" -CA "$INT_CERT" -CAkey "$INT_KEY" -CAcreateserial \
    -out "$SIGN_CERT" -days 365 -sha256
rm "$SIGN_CSR"

# Create certificate chain (signing cert + intermediate + root)
# cat "$SIGN_CERT" "$INT_CERT" "$ROOT_CERT" > "$CHAIN_CERT"

# Verify the newly created certificate chain
openssl verify -x509_strict -no_check_time -CAfile "$ROOT_CERT" -untrusted "$INT_CERT" "$SIGN_CERT"


# Create PKCS#12 file containing signing key and certificate chain
openssl pkcs12 -export -inkey "$SIGN_KEY" -in "$SIGN_CERT" -certfile "$INT_CERT" \
    -out "$PKCS12_FILE" -name "PackageSigner" -password pass:


# Print summary
cat <<EOF
Generated credentials:
  Private key: $SIGN_KEY
  Root CA: $ROOT_CERT
  Intermediate CA: $INT_CERT
  Signing certificate: $SIGN_CERT
  PKCS#12 file: $PKCS12_FILE
EOF
