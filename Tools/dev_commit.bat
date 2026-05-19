@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion

echo.
echo ============================================
echo   Dev Commit - Valorant Aim Trainer
echo ============================================
echo.

if "%~1"=="" (
    echo Usage: dev_commit.bat "commit message"
    echo.
    echo Example:
    echo   dev_commit.bat "Phase 1: Create Input Actions and Blueprints"
    echo.
    pause
    exit /b 1
)

set MSG=%~1

cd /d "F:\project\gitprojects\UE\valorant\ascent1"

echo [1/4] Staging changes...
git add -A

echo [2/4] Committing: "%MSG%"
git commit -m "%MSG%"

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [INFO] Nothing to commit or commit failed.
    pause
    exit /b 0
)

echo [3/4] Updating CHANGELOG.md...

REM Add commit to changelog
echo ### %MSG% >> CHANGELOG_TEMP.md
echo - **Committed**: %date% %time% >> CHANGELOG_TEMP.md
echo. >> CHANGELOG_TEMP.md

echo [4/4] Done!
echo.
echo Commit successful: "%MSG%"
echo.

git log --oneline -5

echo.
pause
