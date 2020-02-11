rd /s /q bin32\Release
rd /s /q bin32\Release-Con
rd /s /q bin32\Release-DLL
rd /s /q bin64\Release
rd /s /q bin64\Release-Con
rd /s /q bin64\Release-DLL
MSBuild.exe ../milkdrop2-musikcube/milkdrop2-musikcube.sln /m /nologo /verbosity:minimal /p:Configuration=Release /p:Platform=Win32
MSBuild.exe musikcube.sln /m /nologo /verbosity:minimal /p:Configuration=Release-Win /p:Platform=Win32
MSBuild.exe musikcube.sln /m /nologo /verbosity:minimal /p:Configuration=Release-Con /p:Platform=Win32
MSBuild.exe musikcube.sln /m /nologo /verbosity:minimal /p:Configuration=Release-DLL /p:Platform=Win32
MSBuild.exe musikcube.sln /m /nologo /verbosity:minimal /p:Configuration=Release-Win /p:Platform=Win64
MSBuild.exe musikcube.sln /m /nologo /verbosity:minimal /p:Configuration=Release-Con /p:Platform=Win64
MSBuild.exe musikcube.sln /m /nologo /verbosity:minimal /p:Configuration=Release-DLL /p:Platform=Win64
