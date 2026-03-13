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

#include "XmlDigitalSignature.h"

#include <libxml/tree.h>

#include <cinttypes>
#include <memory>
#include <string>
#include <vector>

class XmlDigitalSignatureImpl
{
public:
    static std::shared_ptr<XmlDigitalSignatureImpl> createImpl(xmlDoc *_Nonnull document,
                                                               LIBRALF_NS::Error *_Nullable error);

public:
    XmlDigitalSignatureImpl(xmlDoc *_Nonnull document, xmlNode *_Nonnull signedInfoNode,
                            xmlNode *_Nonnull signedValueNode, xmlNode *_Nonnull keyInfoNode);
    ~XmlDigitalSignatureImpl();

    bool verify(const LIBRALF_NS::VerificationBundle &bundle, XmlDigitalSignature::VerifyOptions options,
                LIBRALF_NS::Error *_Nullable error) const;

    std::vector<XmlDigitalSignature::Reference> references(LIBRALF_NS::Error *_Nullable error) const;

    std::list<LIBRALF_NS::Certificate> signingCertificates(LIBRALF_NS::Error *_Nullable error) const;

private:
    static int isSignedInfoNode(void *_Nonnull userData, xmlNodePtr _Nullable node, xmlNodePtr _Nullable parent);

    std::string canonicalizedSignedInfo(LIBRALF_NS::Error *_Nullable error) const;

    std::vector<uint8_t> signatureValue(LIBRALF_NS::Error *_Nullable error) const;

    static std::list<LIBRALF_NS::Certificate>
    sortCertificates(std::vector<LIBRALF_NS::Certificate> *_Nonnull unsortedCerts, LIBRALF_NS::Error *_Nullable error);

private:
    xmlDoc *_Nonnull const m_document;

    xmlNode *_Nonnull const m_signedInfoNode;
    xmlNode *_Nonnull const m_signedValueNode;
    xmlNode *_Nonnull const m_keyInfoNode;
};
