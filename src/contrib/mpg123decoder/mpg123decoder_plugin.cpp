#include "stdafx.h"
#include <core/sdk/IPlugin.h>
#include "Mpg123DecoderFactory.h"

#ifdef WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

#ifdef WIN32
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    return true;
}
#endif

class Mpg123Plugin : public musik::core::IPlugin {
    virtual void Destroy() { delete this; };
    virtual const char* Name() { return "mpg123 decoder"; };
    virtual const char* Version() { return "0.2"; };
    virtual const char* Author() { return "Daniel �nnerby, _avatar"; };
};

extern "C" DLLEXPORT musik::core::IPlugin* GetPlugin() {
    return new Mpg123Plugin();
}

extern "C" DLLEXPORT musik::core::audio::IDecoderFactory* GetDecoderFactory() {
    return new Mpg123DecoderFactory();
}
