call clean-win.bat

echo "*** BUILDING MILKDROP ***"
MSBuild.exe ../milkdrop2-musikcube/milkdrop2-musikcube.sln /m /nologo /verbosity:minimal /p:Configuration=Release /p:Platform=Win32

echo "*** BUILDING WIN32 ***"
MSBuild.exe musikcube.sln /m /nologo /verbosity:minimal /p:Configuration=Release-Win /p:Platform=Win32
MSBuild.exe musikcube.sln /m /nologo /verbosity:minimal /p:Configuration=Release-Con /p:Platform=Win32
MSBuild.exe musikcube.sln /m /nologo /verbosity:minimal /p:Configuration=Release-DLL /p:Platform=Win32

echo "*** BUILDING WIN64 ***"
MSBuild.exe musikcube.sln /m /nologo /verbosity:minimal /p:Configuration=Release-Win /p:Platform=Win64
MSBuild.exe musikcube.sln /m /nologo /verbosity:minimal /p:Configuration=Release-Con /p:Platform=Win64
MSBuild.exe musikcube.sln /m /nologo /verbosity:minimal /p:Configuration=Release-DLL /p:Platform=Win64
