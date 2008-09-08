//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright © 2008, mC2 team
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
#include "pch.hpp"
#include <core/Crypt.h>
#include <core/config_format.h>

#include <md5/md5.h>
#include <boost/random.hpp>

//////////////////////////////////////////////////////////////////////////////

using namespace musik::core;

//////////////////////////////////////////////////////////////////////////////

std::string Crypt::GenerateSalt(){
    const std::string characters("ABCDEFGHKLMNOPQRSTWXYZabcdefghjkmnpqrstwxyz123456789");

    typedef boost::mt19937 RandomGenerator;
    RandomGenerator randomGenerator;


    boost::uniform_int<> randDistribution(0,(int)characters.size()-1);
    boost::variate_generator<RandomGenerator&, boost::uniform_int<> > rand(randomGenerator, randDistribution);

    std::string salt;
    for(int i(0);i<64;++i){
        salt    += characters.at(rand());
    }
    return salt;
}

std::string Crypt::StaticSalt(){
    return "mC2, don't be square";
}

std::string Crypt::Encrypt(std::string cryptContent,std::string salt){
    md5_state_t md5State;
    md5_init(&md5State);

    std::string encryptString   = cryptContent+salt;

    md5_append(&md5State,(const md5_byte_t *)encryptString.c_str(),(int)encryptString.size());

    // Encrypted result
    md5_byte_t result[16];

    // Get the results
    md5_finish(&md5State, result);

    // Add result to encryptedString
    std::string encryptedHexString;
    boost::format formatHex("%1$02x");
    for(int i=0;i<16;++i){
        int hex     = result[i];
        formatHex%hex;
        encryptedHexString += formatHex.str();
        formatHex.clear();
    }
    return encryptedHexString;
}
