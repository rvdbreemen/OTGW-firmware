@echo off
set "mklittlefs_exec=%homedrive%%homepath%\AppData\Local\Arduino15\packages\esp8266\tools\mklittlefs\2.5.0-4-fe5bb56\mklittlefs.exe"
set /A block_size = 4096
set /A page_size = 256
set /A fs_size = 1024 * 1024
set "source_dir=data"
set "img_name=OTGW-Firmware.littlefs.1M.bin"
echo Building filesystem (%source_dir%):
set "build_command=%mklittlefs_exec% -c %source_dir% -b %block_size% -p %page_size% -s %fs_size% %img_name%"
call %build_command%
pause
echo Check content of filesystem:
set "check_command=%mklittlefs_exec% -l  -b %block_size% -p %page_size% -s %fs_size% %img_name%"
call %check_command%


