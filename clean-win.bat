echo "*** REMOVING BUILD ARTIFACTS ***"
rd /s /q bin32
rd /s /q bin64

echo "*** CLEANING ALL BUILD FILES ***"
MSBuild.exe ../milkdrop2-musikcube/milkdrop2-musikcube.sln /m /t:Clean /nologo /verbosity:minimal /p:Configuration=Release /p:Platform=Win32
MSBuild.exe ../milkdrop2-musikcube/milkdrop2-musikcube.sln /m /t:Clean /nologo /verbosity:minimal /p:Configuration=Debug /p:Platform=Win32
MSBuild.exe musikcube.sln /m /t:Clean /nologo /verbosity:minimal /p:Configuration=Release-Win /p:Platform=Win32
MSBuild.exe musikcube.sln /m /t:Clean /nologo /verbosity:minimal /p:Configuration=Release-Con /p:Platform=Win32
MSBuild.exe musikcube.sln /m /t:Clean /nologo /verbosity:minimal /p:Configuration=Release-DLL /p:Platform=Win32
MSBuild.exe musikcube.sln /m /t:Clean /nologo /verbosity:minimal /p:Configuration=Debug-Win /p:Platform=Win32
MSBuild.exe musikcube.sln /m /t:Clean /nologo /verbosity:minimal /p:Configuration=Debug-Con /p:Platform=Win32
MSBuild.exe musikcube.sln /m /t:Clean /nologo /verbosity:minimal /p:Configuration=Debug-DLL /p:Platform=Win32
MSBuild.exe musikcube.sln /m /t:Clean /nologo /verbosity:minimal /p:Configuration=Release-Win /p:Platform=Win64
MSBuild.exe musikcube.sln /m /t:Clean /nologo /verbosity:minimal /p:Configuration=Release-Con /p:Platform=Win64
MSBuild.exe musikcube.sln /m /t:Clean /nologo /verbosity:minimal /p:Configuration=Release-DLL /p:Platform=Win64
MSBuild.exe musikcube.sln /m /t:Clean /nologo /verbosity:minimal /p:Configuration=Debug-Win /p:Platform=Win64
MSBuild.exe musikcube.sln /m /t:Clean /nologo /verbosity:minimal /p:Configuration=Debug-Con /p:Platform=Win64
MSBuild.exe musikcube.sln /m /t:Clean /nologo /verbosity:minimal /p:Configuration=Debug-DLL /p:Platform=Win64
