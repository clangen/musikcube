//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright © 2008, Daniel Önnerby
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

#define XMD_H
#include <boost/gil/image.hpp>
#include <boost/gil/typedefs.hpp>
#include <boost/gil/extension/io/jpeg_io.hpp>
#include <boost/gil/extension/numeric/sampler.hpp>
#include <boost/gil/extension/numeric/resample.hpp>

#include <boost/shared_ptr.hpp>

#include <core/config.h>
#include <core/Query/ThumbnailInfo.h>
#include <core/Library/Base.h>

#include <sigslot/sigslot.h>


//////////////////////////////////////////////////////////////////////////////

namespace musik{ namespace core{

//////////////////////////////////////////////////////////////////////////////

class Thumbnail : public sigslot::has_slots<> {

public:
    Thumbnail();
    ~Thumbnail();

    typedef boost::shared_ptr<boost::gil::rgba8_image_t> ImagePtr;

    void Load(int trackId,musik::core::LibraryPtr library);
    ImagePtr GetImage(int width,int height);

    sigslot::signal1<bool> ImageLoaded;



private:
    void OnThumbnailInfo(utfstring filename);
    void ClearImage();


    boost::gil::rgb8_image_t *image;

    int trackId;
    int maxWidth;
    int maxHeight;
    musik::core::Query::ThumbnailInfo infoQuery;
};

//////////////////////////////////////////////////////////////////////////////
} } // musik::core
//////////////////////////////////////////////////////////////////////////////

