# Building musikcube on Windows

Install Git and Visual Studio 2022 with the Desktop development with C++ workload, including the x64 compiler and a Windows SDK. Keep the checkout path free of spaces for the FFmpeg build. The supported Visual Studio configurations are **Debug x64** and **Release x64**.

Clone with submodules so the Asio sources are available. From the checkout root, run the vendor build once before building the solution.

```powershell
git clone --recurse-submodules https://github.com/clangen/musikcube.git
Set-Location musikcube
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\script\build-vendor-libraries.ps1
```

For an existing checkout, run `git submodule update --init --recursive` before building. The vendor script downloads pinned sources and portable build tools, so the first run needs internet access. It uses MSVC to compile the libraries and a repo-local MSYS2 shell for FFmpeg's build. It installs no system-wide tools. Sources, caches, headers, libraries, and runtime DLLs are stored under the ignored `vendor\` directory.

Open `musikcube.sln`, select **x64**, and build either configuration. Or use an x64 Developer PowerShell for Visual Studio.

```powershell
msbuild .\musikcube.sln /t:Build /p:Configuration=Release /p:Platform=x64
msbuild .\musikcube.sln /t:Build /p:Configuration=Debug /p:Platform=x64
```

Run `bin64\Release\musikcube.exe` or `bin64\Debug\musikcube.exe`. The projects copy the vendor DLLs beside the executable during the build. Both configurations use the same shared dependency DLLs. The static libraries used by plugins are built for the corresponding Release or Debug CRT.

After changing dependency versions or build flags, rerun the vendor script and rebuild into a clean output directory so obsolete DLLs from a previous build cannot remain beside the executable. The vcpkg version is pinned in `script/build-vendor-libraries.ps1`. FFmpeg's codec and format options are in `script/windows-vendor/ffmpeg-options.txt`.
