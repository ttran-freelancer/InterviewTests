Compile:
cl /EHsc download.cpp /link libcurl.lib

Usage:
./download --url=<download url> --thread=<number of threads> --conn=<number of connections> --out=<download path>

Ex:
- Download (.exe) .NET Framework 4.8 Web Installer
download.exe "--url=https://download.visualstudio.microsoft.com/download/pr/014120d7-d689-4305-befd-3cb711108212/1f81f3962f75eff5d83a60abd3a3ec7b/ndp48-web.exe" "--out=./ndp48-web.exe"

download.exe "--url=https://download.visualstudio.microsoft.com/download/pr/014120d7-d689-4305-befd-3cb711108212/1f81f3962f75eff5d83a60abd3a3ec7b/ndp48-web.exe" "--out=./ndp48-web.exe" --thread=8 --conn=8