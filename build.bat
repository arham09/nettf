@echo off
echo Building nettf file transfer tool for Windows...

if not exist obj mkdir obj

gcc -Wall -Wextra -std=c99 -O2 -c src/platform.c -o obj/platform.o
if errorlevel 1 goto :error

gcc -Wall -Wextra -std=c99 -O2 -c src/protocol.c -o obj/protocol.o
if errorlevel 1 goto :error

gcc -Wall -Wextra -std=c99 -O2 -c src/client.c -o obj/client.o
if errorlevel 1 goto :error

gcc -Wall -Wextra -std=c99 -O2 -c src/server.c -o obj/server.o
if errorlevel 1 goto :error

gcc -Wall -Wextra -std=c99 -O2 -c src/main.c -o obj/main.o
if errorlevel 1 goto :error

gcc obj/platform.o obj/protocol.o obj/client.o obj/server.o obj/main.o -o nettf.exe -lws2_32
if errorlevel 1 goto :error

echo Build successful! Executable: nettf.exe
goto :end

:error
echo Build failed!
exit /b 1

:end
pause