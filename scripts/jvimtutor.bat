@echo off
rem jvimtutor.bat: open a copy of the JVim tutorial on Windows

setlocal

set TUTOR_TMP=%TEMP%
if "%TUTOR_TMP%"=="" set TUTOR_TMP=%TMP%
if "%TUTOR_TMP%"=="" set TUTOR_TMP=.

set JVIM_DIR=%~dp0
set BIN=%JVIM_DIR%jvim32w.exe
if not exist "%BIN%" set BIN=%JVIM_DIR%jvim32.exe
if not exist "%BIN%" set BIN=jvim32.exe

set TUTOR_SRC=
rem Check for Japanese tutor
if exist "%JVIM_DIR%tutor\tutor.j" set TUTOR_SRC=%JVIM_DIR%tutor\tutor.j
if "%TUTOR_SRC%"=="" if exist "%JVIM_DIR%doc.j\tutor\tutor.j" set TUTOR_SRC=%JVIM_DIR%doc.j\tutor\tutor.j

rem Fallback to English tutor if not found or English preferred
if "%TUTOR_SRC%"=="" if exist "%JVIM_DIR%tutor\tutor" set TUTOR_SRC=%JVIM_DIR%tutor\tutor
if "%TUTOR_SRC%"=="" if exist "%VIM%\tutor\tutor" set TUTOR_SRC=%VIM%\tutor\tutor
if "%TUTOR_SRC%"=="" if exist "%VIM%\tutor\tutor.j" set TUTOR_SRC=%VIM%\tutor\tutor.j

if "%TUTOR_SRC%"=="" (
    echo jvimtutor: tutor file not found
    exit /b 1
)

set TMPFILE=%TUTOR_TMP%\tutor%RANDOM%.txt
copy /y "%TUTOR_SRC%" "%TMPFILE%" >nul 2>&1
if errorlevel 1 (
    echo jvimtutor: failed to create temporary tutor copy
    exit /b 1
)

start "" /wait "%BIN%" "%TMPFILE%"

if exist "%TMPFILE%" del /f /q "%TMPFILE%" >nul 2>&1

endlocal
