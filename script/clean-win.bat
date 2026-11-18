@echo off
pushd "%~dp0.."
if exist "build\windows-x64\CMakeCache.txt" (
  cmake --build --preset windows-release --target clean || (popd & exit /b 1)
  cmake --build --preset windows-debug --target clean || (popd & exit /b 1)
)
rem CMake's clean target leaves the copied runtime files in bin64.
node -e "const fs = require('node:fs'); const output = require('node:path').join(fs.realpathSync('.'), 'bin64'); if (fs.existsSync(output)) { if (fs.realpathSync(output) !== output) throw new Error('Unexpected bin64 path'); fs.rmSync(output, { recursive: true }); }" || (popd & exit /b 1)
popd
