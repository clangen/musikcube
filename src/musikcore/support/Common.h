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

/** @file Common.h
 *  @brief Common filesystem and string utility functions.
 *  @details Provides platform-abstracted access to the user's home, application,
 *      data and plugin directories, plus small helpers for paths, files and
 *      string copies. */

#include <string>
#include <vector>
#include <musikcore/config.h>

/** @namespace musik::core
 *  @brief Core application services: libraries, indexing, playback and utilities. */
namespace musik { namespace core {

    /** @return The user's home directory. */
    std::string GetHomeDirectory();
    /** @return The directory containing the application executable. */
    std::string GetApplicationDirectory();
    /** @return The application data directory.
     *  @param create true to create the directory if missing. */
    std::string GetDataDirectory(bool create = true);
    /** @return A path under the data directory.
     *  @param sFile The file name or relative path. */
    std::string GetPath(const std::string &sFile);
    /** @return The directory where plugins are installed. */
    std::string GetPluginDirectory();
    /** @brief Normalizes a path's separators and trailing slash.
     *  @param path The path to normalize.
     *  @return The normalized path. */
    std::string NormalizeDir(std::string path);
    /** @brief Opens a file with the default OS handler.
     *  @param path The file path to open. */
    void OpenFile(const std::string& path);
    /** @brief Copies a file.
     *  @param from The source path.
     *  @param to The destination path.
     *  @return true on success. */
    bool CopyFile(const std::string& from, const std::string& to);
    /** @brief Computes a checksum over a byte buffer.
     *  @param data The buffer.
     *  @param bytes Buffer length.
     *  @return The checksum. */
    int64_t Checksum(char *data,unsigned int bytes);
    /** @brief Copies a string into a fixed-size buffer (null-terminated).
     *  @param src The source string.
     *  @param dst The destination buffer.
     *  @param size Capacity of dst.
     *  @return The number of bytes copied. */
    size_t CopyString(const std::string& src, char* dst, size_t size);
    /** @brief Reads a file into a byte array.
     *  @param path The file path.
     *  @param target Output buffer (must be freed by the caller).
     *  @param size Output size, in bytes.
     *  @param nullTerminate Whether to add a trailing null byte.
     *  @return true on success. */
    bool FileToByteArray(const std::string& path, char** target, int& size, bool nullTerminate = false);

} }
