@echo off
set "mklittlefs_exec=%homedrive%%homepath%\AppData\Local\Arduino15\packages\esp8266\tools\mklittlefs\2.5.0-4-fe5bb56\mklittlefs.exe"
set /A block_size = 8192
set /A page_size = 256
set /A fs_size = (2 * 1024 * 1024) - (24 * 1024)
set "source_dir=data"
set "img_name=OTGW-Firmware.littlefs.2M.bin"
set "build_command=%mklittlefs_exec% -c %source_dir% -b %block_size% -p %page_size% -s %fs_size% %img_name%"
call %build_command%
pause
set "check_command=%mklittlefs_exec% -l  -b %block_size% -p %page_size% -s %fs_size% %img_name%"
call %check_command%


