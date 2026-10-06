@echo off
setlocal
REM ============================================================
REM  Project map source = Saved\AscentPlayable\AscentPlayable.blend
REM  One-shot: blend -> AscentPlayable_UE.fbx  (then Reimport in UE)
REM  NOTE: keep this file ASCII-only. UTF-8 CJK breaks cmd.exe parsing.
REM ============================================================

set BLENDER=F:\Program Files\Blender5.1\blender.exe
set ROOT=F:\project\gitprojects\UE\valorant\ascent1
set BLEND=%ROOT%\Saved\AscentPlayable\AscentPlayable.blend
set FBX=%ROOT%\Saved\AscentPlayable\AscentPlayable_UE.fbx

if not exist "%BLENDER%" (
    echo [ERROR] Blender not found: %BLENDER%
    exit /b 1
)
if not exist "%BLEND%" (
    echo [ERROR] Blend not found: %BLEND%
    exit /b 1
)

if exist "%FBX%" (
    for %%I in ("%FBX%") do set FBX_SIZE_BEFORE=%%~zI
) else (
    set FBX_SIZE_BEFORE=0
)

echo [1/2] Exporting FBX from AscentPlayable.blend ...
"%BLENDER%" -b "%BLEND%" -P "%ROOT%\Tools\export_fbx.py" -- "%FBX%"
set BLENDER_RC=%errorlevel%
if not "%BLENDER_RC%"=="0" (
    echo [ERROR] Blender export failed with exit code %BLENDER_RC%
    exit /b 1
)

if not exist "%FBX%" (
    echo [ERROR] FBX was not created: %FBX%
    exit /b 1
)
for %%I in ("%FBX%") do set FBX_SIZE_AFTER=%%~zI
if "%FBX_SIZE_AFTER%"=="0" (
    echo [ERROR] FBX is empty: %FBX%
    exit /b 1
)
if "%FBX_SIZE_BEFORE%"=="%FBX_SIZE_AFTER%" (
    echo [ERROR] FBX size unchanged ^(%FBX_SIZE_AFTER% bytes^) - export likely did not run
    exit /b 1
)

echo [2/2] Done: %FBX% ^(%FBX_SIZE_AFTER% bytes^)
echo.
echo Next: open UE editor - Content\Ascent\AscentPlayable - right-click Reimport
echo Or run:  Tools\ue_reimport_map.py  inside the editor Python console
echo.
echo Project map path is locked to: /Game/Ascent/AscentPlayable
echo Spots: Content\Data\trainer_spots_v1.json
endlocal
