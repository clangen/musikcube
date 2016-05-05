/***************************************************************************
    copyright            : (C) 2002 - 2008 by Scott Wheeler
    email                : wheeler@kde.org
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

#include <tbytevector.h>

#include "aiffproperties.h"
#include "apeproperties.h"
#include "asfproperties.h"
#include "flacproperties.h"
#include "mp4properties.h"
#include "mpcproperties.h"
#include "mpegproperties.h"
#include "opusproperties.h"
#include "speexproperties.h"
#include "trueaudioproperties.h"
#include "vorbisproperties.h"
#include "wavproperties.h"
#include "wavpackproperties.h"

#include "audioproperties.h"

using namespace TagLib;

class AudioProperties::AudioPropertiesPrivate
{

};

////////////////////////////////////////////////////////////////////////////////
// public methods
////////////////////////////////////////////////////////////////////////////////

AudioProperties::~AudioProperties()
{

}

int AudioProperties::lengthInSeconds() const
{
  // This is an ugly workaround but we can't add a virtual function.
  // Should be virtual in taglib2.

  if(dynamic_cast<const APE::Properties*>(this))
    return dynamic_cast<const APE::Properties*>(this)->lengthInSeconds();

  else if(dynamic_cast<const ASF::Properties*>(this))
    return dynamic_cast<const ASF::Properties*>(this)->lengthInSeconds();

  else if(dynamic_cast<const FLAC::Properties*>(this))
    return dynamic_cast<const FLAC::Properties*>(this)->lengthInSeconds();

  else if(dynamic_cast<const MP4::Properties*>(this))
    return dynamic_cast<const MP4::Properties*>(this)->lengthInSeconds();

  else if(dynamic_cast<const MPC::Properties*>(this))
    return dynamic_cast<const MPC::Properties*>(this)->lengthInSeconds();

  else if(dynamic_cast<const MPEG::Properties*>(this))
    return dynamic_cast<const MPEG::Properties*>(this)->lengthInSeconds();

  else if(dynamic_cast<const Ogg::Opus::Properties*>(this))
    return dynamic_cast<const Ogg::Opus::Properties*>(this)->lengthInSeconds();

  else if(dynamic_cast<const Ogg::Speex::Properties*>(this))
    return dynamic_cast<const Ogg::Speex::Properties*>(this)->lengthInSeconds();

  else if(dynamic_cast<const TrueAudio::Properties*>(this))
    return dynamic_cast<const TrueAudio::Properties*>(this)->lengthInSeconds();

  else if (dynamic_cast<const RIFF::AIFF::Properties*>(this))
    return dynamic_cast<const RIFF::AIFF::Properties*>(this)->lengthInSeconds();

  else if(dynamic_cast<const RIFF::WAV::Properties*>(this))
    return dynamic_cast<const RIFF::WAV::Properties*>(this)->lengthInSeconds();

  else if(dynamic_cast<const Vorbis::Properties*>(this))
    return dynamic_cast<const Vorbis::Properties*>(this)->lengthInSeconds();

  else if(dynamic_cast<const WavPack::Properties*>(this))
    return dynamic_cast<const WavPack::Properties*>(this)->lengthInSeconds();

  else
    return 0;
}

int AudioProperties::lengthInMilliseconds() const
{
  // This is an ugly workaround but we can't add a virtual function.
  // Should be virtual in taglib2.

  if(dynamic_cast<const APE::Properties*>(this))
    return dynamic_cast<const APE::Properties*>(this)->lengthInMilliseconds();

  else if(dynamic_cast<const ASF::Properties*>(this))
    return dynamic_cast<const ASF::Properties*>(this)->lengthInMilliseconds();

  else if(dynamic_cast<const FLAC::Properties*>(this))
    return dynamic_cast<const FLAC::Properties*>(this)->lengthInMilliseconds();

  else if(dynamic_cast<const MP4::Properties*>(this))
    return dynamic_cast<const MP4::Properties*>(this)->lengthInMilliseconds();

  else if(dynamic_cast<const MPC::Properties*>(this))
    return dynamic_cast<const MPC::Properties*>(this)->lengthInMilliseconds();

  else if(dynamic_cast<const MPEG::Properties*>(this))
    return dynamic_cast<const MPEG::Properties*>(this)->lengthInMilliseconds();

  else if(dynamic_cast<const Ogg::Opus::Properties*>(this))
    return dynamic_cast<const Ogg::Opus::Properties*>(this)->lengthInMilliseconds();

  else if(dynamic_cast<const Ogg::Speex::Properties*>(this))
    return dynamic_cast<const Ogg::Speex::Properties*>(this)->lengthInMilliseconds();

  else if(dynamic_cast<const TrueAudio::Properties*>(this))
    return dynamic_cast<const TrueAudio::Properties*>(this)->lengthInMilliseconds();

  else if(dynamic_cast<const RIFF::AIFF::Properties*>(this))
    return dynamic_cast<const RIFF::AIFF::Properties*>(this)->lengthInMilliseconds();

  else if(dynamic_cast<const RIFF::WAV::Properties*>(this))
    return dynamic_cast<const RIFF::WAV::Properties*>(this)->lengthInMilliseconds();

  else if(dynamic_cast<const Vorbis::Properties*>(this))
    return dynamic_cast<const Vorbis::Properties*>(this)->lengthInMilliseconds();

  else if(dynamic_cast<const WavPack::Properties*>(this))
    return dynamic_cast<const WavPack::Properties*>(this)->lengthInMilliseconds();

  else
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
// protected methods
////////////////////////////////////////////////////////////////////////////////

AudioProperties::AudioProperties(ReadStyle) :
  d(0)
{

}
