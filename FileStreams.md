## _Draft_ ##

# Introduction #

Filestreams can be complicated. When writing mC2 we thought about how to make a filestream as flexible as possible so that we could extend them to read from local files as well as from http streams. The result was a simple and very extendible interface that can be extended to read from anything like http stream, P2P streams or archives (RAR, ZIP).


# How to create a new filestream type #

The interface is very simple. All you need to do to make a new type of filestream is basically to extend the [IFileStream](http://code.google.com/p/musikcube/source/browse/trunk/src/core/filestreams/IFileStream.h) interface.
The IFileStream contains the basic file IO operations like Open, Close, Read, SetPosition, etc and needs to implement most of them.

Writing a plugin that extends filestreams also requires you to make a interface that can create the new IFileStream. This is done by extending the [IFileStreamFactory](http://code.google.com/p/musikcube/source/browse/trunk/src/core/filestreams/IFileStreamFactory.h) that only contains 2 methods:
  * `bool CanReadFile(const utfchar *filename)`: Should return if this kind of stream is able to read the specified filename. For instance if you are writing a httpstream plugin, this method should check if the filename is starting with `http://`
  * `IFileStream* OpenFile(const utfchar *filename)`: Create a new filestream from the filename specified.

# How to create a filestream #

This is the simple part.
```
#include <core/filestreams/Factory.h>

using namespace musik::core;
...

filestreams::FileStreamPtr myFile = filestreams::Factory::OpenFile(UTF("http://www.musikcube.com/my_song.mp3"));

if(myFile){
    // Start using the file
}
```