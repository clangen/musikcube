# Building musikcube on Windows

Install Git, Node.js, and Visual Studio 2026 with the Desktop development with C++ workload, including the x64 compiler and a Windows SDK. Keep the checkout path free of spaces for the FFmpeg build. The supported Visual Studio configurations are **Debug x64** and **Release x64**.

Clone with submodules so the Asio sources are available. From the checkout root, run the vendor build once before building the solution.

```powershell
git clone --recurse-submodules https://github.com/clangen/musikcube.git
Set-Location musikcube
node .\script\build-vendor-libraries.js
```

For an existing checkout, run `git submodule update --init --recursive` before building. The vendor script downloads pinned sources and portable build tools, so the first run needs internet access. It uses MSVC to compile the libraries and a repo-local MSYS2 shell for FFmpeg's build. It installs no system-wide tools. Sources, caches, headers, libraries, and runtime DLLs are stored under the ignored `vendor\` directory.

The Node script uses only built-in modules. Run it with `--preflight` to check the tools or `--help` to see the available options.

Pass `--clean-work` to remove `vendor/.build/` after the script publishes the headers, libraries, and DLLs. This also removes cached downloads and portable build tools. A later vendor rebuild will download them again.

### Generate the Visual Studio solution with CMake

Use CMake 4.2 or later from an x64 Developer PowerShell for Visual Studio 2026. The `windows-x64` preset generates a Visual Studio 2026 solution with the v145 toolset.

If you previously configured with Visual Studio 2022, move `build/windows-x64` aside before generating the solution so CMake creates a fresh cache.

```powershell
node .\script\generate-windows-solution.js
cmake --build --preset windows-release
cmake --build --preset windows-debug
```

Open the generated `musikcube.slnx` (or `musikcube.sln`, depending on your CMake version) at the checkout root to build and debug from Visual Studio. Rerun the generator after adding or removing a CMake target. The GUI and console executables, plugins, and runtime files are in `bin64\Release` and `bin64\Debug`.

```powershell
node .\script\build-vendor-libraries.js --audit-directory .\bin64\Release
node .\script\build-vendor-libraries.js --audit-directory .\bin64\Debug
```

Both configurations use the same shared dependency DLLs. The static libraries used by plugins are built for the corresponding Release or Debug CRT.

After changing dependency versions or build flags, rerun the vendor script, run `script\clean-win.bat` to clear the old build and staged DLLs, and rebuild. FFmpeg's codec and format options are in `script/vendor-packages/windows/ffmpeg-options.txt`.

### Package a Release build with MilkDrop

For the optional MilkDrop visualizer, clone the plugin into `../milkdrop2-musikcube` next to the musikcube checkout and use a revision that supports Visual Studio 2026. After building the vendor libraries, run `script\build-win.bat` from the VS 2026 Developer PowerShell. It builds Release x64 and, when the sibling solution is present, restores MilkDrop's D3DX dependency and builds the plugin.

From the checkout root, run `bash script/archive-win.sh <version>` in a shell with Bash and 7-Zip available. The ZIP includes the staged MilkDrop plugin, resources, and DirectX runtime DLLs.
