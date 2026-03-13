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

#include "Logging.h"

#include <cstdarg>
#include <system_error>

void logPrint(LIBRALF_NS::LogPriority priority, const char *tag, const char *file, int line, const char *function,
              const char *format, ...) __attribute__((format(printf, 6, 7)));

void vlogPrint(LIBRALF_NS::LogPriority priority, const char *tag, const char *file, int line, const char *function,
               const char *format, va_list args) __attribute__((format(printf, 6, 0)));

void logSysPrint(LIBRALF_NS::LogPriority priority, const char *tag, const char *file, int line, const char *function,
                 int error, const char *format, ...) __attribute__((format(printf, 7, 8)));

void logSysPrint(LIBRALF_NS::LogPriority priority, const char *tag, const char *file, int line, const char *function,
                 std::error_code error, const char *format, ...) __attribute__((format(printf, 7, 8)));

#if defined(__FILE_NAME__)
#    define LOG_FILE_NAME __FILE_NAME__
#else
#    define LOG_FILE_NAME __FILE__
#endif

#define logDebug(format, args...)                                                                                      \
    logPrint(LIBRALF_NS::LogPriority::Debug, LOG_TAG, LOG_FILE_NAME, __LINE__, __FUNCTION__, format, ##args)

#define logInfo(format, args...)                                                                                       \
    logPrint(LIBRALF_NS::LogPriority::Info, LOG_TAG, LOG_FILE_NAME, __LINE__, __FUNCTION__, format, ##args)

#define logSysInfo(error, format, args...)                                                                             \
    logSysPrint(LIBRALF_NS::LogPriority::Info, LOG_TAG, LOG_FILE_NAME, __LINE__, __FUNCTION__, error, format, ##args)

#define logWarning(format, args...)                                                                                    \
    logPrint(LIBRALF_NS::LogPriority::Warning, LOG_TAG, LOG_FILE_NAME, __LINE__, __FUNCTION__, format, ##args)

#define logSysWarning(error, format, args...)                                                                          \
    logSysPrint(LIBRALF_NS::LogPriority::Warning, LOG_TAG, LOG_FILE_NAME, __LINE__, __FUNCTION__, error, format, ##args)

#define logError(format, args...)                                                                                      \
    logPrint(LIBRALF_NS::LogPriority::Error, LOG_TAG, LOG_FILE_NAME, __LINE__, __FUNCTION__, format, ##args)

#define logSysError(error, format, args...)                                                                            \
    logSysPrint(LIBRALF_NS::LogPriority::Error, LOG_TAG, LOG_FILE_NAME, __LINE__, __FUNCTION__, error, format, ##args)

#define logFatal(format, args...)                                                                                      \
    logPrint(LIBRALF_NS::LogPriority::Fatal, LOG_TAG, LOG_FILE_NAME, __LINE__, __FUNCTION__, format, ##args)

#define logSysFatal(error, format, args...)                                                                            \
    logSysPrint(LIBRALF_NS::LogPriority::Fatal, LOG_TAG, LOG_FILE_NAME, __LINE__, __FUNCTION__, error, format, ##args)
