@echo off
set start=%time%
@echo on

SET "scriptdir=%~dp0"
pushd "%scriptdir%.."
node "%scriptdir%generate-windows-solution.js" || (popd & exit /b 1)
call "%scriptdir%clean-win.bat" || (popd & exit /b 1)

echo "*** BUILDING x64 ***"
cmake --build --preset windows-release --parallel || (popd & exit /b 1)

rem Build the external MilkDrop plugin when its checkout is available.
if exist "..\milkdrop2-musikcube\milkdrop2-musikcube.sln" (
  MSBuild.exe ..\milkdrop2-musikcube\milkdrop2-musikcube.sln /restore /p:RestorePackagesConfig=true /m /nologo /verbosity:minimal /p:Configuration=Release /p:Platform=x64 || (popd & exit /b 1)
)

popd

echo "*** DONE ***"
@echo off
set end=%time%
set options="tokens=1-4 delims=:.,"
for /f %options% %%a in ("%start%") do set start_h=%%a&set /a start_m=100%%b %% 100&set /a start_s=100%%c %% 100&set /a start_ms=100%%d %% 100
for /f %options% %%a in ("%end%") do set end_h=%%a&set /a end_m=100%%b %% 100&set /a end_s=100%%c %% 100&set /a end_ms=100%%d %% 100
set /a hours=%end_h%-%start_h%
set /a mins=%end_m%-%start_m%
set /a secs=%end_s%-%start_s%
set /a ms=%end_ms%-%start_ms%
if %ms% lss 0 set /a secs = %secs% - 1 & set /a ms = 100%ms%
if %secs% lss 0 set /a mins = %mins% - 1 & set /a secs = 60%secs%
if %mins% lss 0 set /a hours = %hours% - 1 & set /a mins = 60%mins%
if %hours% lss 0 set /a hours = 24%hours%
if 1%ms% lss 100 set ms=0%ms%
set /a totalsecs = %hours%*3600 + %mins%*60 + %secs%
@echo on
echo Build time: %hours%:%mins%:%secs%.%ms% (%totalsecs%.%ms%s total)
