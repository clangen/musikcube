/***************************************************************************
    copyright            : (C) 2016 by Tsuda Kageyu
    email                : tsuda.kageyu@gmail.com
 ***************************************************************************/

/***************************************************************************
 *   This library is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Lesser General Public License version   *
 *   2.1 as published by the Free Software Foundation.                     *
 *                                                                         *
 *   This library is distributed in the hope that it will be useful, but   *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of            *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU     *
 *   Lesser General Public License for more details.                       *
 *                                                                         *
 *   You should have received a copy of the GNU Lesser General Public      *
 *   License along with this library; if not, write to the Free Software   *
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA         *
 *   02110-1301  USA                                                       *
 *                                                                         *
 *   Alternatively, this file is available under the Mozilla Public        *
 *   License Version 1.1.  You may obtain a copy of the License at         *
 *   http://www.mozilla.org/MPL/                                           *
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#if defined(HAVE_ZLIB)
# include <zlib.h>
#elif defined(HAVE_BOOST_ZLIB)
# include <boost/iostreams/filtering_streambuf.hpp>
# include <boost/iostreams/filter/zlib.hpp>
#endif

#include <tstring.h>
#include <tdebug.h>

#include "tzlib.h"

using namespace TagLib;

bool zlib::isAvailable()
{
#if defined(HAVE_ZLIB) || defined(HAVE_BOOST_ZLIB)

  return true;

#else

  return false;

#endif
}

ByteVector zlib::decompress(const ByteVector &data)
{
#if defined(HAVE_ZLIB)

  z_stream stream = {};

  if(inflateInit(&stream) != Z_OK) {
    debug("zlib::decompress() - Failed to initizlize zlib.");
    return ByteVector();
  }

  ByteVector inData = data;

  stream.avail_in = static_cast<uInt>(inData.size());
  stream.next_in  = reinterpret_cast<Bytef *>(inData.data());

  const unsigned int chunkSize = 1024;

  ByteVector outData;

  do {
    const size_t offset = outData.size();
    outData.resize(outData.size() + chunkSize);

    stream.avail_out = static_cast<uInt>(chunkSize);
    stream.next_out  = reinterpret_cast<Bytef *>(outData.data() + offset);

    const int result = inflate(&stream, Z_NO_FLUSH);

    if(result == Z_STREAM_ERROR ||
       result == Z_NEED_DICT ||
       result == Z_DATA_ERROR ||
       result == Z_MEM_ERROR)
    {
      if(result != Z_STREAM_ERROR)
        inflateEnd(&stream);

      debug("zlib::decompress() - Error reading compressed stream.");
      return ByteVector();
    }

    outData.resize(outData.size() - stream.avail_out);
  } while(stream.avail_out == 0);

  inflateEnd(&stream);

  return outData;

#elif defined(HAVE_BOOST_ZLIB)

  using namespace boost::iostreams;

  struct : public sink
  {
    ByteVector data;

    typedef char     char_type;
    typedef sink_tag category;

    std::streamsize write(char const* s, std::streamsize n)
    {
      const unsigned int originalSize = data.size();

      data.resize(static_cast<unsigned int>(originalSize + n));
      ::memcpy(data.data() + originalSize, s, static_cast<size_t>(n));

      return n;
    }
  } sink;

  try {
    zlib_decompressor().write(sink, data.data(), data.size());
  }
  catch(const zlib_error &) {
    debug("zlib::decompress() - Error reading compressed stream.");
    return ByteVector();
  }

  return sink.data;

#else

  return ByteVector();

#endif
}
