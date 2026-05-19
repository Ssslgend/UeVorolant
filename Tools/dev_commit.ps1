# Dev Commit Script - Valorant Aim Trainer
# 用法: .\dev_commit.ps1 "commit message"

param(
    [Parameter(Mandatory=$true)]
    [string]$Message
)

$ProjectDir = "F:\project\gitprojects\UE\valorant\ascent1"

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  Dev Commit - Valorant Aim Trainer" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

Set-Location $ProjectDir

# 1. 检查 git 状态
Write-Host "[1/3] Checking git status..." -ForegroundColor Yellow
$status = git status --porcelain
if ([string]::IsNullOrWhiteSpace($status)) {
    Write-Host "  No changes to commit." -ForegroundColor Gray
    exit 0
}

# 2. 暂存并提交
Write-Host "[2/3] Staging and committing: `"$Message`"" -ForegroundColor Yellow
git add -A
git commit -m $Message

if ($LASTEXITCODE -ne 0) {
    Write-Host "  Commit failed!" -ForegroundColor Red
    exit 1
}

# 3. 显示最近的提交
Write-Host ""
Write-Host "[3/3] Recent commits:" -ForegroundColor Yellow
git log --oneline -5

Write-Host ""
Write-Host "Done!" -ForegroundColor Green
Write-Host ""
