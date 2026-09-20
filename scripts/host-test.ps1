# 在 host 上编译并跑全部单元测试。
# 本机 PATH 里没有 cmake / ninja / cl，这里借 Visual Studio 自带的那一套：
# 用 vswhere 找到最新的 VS，进入其开发者环境后再调 cmake 预设。
$ErrorActionPreference = 'Stop'

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) { throw '找不到 vswhere.exe：未安装 Visual Studio。' }

$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath) { throw '找不到带 C++ 工具集的 Visual Studio。' }

Import-Module (Join-Path $vsPath 'Common7\Tools\Microsoft.VisualStudio.DevShell.dll')
Enter-VsDevShell -VsInstallPath $vsPath -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw 'VS 开发者环境里没有 cmake：在 VS Installer 里勾选「用于 Windows 的 C++ CMake 工具」。'
}

Push-Location (Split-Path $PSScriptRoot -Parent)
try {
    cmake --preset host
    if ($LASTEXITCODE) { throw 'configure 失败' }
    cmake --build --preset host
    if ($LASTEXITCODE) { throw 'build 失败' }
    ctest --preset host
    if ($LASTEXITCODE) { throw '测试未通过' }
}
finally {
    Pop-Location
}
