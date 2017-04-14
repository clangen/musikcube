/*
     This file is part of libmicrohttpd
     Copyright (C) 2007, 2011, 2017 Christian Grothoff, Karlson2k (Evgeny Grin)

     This library is free software; you can redistribute it and/or
     modify it under the terms of the GNU Lesser General Public
     License as published by the Free Software Foundation; either
     version 2.1 of the License, or (at your option) any later version.

     This library is distributed in the hope that it will be useful,
     but WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
     Lesser General Public License for more details.

     You should have received a copy of the GNU Lesser General Public
     License along with this library; if not, write to the Free Software
     Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

*/
/**
 * @file reason_phrase.c
 * @brief  Tables of the string response phrases
 * @author Elliot Glaysher
 * @author Christian Grothoff (minor code clean up)
 * @author Karlson2k (Evgeny Grin)
 */
#include "platform.h"
#include "microhttpd.h"

#ifndef NULL
#define NULL ((void*)0)
#endif

static const char *const invalid_hundred[] = {
  NULL
};

static const char *const one_hundred[] = {
  "Continue",
  "Switching Protocols",
  "Processing"
};

static const char *const two_hundred[] = {
  "OK",
  "Created",
  "Accepted",
  "Non-Authoritative Information",
  "No Content",
  "Reset Content",
  "Partial Content",
  "Multi-Status",
  "Already Reported",
  "Unknown",
  "Unknown", /* 210 */
  "Unknown",
  "Unknown",
  "Unknown",
  "Unknown",
  "Unknown", /* 215 */
  "Unknown",
  "Unknown",
  "Unknown",
  "Unknown",
  "Unknown", /* 220 */
  "Unknown",
  "Unknown",
  "Unknown",
  "Unknown",
  "Unknown", /* 225 */
  "IM Used"
};

static const char *const three_hundred[] = {
  "Multiple Choices",
  "Moved Permanently",
  "Moved Temporarily",
  "Found",
  "See Other",
  "Not Modified",
  "Use Proxy",
  "Switch Proxy",
  "Temporary Redirect",
  "Permanent Redirect"
};

static const char *const four_hundred[] = {
  "Bad Request",
  "Unauthorized",
  "Payment Required",
  "Forbidden",
  "Not Found",
  "Method Not Allowed",
  "Not Acceptable",
  "Proxy Authentication Required",
  "Request Timeout",
  "Conflict",
  "Gone",
  "Length Required",
  "Precondition Failed",
  "Payload Too Large",
  "URI Too Long",
  "Unsupported Media Type",
  "Range Not Satisfiable",
  "Expectation Failed",
  "Unknown",
  "Unknown",
  "Unknown", /* 420 */
  "Misdirected Request",
  "Unprocessable Entity",
  "Locked",
  "Failed Dependency",
  "Unordered Collection",
  "Upgrade Required",
  "Unknown",
  "Precondition Required",
  "Too Many Requests",
  "Unknown", /* 430 */
  "Request Header Fields Too Large",
  "Unknown",
  "Unknown",
  "Unknown",
  "Unknown", /* 435 */
  "Unknown",
  "Unknown",
  "Unknown",
  "Unknown",
  "Unknown", /* 440 */
  "Unknown",
  "Unknown",
  "Unknown",
  "No Response",
  "Unknown", /* 445 */
  "Unknown",
  "Unknown",
  "Unknown",
  "Retry With",
  "Blocked by Windows Parental Controls", /* 450 */
  "Unavailable For Legal Reasons"
};

static const char *const five_hundred[] = {
  "Internal Server Error",
  "Not Implemented",
  "Bad Gateway",
  "Service Unavailable",
  "Gateway Timeout",
  "HTTP Version Not Supported",
  "Variant Also Negotiates",
  "Insufficient Storage",
  "Loop Detected",
  "Bandwidth Limit Exceeded",
  "Not Extended",
  "Network Authentication Required"
};


struct MHD_Reason_Block
{
  size_t max;
  const char *const*data;
};

#define BLOCK(m) { (sizeof(m) / sizeof(char*)), m }

static const struct MHD_Reason_Block reasons[] = {
  BLOCK (invalid_hundred),
  BLOCK (one_hundred),
  BLOCK (two_hundred),
  BLOCK (three_hundred),
  BLOCK (four_hundred),
  BLOCK (five_hundred),
};


const char *
MHD_get_reason_phrase_for (unsigned int code)
{
  if ( (code >= 100) &&
       (code < 600) &&
       (reasons[code / 100].max > (code % 100)) )
    return reasons[code / 100].data[code % 100];
  return "Unknown";
}
