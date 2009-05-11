#include "boost/algorithm/string.hpp"
#include "boost/filesystem.hpp"

#include "APESourceSupplier.h"

#include "APEDecoder.h"

APESourceSupplier::APESourceSupplier(void)
{
}

APESourceSupplier::~APESourceSupplier(void)
{
}

void APESourceSupplier::Destroy()
{
    delete this;
}

IAudioSource* APESourceSupplier::CreateAudioSource()
{
    return new APEDecoder();
}

bool APESourceSupplier::CanHandle(const utfchar* source) const
{
    using namespace boost::filesystem;
    using namespace boost::algorithm;

    wpath sourcepath(source);

    if (!is_regular(sourcepath)) 
        return false;

    if (to_lower_copy(extension(sourcepath)) != TEXT(".ape"))
        return false;

    return true;
}
