# 在 PC 上编译并运行纯逻辑单元测试（不需要 ARM 工具链）
# 用法：powershell -NoProfile -ExecutionPolicy Bypass -File tools/host_test/run_tests.ps1
param(
    [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
)

$ErrorActionPreference = 'Stop'

$gcc = Get-Command gcc -ErrorAction SilentlyContinue
if (-not $gcc) {
    $candidate = 'E:\mingw64\bin\gcc.exe'
    if (Test-Path $candidate) { $gcc = Get-Item $candidate } else { throw '未找到 gcc（MinGW-w64）' }
}

$out = Join-Path $RepoRoot 'build_host'
New-Item -ItemType Directory -Force -Path $out | Out-Null

$cflags = @('-std=c11', '-Wall', '-Wextra', '-Werror', '-O1', '-g',
            "-I$RepoRoot\tools\host_test\stubs",
            "-I$RepoRoot\application", "-I$RepoRoot\components", "-I$RepoRoot\bsp")

$targets = @(
    @{ Name = 'test_servo_map'; Src = @(
        'tools\host_test\test_servo_map.c', 'application\app_config.c', 'components\servo_map.c') },
    @{ Name = 'test_motion'; Src = @(
        'tools\host_test\test_motion.c', 'components\motion_profile.c') }
)

foreach ($t in $targets) {
    $src = $t.Src | ForEach-Object { Join-Path $RepoRoot $_ }
    $exe = Join-Path $out ($t.Name + '.exe')
    Write-Host "==> 编译 $($t.Name)" -ForegroundColor Cyan
    & $gcc.Source @cflags @src '-lm' '-o' $exe
    if ($LASTEXITCODE -ne 0) { throw "编译失败: $($t.Name)" }
    & $exe
    if ($LASTEXITCODE -ne 0) { throw "测试失败: $($t.Name)" }
}

Write-Host '全部单元测试通过' -ForegroundColor Green
