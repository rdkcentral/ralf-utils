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

#include "Error.h"

#include <optional>

namespace LIBRALF_NS
{

    // -----------------------------------------------------------------------------
    /*!
        \class Result
        \brief Object that wraps a return value or an error.

        It's motivated by the std::optional class, but allows for an error to be
        set if the value is not valid.  This allows for a single return type to be
        used for functions that can return a value or an error.

        The inspiration is the rust Result type.

        You can think of this as a union of std::optional<T> and Error, where the
        Error is only valid if the std::optional<T> is not valid.  Like with
        std::optional<T>, if the object is not valid (contains an error) and you
        try and deference it, then the program will crash.  You should always
        check the object is valid before dereferencing it.

        In terms of usage, is it similar API to std::optional<T>, but with some
        extra methods to check for and return the error.

     */
    template <class T = void>
    class Result
    {
    public:
        Result() = default;
        Result(const Result &) = default;
        Result(Result &&) noexcept = default;

        constexpr Result(const Error &error) noexcept // NOLINT(google-explicit-constructor)
            : m_value(std::nullopt)
            , m_error(error)
        {
        }

        constexpr Result(Error &&error) noexcept // NOLINT(google-explicit-constructor)
            : m_value(std::nullopt)
            , m_error(std::move(error))
        {
        }

        template <class... Args>
        constexpr explicit Result(std::in_place_t, Args &&...args)
            : m_value(std::in_place, std::forward<Args>(args)...)
        {
        }

        template <class U, class... Args>
        constexpr explicit Result(std::in_place_t, std::initializer_list<U> ilist, Args &&...args)
            : m_value(std::in_place, ilist, std::forward<Args>(args)...)
        {
        }

        template <class U = std::remove_cv_t<T>>
        constexpr Result(U &&value) // NOLINT(google-explicit-constructor, bugprone-forwarding-reference-overload)
            : m_value(std::forward<U>(value))
        {
        }

        Result &operator=(const Result &) = default;
        Result &operator=(Result &&) noexcept = default;

        ~Result() = default;

        constexpr explicit operator bool() const noexcept { return m_value.operator bool(); }

        // NOLINTNEXTLINE(readability-identifier-naming)
        constexpr bool has_value() const noexcept { return m_value.has_value(); }

        constexpr bool hasValue() const noexcept { return m_value.has_value(); }

        constexpr const T *operator->() const noexcept { return m_value.operator->(); }

        constexpr T *operator->() noexcept { return m_value.operator->(); }

        // clang-format off

        constexpr const T &operator*() const & noexcept { return m_value.operator->(); }

        constexpr T &operator*() & noexcept { return m_value.operator->(); }

        constexpr const T &&operator*() const && noexcept { return m_value.operator->(); }

        constexpr T &&operator*() && noexcept { return m_value.operator->(); }

        // clang-format on

        constexpr T &value() & { return m_value.value(); }

        constexpr const T &value() const & { return m_value.value(); }

        constexpr T &&value() && { return m_value.value(); }

        constexpr const T &&value() const && { return m_value.value(); }

        template <class U = std::remove_cv_t<T>>
        constexpr T value_or(U &&default_value) const & // NOLINT(readability-identifier-naming)
        {
            return m_value.value_or(default_value);
        }

        template <class U = std::remove_cv_t<T>>
        constexpr T value_or(U &&default_value) && // NOLINT(readability-identifier-naming)
        {
            return m_value.value_or(default_value);
        }

        template <class... Args>
        T &emplace(Args &&...args)
        {
            m_value.emplace(std::forward<Args>(args)...);
            m_error.clear();
            return *m_value;
        }

        template <class U, class... Args>
        T &emplace(std::initializer_list<U> ilist, Args &&...args)
        {
            m_value.emplace(ilist, std::forward<Args>(args)...);
            m_error.clear();
            return *m_value;
        }

        // NOLINTNEXTLINE(readability-identifier-naming)
        constexpr bool is_error() const noexcept { return m_value.has_value() == false; }

        constexpr bool isError() const noexcept { return m_value.has_value() == false; }

        constexpr const Error &error() const noexcept { return m_error; }

    private:
        std::optional<T> m_value;
        Error m_error;
    };

    template <>
    class Result<void>
    {
    public:
        Result() = default;
        Result(const Result &) = default;
        Result(Result &&) noexcept = default;

        Result(const Error &error) noexcept // NOLINT(google-explicit-constructor)
            : m_error(error)
        {
        }

        Result(Error &&error) noexcept // NOLINT(google-explicit-constructor)
            : m_error(std::move(error))
        {
        }

        Result &operator=(const Result &) = default;
        Result &operator=(Result &&) noexcept = default;

        ~Result() = default;

        explicit operator bool() const noexcept { return m_error.code() == ErrorCode::NoError; }

        // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
        constexpr bool has_value() const noexcept { return false; }

        // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
        constexpr bool hasValue() const noexcept { return false; }

        // NOLINTNEXTLINE(readability-identifier-naming)
        bool is_error() const noexcept { return m_error.code() != ErrorCode::NoError; }

        bool isError() const noexcept { return m_error.code() != ErrorCode::NoError; }

        constexpr const Error &error() const noexcept { return m_error; }

    private:
        Error m_error;
    };

    template <class T, class U = std::remove_reference_t<T>>
    inline Result<U> Ok(T &&value)
    {
        return { std::forward<T>(value) };
    }

    inline Result<void> Ok()
    {
        return {};
    }

    template <class T, class U>
    constexpr bool operator==(const Result<T> &lhs, const Result<U> &rhs)
    {
        if (lhs.isError() && rhs.isError())
            return lhs.error() == rhs.error();

        if (lhs.isError() || rhs.isError())
            return false;

        return lhs.value() == rhs.value();
    }

    template <class T, class U>
    constexpr bool operator!=(const Result<T> &lhs, const Result<U> &rhs)
    {
        if (lhs.isError() && rhs.isError())
            return lhs.error() != rhs.error();

        if (lhs.isError() || rhs.isError())
            return true;

        return lhs.value() != rhs.value();
    }

    template <class T, class U>
    constexpr bool operator==(const Result<T> &opt, const U &value)
    {
        if (opt.isError())
            return false;

        return opt.value() == value;
    }

    template <class U, class T>
    constexpr bool operator==(const U &value, const Result<T> &opt)
    {
        if (opt.isError())
            return false;

        return opt.value() == value;
    }

    template <class T, class U>
    constexpr bool operator!=(const Result<T> &opt, const U &value)
    {
        if (opt.isError())
            return true;

        return opt.value() != value;
    }

    template <class U, class T>
    constexpr bool operator!=(const U &value, const Result<T> &opt)
    {
        if (opt.isError())
            return true;

        return opt.value() != value;
    }

} // namespace LIBRALF_NS