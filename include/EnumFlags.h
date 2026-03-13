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

// -----------------------------------------------------------------------------
/*!
    \macro LIBRALF_ENUM_FLAGS
    \brief Defines some convenient bit operators for an enum type so it can be
    treated like flags.

    You use it like this:
    \code
        enum class Option : unsigned
        {
            None = 0,
            First = 1
            Second = 2
         };
         LIBRALF_ENUM_FLAGS(Options, Option)
         ...
    \endcode

     If the enum is part of a class you can do the following
    \code
        class Foo
        {
        public:
            enum class Option : unsigned
            {
                None = 0,
                First = 1
                Second = 2
            };
            LIBRALF_DECLARE_ENUM_FLAGS(Options, Option)
            ...
        };

        LIBRALF_DECLARE_ENUM_FLAGS_OPERATORS(Foo::Options)
    \endcode

    Then you can use type Options to | or & flags into the type without having
    to use static_cast<unsigned>(...) everywhere.

    The functions the macro generates are not rocket science and essentially
    just wrappers around static_cast.  The idea for the macro came for looking
    at what the STL library does for bit flags (ie. std::filesystem::perms).
    It also takes some ideas from QFlags template class in Qt.

 */

// NOLINTBEGIN(bugprone-macro-parentheses)

#define LIBRALF_DECLARE_ENUM_FLAGS(Flags, Enum) using Flags = Enum;

#define LIBRALF_DECLARE_ENUM_FLAGS_OPERATORS(Flags)                                                                    \
    inline constexpr Flags operator&(Flags lhs, Flags rhs)                                                             \
    {                                                                                                                  \
        return static_cast<Flags>(static_cast<unsigned>(lhs) & static_cast<unsigned>(rhs));                            \
    }                                                                                                                  \
    inline constexpr Flags operator|(Flags lhs, Flags rhs)                                                             \
    {                                                                                                                  \
        return static_cast<Flags>(static_cast<unsigned>(lhs) | static_cast<unsigned>(rhs));                            \
    }                                                                                                                  \
    inline constexpr Flags operator^(Flags lhs, Flags rhs)                                                             \
    {                                                                                                                  \
        return static_cast<Flags>(static_cast<unsigned>(lhs) ^ static_cast<unsigned>(rhs));                            \
    }                                                                                                                  \
    inline constexpr Flags operator~(Flags lhs)                                                                        \
    {                                                                                                                  \
        return static_cast<Flags>(~static_cast<unsigned>(lhs));                                                        \
    }                                                                                                                  \
    inline Flags &operator&=(Flags &lhs, Flags rhs)                                                                    \
    {                                                                                                                  \
        return lhs = lhs & rhs;                                                                                        \
    }                                                                                                                  \
    inline Flags &operator|=(Flags &lhs, Flags rhs)                                                                    \
    {                                                                                                                  \
        return lhs = lhs | rhs;                                                                                        \
    }                                                                                                                  \
    inline Flags &operator^=(Flags &lhs, Flags rhs)                                                                    \
    {                                                                                                                  \
        return lhs = lhs ^ rhs;                                                                                        \
    }

#define LIBRALF_ENUM_FLAGS(Flags, Enum)                                                                                \
    LIBRALF_DECLARE_ENUM_FLAGS(Flags, Enum)                                                                            \
    LIBRALF_DECLARE_ENUM_FLAGS_OPERATORS(Flags)

// NOLINTEND(bugprone-macro-parentheses)