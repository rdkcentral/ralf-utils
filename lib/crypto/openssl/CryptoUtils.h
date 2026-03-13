/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 Comcast Cable Communications Management, LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/pkcs7.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include <functional>
#include <memory>
#include <string>

/// Some std::unique_ptr wrappers around openssl objects to manage their allocation and free
using BIOUniquePtr = std::unique_ptr<BIO, std::function<void(BIO *)>>;
using X509UniquePtr = std::unique_ptr<X509, std::function<void(X509 *)>>;
using ASN1_TIMEUniquePtr = std::unique_ptr<ASN1_TIME, std::function<void(ASN1_TIME *)>>;
using PKCS7UniquePtr = std::unique_ptr<PKCS7, std::function<void(PKCS7 *)>>;
using X509StoreUniquePtr = std::unique_ptr<X509_STORE, std::function<void(X509_STORE *)>>;
using X509StoreCtxUniquePtr = std::unique_ptr<X509_STORE_CTX, std::function<void(X509_STORE_CTX *)>>;
using X509StackUniquePtr = std::unique_ptr<STACK_OF(X509), std::function<void(STACK_OF(X509) *)>>;
using EVP_PKEYUniquePtr = std::unique_ptr<EVP_PKEY, std::function<void(EVP_PKEY *)>>;
using EVP_MD_CTXUniquePtr = std::unique_ptr<EVP_MD_CTX, std::function<void(EVP_MD_CTX *)>>;

///
std::string getLastOpenSSLError();
