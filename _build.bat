@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat" || exit /b 1
cmake -S "%~dp0." -B "%~dp0build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DNPP_SDK_INCLUDE="%~dp0_deps\plugintemplate\src" || exit /b 1
cmake --build "%~dp0build" || exit /b 1
echo BUILD_DONE
