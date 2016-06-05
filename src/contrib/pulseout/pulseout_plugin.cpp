#include <core/sdk/IPlugin.h>
#include <core/sdk/IOutput.h>
#include "PulseOut.h"

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

class CoreAudioOutPlugin : public musik::core::IPlugin {
    void Destroy() { delete this; };
    const char* Name() { return "PulseAudio IOutput"; };
    const char* Version() { return "0.1"; };
    const char* Author() { return "clangen"; };
};

extern "C" DLLEXPORT musik::core::IPlugin* GetPlugin() {
    return new CoreAudioOutPlugin;
}

extern "C" DLLEXPORT musik::core::audio::IOutput* GetAudioOutput() {
    return new PulseOut();
}
