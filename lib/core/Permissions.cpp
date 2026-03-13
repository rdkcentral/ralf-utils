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

#include "IPermissionsImpl.h"
#include "PackageMetaData.h"

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

Permissions::Permissions(std::shared_ptr<IPermissionsImpl> impl)
    : m_impl(std::move(impl))
{
}

bool Permissions::get(std::string_view privilege) const
{
    if (m_impl)
        return m_impl->get(privilege);
    else
        return false;
}

bool Permissions::operator[](std::string_view privilege) const
{
    if (m_impl)
        return m_impl->get(privilege);
    else
        return false;
}

std::set<std::string> Permissions::all() const
{
    if (m_impl)
        return m_impl->all();
    else
        return {};
}
