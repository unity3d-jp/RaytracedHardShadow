call toolchain.bat
msbuild rths.vcxproj /t:Build /p:Configuration=Release /p:Platform=x64 /m /nologo
