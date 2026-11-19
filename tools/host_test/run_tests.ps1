# 在 PC 上编译并运行纯逻辑单元测试（不需要 STM32 硬件）
# 用法：pwsh -File tools/host_test/run_tests.ps1
param(
    [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
)

$ErrorActionPreference = 'Stop'
$out = Join-Path $RepoRoot 'build_host'
New-Item -ItemType Directory -Force -Path $out | Out-Null

$gcc = Get-Command gcc -ErrorAction SilentlyContinue
if (-not $gcc) { throw '未找到 gcc，请安装 MinGW-w64，或在 WSL/Linux 下用同样的 gcc 命令运行' }

$cflags = @('-std=c17', '-Wall', '-Wextra', '-Werror', '-O1', '-g',
            "-I$RepoRoot\tools\host_test\stubs",
            "-I$RepoRoot\protocol", "-I$RepoRoot\components",
            "-I$RepoRoot\bsp", "-I$RepoRoot\application")

$targets = @(
    @{ Name = 'test_protocol'; Src = @(
        'tools\host_test\test_protocol.c', 'protocol\arm_protocol.c', 'protocol\crc8.c') },
    @{ Name = 'test_motion'; Src = @(
        'tools\host_test\test_motion.c', 'components\motion_profile.c', 'components\servo_map.c') },
    @{ Name = 'test_app'; Src = @(
        'tools\host_test\test_app.c', 'tools\host_test\stub_hal.c',
        'protocol\arm_protocol.c', 'protocol\crc8.c',
        'components\servo_map.c', 'components\motion_profile.c',
        'bsp\bsp_time.c', 'bsp\bsp_pwm.c', 'bsp\bsp_usb_cdc.c', 'bsp\usb_link.c',
        'application\app_config.c', 'application\app_servo.c', 'application\app_motion.c',
        'application\app_state.c', 'application\app_protocol.c') }
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
