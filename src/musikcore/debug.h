//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2019 musikcube team
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

#include <string>
#include <vector>
#include <fstream>
#include <memory>

namespace musik {
    class debug {
        public:
            class IBackend {
                public:
                    virtual ~IBackend() { }
                    virtual void verbose(const std::string& tag, const std::string& string) = 0;
                    virtual void info(const std::string& tag, const std::string& string) = 0;
                    virtual void warning(const std::string& tag, const std::string& string) = 0;
                    virtual void error(const std::string& tag, const std::string& string) = 0;
            };

            class FileBackend : public IBackend {
                public:
                    FileBackend(const std::string& fn);
                    FileBackend(FileBackend&& fn);
                    virtual ~FileBackend() override;
                    virtual void verbose(const std::string& tag, const std::string& string) override;
                    virtual void info(const std::string& tag, const std::string& string) override;
                    virtual void warning(const std::string& tag, const std::string& string) override;
                    virtual void error(const std::string& tag, const std::string& string) override;
                private:
                    std::ofstream out;
            };

            class SimpleFileBackend: public FileBackend {
                public:
                    SimpleFileBackend();
                    SimpleFileBackend(const std::string& fn) = delete;
                    SimpleFileBackend(FileBackend&& fn) = delete;
            };

            class ConsoleBackend : public IBackend {
                public:
                    ConsoleBackend();
                    virtual ~ConsoleBackend() override;
                    virtual void verbose(const std::string& tag, const std::string& string) override;
                    virtual void info(const std::string& tag, const std::string& string) override;
                    virtual void warning(const std::string& tag, const std::string& string) override;
                    virtual void error(const std::string& tag, const std::string& string) override;
            };

            static void Start(std::vector<IBackend*> backends = { new SimpleFileBackend() });
            static void Stop();

            static void verbose(const std::string& tag, const std::string& string);
            static void v(const std::string& tag, const std::string& string);
            static void info(const std::string& tag, const std::string& string);
            static void i(const std::string& tag, const std::string& string);
            static void warning(const std::string& tag, const std::string& string);
            static void w(const std::string& tag, const std::string& string);
            static void error(const std::string& tag, const std::string& string);
            static void e(const std::string& tag, const std::string& string);
    };
}
