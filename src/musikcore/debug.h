//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2023 musikcube team
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of the author nor the names of other contributors may
//      be used to endorse or promote products derived from this software
//      without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//////////////////////////////////////////////////////////////////////////////

#pragma once

/** @file debug.h
 *  @brief Application logging framework.
 *  @details Provides a leveled logging facility (verbose/info/warning/error) with
 *      pluggable backends. Logs can be routed to a file, the console, or any
 *      custom IBackend implementation. */

#include <string>
#include <vector>
#include <fstream>
#include <memory>
#include "utfutil.h"

/** @namespace musik
 *  @brief The application namespace. */
namespace musik {
    /** @brief Application logging framework.
     *  @details Static methods write leveled log entries to every registered
     *      backend. Backends are installed with Start() and shut down with
     *      Shutdown(). */
    class debug {
        public:
            /** @brief Interface for a log output backend. */
            class IBackend {
                public:
                    virtual ~IBackend() { }
                    /** @brief Writes a verbose-level entry.
                     *  @param tag The logging tag.
                     *  @param string The log text. */
                    virtual void verbose(const std::string& tag, const std::string& string) = 0;
                    /** @brief Writes an info-level entry.
                     *  @param tag The logging tag.
                     *  @param string The log text. */
                    virtual void info(const std::string& tag, const std::string& string) = 0;
                    /** @brief Writes a warning-level entry.
                     *  @param tag The logging tag.
                     *  @param string The log text. */
                    virtual void warning(const std::string& tag, const std::string& string) = 0;
                    /** @brief Writes an error-level entry.
                     *  @param tag The logging tag.
                     *  @param string The log text. */
                    virtual void error(const std::string& tag, const std::string& string) = 0;
            };

            /** @brief Backend that writes logs to a file. */
            class FileBackend : public IBackend {
                public:
                    /** @brief Creates a backend writing to the given file.
                     *  @param fn The log file path. */
                    FileBackend(const std::string& fn);
                    /** @brief Move constructor.
                     *  @param fn The backend to move. */
                    FileBackend(FileBackend&& fn);
                    virtual ~FileBackend() override;
                    /** @brief Writes a verbose-level entry.
                     *  @param tag The logging tag.
                     *  @param string The log text. */
                    virtual void verbose(const std::string& tag, const std::string& string) override;
                    /** @brief Writes an info-level entry.
                     *  @param tag The logging tag.
                     *  @param string The log text. */
                    virtual void info(const std::string& tag, const std::string& string) override;
                    /** @brief Writes a warning-level entry.
                     *  @param tag The logging tag.
                     *  @param string The log text. */
                    virtual void warning(const std::string& tag, const std::string& string) override;
                    /** @brief Writes an error-level entry.
                     *  @param tag The logging tag.
                     *  @param string The log text. */
                    virtual void error(const std::string& tag, const std::string& string) override;
                private:
                    std::ofstream out; /**< Log file stream. */
            };

            /** @brief Backend writing to the default log file location. */
            class SimpleFileBackend: public FileBackend {
                public:
                    /** @brief Creates the default file backend. */
                    SimpleFileBackend();
                    SimpleFileBackend(const std::string& fn) = delete;
                    SimpleFileBackend(FileBackend&& fn) = delete;
            };

            /** @brief Backend that writes logs to the console. */
            class ConsoleBackend : public IBackend {
                public:
                    /** @brief Creates a console backend. */
                    ConsoleBackend();
                    virtual ~ConsoleBackend() override;
                    /** @brief Writes a verbose-level entry.
                     *  @param tag The logging tag.
                     *  @param string The log text. */
                    virtual void verbose(const std::string& tag, const std::string& string) override;
                    /** @brief Writes an info-level entry.
                     *  @param tag The logging tag.
                     *  @param string The log text. */
                    virtual void info(const std::string& tag, const std::string& string) override;
                    /** @brief Writes a warning-level entry.
                     *  @param tag The logging tag.
                     *  @param string The log text. */
                    virtual void warning(const std::string& tag, const std::string& string) override;
                    /** @brief Writes an error-level entry.
                     *  @param tag The logging tag.
                     *  @param string The log text. */
                    virtual void error(const std::string& tag, const std::string& string) override;
            };

            /** @brief Installs the logging backends.
             *  @param backends The backends to use (defaults to a simple file backend). */
            static void Start(std::vector<IBackend*> backends = { new SimpleFileBackend() });
            /** @brief Shuts down and releases all backends. */
            static void Shutdown();

            /** @brief Writes a verbose-level entry.
             *  @param tag The logging tag.
             *  @param string The log text. */
            static void verbose(const std::string& tag, const std::string& string) noexcept;
            /** @brief Short alias for verbose().
             *  @param tag The logging tag.
             *  @param string The log text. */
            static void v(const std::string& tag, const std::string& string) noexcept;
            /** @brief Writes an info-level entry.
             *  @param tag The logging tag.
             *  @param string The log text. */
            static void info(const std::string& tag, const std::string& string) noexcept;
            /** @brief Short alias for info().
             *  @param tag The logging tag.
             *  @param string The log text. */
            static void i(const std::string& tag, const std::string& string) noexcept;
            /** @brief Writes a warning-level entry.
             *  @param tag The logging tag.
             *  @param string The log text. */
            static void warning(const std::string& tag, const std::string& string) noexcept;
            /** @brief Short alias for warning().
             *  @param tag The logging tag.
             *  @param string The log text. */
            static void w(const std::string& tag, const std::string& string) noexcept;
            /** @brief Writes an error-level entry.
             *  @param tag The logging tag.
             *  @param string The log text. */
            static void error(const std::string& tag, const std::string& string) noexcept;
            /** @brief Short alias for error().
             *  @param tag The logging tag.
             *  @param string The log text. */
            static void e(const std::string& tag, const std::string& string) noexcept;
    };
}
