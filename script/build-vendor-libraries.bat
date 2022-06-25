set PATH=%PATH%;C:\Program Files\NASM
set _CL_=/MT
set PREFIX=z:\src\vendor\bin\
set OPENSSLDIR=z:\src\vendor\bin\

mkdir z:\src\vendor\bin\

cd z:\src\vendor\openssl-3.0.2
perl Configure VC-WIN64A
nmake
nmake install