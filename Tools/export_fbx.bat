@echo off
chcp 65001 >nul
echo ============================================
echo   Valorant Ascent - Blender FBX Export
echo ============================================
echo.

set BLENDER="F:\Program Files\blender\blender.exe"
set SOURCE="H:\baiduyun\亚海悬城.blend"
set OUTPUT="H:\baiduyun\亚海悬城_UE.fbx"

echo [1/2] Exporting FBX from Blender...
echo   Source: %SOURCE%
echo   Output: %OUTPUT%
echo.

%BLENDER% -b %SOURCE% -- %OUTPUT%

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] Export failed! Check Blender output above.
    pause
    exit /b 1
)

echo.
echo [2/2] Done! FBX exported successfully.
echo.
echo ============================================
echo   Next steps:
echo   1. Open UE project ascent1
echo   2. Content Browser -> Import
echo   3. Select: %OUTPUT%
echo   4. Set Import Uniform Scale = 1.0
echo   5. Click Import
echo ============================================
pause
