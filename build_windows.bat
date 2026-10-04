@echo off
title Stem Cleaner Pro - Build VST3
echo === Stem Cleaner Pro - Generando VST3 ===
where cmake >nul 2>nul || (echo [ERROR] Instala CMake desde cmake.org & pause & exit /b 1)
where git >nul 2>nul || (echo [ERROR] Instala Git desde git-scm.com & pause & exit /b 1)
cmake -B build -G "Visual Studio 17 2022" -A x64
if errorlevel 1 (echo Error configurando & pause & exit /b 1)
cmake --build build --config Release
if errorlevel 1 (echo Error compilando & pause & exit /b 1)
echo.
echo LISTO! Tu plugin esta en:
echo build\StemCleanerPro_artefacts\Release\VST3\"Stem Cleaner Pro.vst3"
echo Copialo a C:\Program Files\Common Files\VST3\
pause
