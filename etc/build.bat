REM TODO: Make this reasonable
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"

REM cl.exe /nologo /DDEBUG=1 /D_CRT_SECURE_NO_WARNINGS /D_CRT_SECURE_NO_DEPRECATE /W4 /Zi main.c
cl.exe /nologo /D_CRT_SECURE_NO_WARNINGS /D_CRT_SECURE_NO_DEPRECATE /W4 /O2 /DBENCH main.c
