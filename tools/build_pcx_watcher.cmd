@echo off
REM Rebuilds OracoolPcxWatcher.exe from OracoolPcxWatcher.cs.
REM
REM Uses the C# compiler that ships inside Windows itself, so there is nothing to install -
REM no Visual Studio, no SDK, no NuGet. Just double-click this file.

setlocal
set CSC=%WINDIR%\Microsoft.NET\Framework64\v4.0.30319\csc.exe
if not exist "%CSC%" set CSC=%WINDIR%\Microsoft.NET\Framework\v4.0.30319\csc.exe
if not exist "%CSC%" (
    echo Could not find the .NET Framework C# compiler.
    echo Looked in %WINDIR%\Microsoft.NET\Framework64\v4.0.30319 and Framework\v4.0.30319
    pause
    exit /b 1
)

"%CSC%" /nologo /target:winexe /optimize+ /out:"%~dp0OracoolPcxWatcher.exe" ^
    /reference:System.dll ^
    /reference:System.Drawing.dll ^
    /reference:System.Windows.Forms.dll ^
    "%~dp0OracoolPcxWatcher.cs"

if errorlevel 1 (
    echo.
    echo BUILD FAILED
    pause
    exit /b 1
)

echo.
echo Built: %~dp0OracoolPcxWatcher.exe
pause
