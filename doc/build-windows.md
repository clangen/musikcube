# Building musikcube on Windows

Install Git, Node.js, and Visual Studio 2026 with the Desktop development with C++ workload, including the x64 compiler and a Windows SDK. Keep the checkout path free of spaces for the FFmpeg build. The supported Visual Studio configurations are **Debug x64** and **Release x64**.

Clone with submodules so the Asio sources are available. From the checkout root, run the vendor build once before building the solution.

```powershell
git clone --recurse-submodules https://github.com/clangen/musikcube.git
Set-Location musikcube
node .\script\build-vendor-libraries.js
```

For an existing checkout, run `git submodule update --init --recursive` before building. The vendor script downloads pinned sources and portable build tools, so the first run needs internet access. It uses MSVC to compile the libraries and a repo-local MSYS2 shell for FFmpeg's build. It installs no system-wide tools. Sources, caches, headers, libraries, and runtime DLLs are stored under the ignored `vendor\` directory.

The Node script uses only built-in modules. Run it with `--preflight` to check the tools or `--help` to see the available options. Windows uses WinHTTP and Schannel by default. To build with the optional curl or OpenSSL backends, first run the vendor script with `--library curl` or `--library openssl`, then configure with `MUSIKCORE_USE_WINDOWS_HTTP_CLIENT=OFF` or `MUSIKCORE_USE_ASIO_WINTLS=OFF`, respectively.

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

Both frontends import the C API from the same `musikcore.dll` in their configuration's runtime directory. The corresponding import library is `build/windows-x64/lib/<configuration>/musikcore.lib`.

```powershell
node .\script\build-vendor-libraries.js --audit-directory .\bin64\Release
node .\script\build-vendor-libraries.js --audit-directory .\bin64\Debug
```

Both configurations use the same shared dependency DLLs. The static libraries used by plugins are built for the corresponding Release or Debug CRT.

The projectM visualizer and its presets are included in the Windows build. Its
OpenGL window requires a graphics driver that supports OpenGL 3.3. The vendor
build currently uses the pinned vcpkg projectM 4.1.4 and SDL 2.32.10 packages;
the Linux vendor build uses projectM 4.1.8 and SDL 2.30.11.

The external MilkDrop2 plugin is optional. To build it from a neighboring
`milkdrop2-musikcube` checkout, pass `--with-milkdrop` to the build and archive
script to build and include the plugin, its resources, and its legacy DirectX DLLs.

After changing dependency versions or build flags, rerun the vendor script, run `script\clean-win.bat` to clear the old build and staged DLLs, and rebuild. FFmpeg's codec and format options are in `script/vendor-packages/windows/ffmpeg-options.txt`.

### Capture the GUI during resizing

In an interactive Windows desktop, open the GUI in a restored window large enough to shrink. Start the scenario from Windows PowerShell, then bring musikcube to the foreground during the three-second countdown.

```powershell
$cube = Get-Process musikcube
.\tests\ui\scenarios\windows-resize.ps1 -TargetProcessId $cube.Id
```

The scenario resizes the window and restores its original size. It checks that the client area shrinks and returns to its initial dimensions. PNG frames and a CSV with capture timestamps and dimensions are saved under `build/ui`. To record a manual border drag, invoke `tests/ui/support/capture-windows-window.ps1` with the same process ID, `-Frames 150`, and leave `-ShrinkPixels` at zero. The capture records visible desktop pixels, so keep the window unobstructed.

### Test the console frontend

```powershell
cmake --build --preset windows-release --target musikcube_ui_tests
ctest --test-dir build/windows-x64 -C Release -R windows_console --output-on-failure
```

The scenario runs `musikcube-cmd.exe` in a Windows pseudoconsole with a temporary profile and generated catalog. It checks that loaded tracks stay visible through repeated height changes and that the normal keyboard shortcut exits cleanly. On failure it retains the text screen, SVG, terminal transcript, and app logs under the build directory.

To compare two Windows Release builds, generate a local catalog once and pass its database to the benchmark. The benchmark copies the database for each run and uses null audio output so the comparison can run without an audio device.

```powershell
$build = 'build/windows-x64'
$baselineBin = 'C:\path\to\baseline\bin'
$catalog = node tests/scripts/seed-test-library.js --tracks 10000 --stereo --indexer bin64/Release/musikcube_seed_library.exe $build | Select-Object -Last 1
node tests/scripts/benchmark-windows.js "$build/tests/ui/bin/Release/musikcube_conpty_host.exe" $catalog $baselineBin bin64/Release "$build/performance/comparison"
```

The benchmark reports startup, search and filter latency, idle and playback CPU, and process memory. It writes raw samples to `results.json` in the specified output directory.

### Package a Release build with MilkDrop

For the optional MilkDrop visualizer, clone the plugin into `../milkdrop2-musikcube` next to the musikcube checkout. After building the vendor libraries, pass `--with-milkdrop` from the VS 2026 Developer PowerShell. The build restores MilkDrop's D3DX dependency and builds the plugin with the VS 2026 v145 toolset.

From the checkout root, run `node script/build-and-archive.js --version=4.0.0` with CMake and 7-Zip on the PATH. The script builds Release x64 and packages dependencies for the configured HTTP and TLS backends. Add `--clean` for a clean build or `--skip-archive` to build without packaging. With the default Windows backends, neither archive includes curl or OpenSSL DLLs. The application ZIP includes projectM and its presets; MilkDrop2 and its legacy DirectX DLLs are excluded unless requested. The GDI visualizer remains available as a sample and is not packaged. A separate `musikcore_sdk_<version>_win64.zip` contains the C SDK, including `lib/musikcore.lib`, the headers and CMake package, and `musikcore.dll` in `bin/`. CMake consumers can set `CMAKE_PREFIX_PATH` to the extracted SDK directory and link `musikcore::musikcore`.
