<#
.SYNOPSIS
  Cài bộ ESP32 Firmware Skills từ package này vào Claude Code.

.DESCRIPTION
  Copy 13 skill (esp32-firmware + esp32-01..12) từ .claude/skills của package
  vào đích cài đặt. Chạy lại bất cứ lúc nào để đồng bộ — thao tác idempotent,
  chỉ ghi đè các thư mục esp32-*, không đụng skill khác.

.PARAMETER Scope
  User    : cài toàn cục vào ~/.claude/skills  (dùng được ở mọi dự án) — mặc định
  Project : cài vào <ProjectPath>/.claude/skills (chỉ dự án đó)

.PARAMETER ProjectPath
  Thư mục gốc dự án, bắt buộc khi -Scope Project.

.PARAMETER Uninstall
  Gỡ toàn bộ skill esp32-* khỏi đích, không đụng skill khác.

.EXAMPLE
  .\install.ps1
  .\install.ps1 -Scope Project -ProjectPath D:\work\my-esp32-app
  .\install.ps1 -Uninstall
#>
[CmdletBinding()]
param(
    [ValidateSet('User', 'Project')]
    [string]$Scope = 'User',
    [string]$ProjectPath,
    [switch]$Uninstall
)

$ErrorActionPreference = 'Stop'

$src = Join-Path $PSScriptRoot 'skills'
if (-not (Test-Path $src)) {
    throw "Không tìm thấy thư mục nguồn: $src"
}

if ($Scope -eq 'Project') {
    if (-not $ProjectPath) { throw "-Scope Project yêu cầu -ProjectPath" }
    if (-not (Test-Path $ProjectPath)) { throw "Không tồn tại: $ProjectPath" }
    $dst = Join-Path (Resolve-Path $ProjectPath) '.claude\skills'
} else {
    $dst = Join-Path $env:USERPROFILE '.claude\skills'
}

if (-not (Test-Path $dst)) { New-Item -ItemType Directory -Force -Path $dst | Out-Null }

# Gỡ các bản esp32-* cũ ở đích (kể cả junction sót lại từ lần cài trước)
Get-ChildItem $dst -Directory -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -like 'esp32-*' } |
    ForEach-Object {
        if ($_.LinkType -eq 'Junction') { cmd /c rmdir "$($_.FullName)" | Out-Null }
        else { Remove-Item $_.FullName -Recurse -Force }
    }

if ($Uninstall) {
    Write-Host "Da go bo ESP32 skills khoi: $dst"
    return
}

$n = 0
Get-ChildItem $src -Directory | ForEach-Object {
    Copy-Item $_.FullName -Destination (Join-Path $dst $_.Name) -Recurse -Force
    $n++
}

Write-Host "Da cai $n skill vao: $dst"
Write-Host "Khoi dong lai Claude Code de nhan dien skill moi."
