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

#include "LibRalf.h"

#include <cstddef>
#include <cstdint>
#include <functional>

namespace LIBRALF_NS
{

    enum class LogPriority : uint32_t
    {
        Debug = 0,
        Info,
        Warning,
        Error,
        Fatal
    };

    class LogContext
    {
    public:
        LogContext(const LogContext &) = delete;
        LogContext &operator=(const LogContext &) = delete;

        constexpr LogContext() noexcept
            : tag(nullptr)
            , file(nullptr)
            , line(0)
            , function(nullptr)
        {
        }

        constexpr LogContext(const char *tag_, const char *file_, int line_, const char *function_) noexcept
            : tag(tag_)
            , file(file_)
            , line(line_)
            , function(function_)
        {
        }

        const char *const tag;
        const char *const file;
        const int line;
        const char *const function;
    };

    // -------------------------------------------------------------------------
    /*!
        Sets either the global or tag specific log level.  The log level
        determines the minimum log level to report, for example setting to
        LogPriority::Warning then info and debug log messages will be ignored.

        If \a tag is not nullptr then the log level applies only to log messages
        with the given tag, otherwise the log level applies globally, overriding
        any previously set tag specific log levels.

     */
    LIBRALF_EXPORT void setLogLevel(const char *tag, LogPriority minLogPriority);

    using LogHandler =
        std::function<void(LogPriority priority, const LogContext &context, const char *message, size_t messageLen)>;

    // -------------------------------------------------------------------------
    /*!
        This is the default log handler, it prints all outputs to either
        stdout or stderr based on the log priority.

     */
    LIBRALF_EXPORT extern LogHandler stdoutLogHandler;

#if defined(EMSCRIPTEN)
    // -------------------------------------------------------------------------
    /*!
        This is the default log handler if building for WASM, it prints all
        outputs to either to the emscriptin log handler functions.  These output
        messages in the browser console.

     */
    extern LogHandler emscriptenLogHandler;
#endif

    // -------------------------------------------------------------------------
    /*!
        Sets the log handler to use.  You can set to one of the in-built log
        handlers (stdoutLogHandler, syslogHandler, etc) or your own custom
        function.

        Setting the \a handler to nullptr will clear the log handler and no
        log messages will be processed.

     */
    LIBRALF_EXPORT void setLogHandler(LogHandler handler);

} // namespace LIBRALF_NS
