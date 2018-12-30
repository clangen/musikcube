//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2019 musikcube team
//
// All rights reserved.
//
// Note: this musikcube plugin (supereq) is not distributed under a BSD
// license like most other project components because it consumes GPL2
// code.
//
//////////////////////////////////////////////////////////////////////////////
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
//////////////////////////////////////////////////////////////////////////////

#pragma once

#include <core/sdk/IDSP.h>
#include "supereq/Equ.h"

using namespace musik::core::sdk;

class SuperEqDsp : public IDSP {
    public:
        SuperEqDsp();
        ~SuperEqDsp();

        virtual void Release() override;
        virtual bool Process(IBuffer *buffer) override;

        static void NotifyChanged();

    private:
        SuperEqState* supereq {nullptr};
        int lastUpdated {0};
        bool enabled;
};
