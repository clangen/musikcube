mC2 is a heavy user of C++ and plugins can be a bit tricky in C++ since the only thing you can export in a application is C functions. Instead mC2 relies on the fact that VTABLES are compiled in an identical manner between different compilers (not true in all cases). So every class that needs to passed between the application and a plugin must be a pure virtual class and -to be on the safe side- we require that memory allocated in a plugin must also be deallocated in the same plugin.

## Exporting basic plugin information ##
Plugins must be built into a DLL and must implement the IPlugin class.
The IPlugin class is only a way for musikCube to get a hold of basic information about the plugin itself and does not implement any of the features the plugin contains.
Like this:
```
class MyPlugin : public musik::core::IPlugin {
    void Destroy() { delete this; };

    const wchar_t* Name(){
        return _T("My own plugin");
    };

    const wchar_t* Version(){
        return _T("0.1");
    };

    const wchar_t* Author(){
        return _T("My name");
    };

};
```
The plugin will also need to export a function for the mC2 to be able to get a hold of the interface:
```
__declspec(dllexport) musik::core::IPlugin* GetPlugin(){
    return new MyPlugin();
}
```

---

## Exporting plugin interfaces ##
There are several different types of plugins, but they all share a common way of implementation. Basically, every plugintype has a interface class (pure virtual classes with prefix "I") that the plugin extends. The plugin is also required to export a function that will return a new instance of the extended interface.

Looking at the MetaDataReader (plugins that reads tags from audiofiles) as an example.
The MetaDataReader plugins has an interface in src/core/Plugin/IMetaDataReader.h looking like this:
```
class IMetaDataReader{
    public:
        virtual bool ReadTag(musik::core::ITrack *track)=0;
        virtual bool CanReadTag(const utfchar *extension)=0;
        virtual void Destroy()=0;
};
```

A MetaDataReader plugin will have to extend this interface, implement each required method (in this case the ReadTag, CanReadTag and Destroy) and finally export a method that will return a new instance of a MetaDataReader.
In the MetaDataReader case, the exported method will need to be called "GetMetaDataReader", looking like this:
```
__declspec(dllexport) musik::core::Plugin::IMetaDataReader* GetMetaDataReader(){
    return new MyClassExtendingIMetaDataReader();
}
```