# 农历查表的全范围独立核对：
# 把 lunar_dump.exe 的逐日输出与 .NET 的 System.Globalization.ChineseLunisolarCalendar 比对。
# 后者是微软的独立实现，支持公历 1901-02-19 至 2101-01-28；更早的日期本脚本不比对，如实计入「未覆盖」。
# 先跑 scripts\host-test.ps1 生成 build\host\lunar_dump.exe。
$ErrorActionPreference = 'Stop'

$root = Split-Path $PSScriptRoot -Parent
$exe = Join-Path $root 'build\host\lunar_dump.exe'
if (-not (Test-Path $exe)) { throw "找不到 $exe，先跑 scripts\host-test.ps1。" }

$cal = [System.Globalization.ChineseLunisolarCalendar]::new()
$min = $cal.MinSupportedDateTime
$max = $cal.MaxSupportedDateTime

$compared = 0; $skipped = 0; $mismatch = 0; $failed = 0
foreach ($line in & $exe) {
    $f = $line.Split(' ')
    $date = [datetime]::ParseExact($f[0], 'yyyy-MM-dd', [cultureinfo]::InvariantCulture)
    if ($f[1] -eq 'FAIL') { $failed++; Write-Host "换算失败 $line"; continue }
    if ($date -lt $min -or $date -gt $max) { $skipped++; continue }

    $y = $cal.GetYear($date); $m = $cal.GetMonth($date); $d = $cal.GetDayOfMonth($date)
    # .NET 在闰年把月份编号为 1..13，GetLeapMonth 给出闰月所在的序号
    $leapIndex = $cal.GetLeapMonth($y)
    $isLeap = 0
    if ($leapIndex -gt 0 -and $m -ge $leapIndex) {
        if ($m -eq $leapIndex) { $isLeap = 1 }
        $m--
    }

    $compared++
    if ([int]$f[1] -ne $y -or [int]$f[2] -ne $m -or [int]$f[3] -ne $d -or [int]$f[4] -ne $isLeap) {
        $mismatch++
        if ($mismatch -le 20) { Write-Host "不一致 $line  | .NET: $y $m $d $isLeap" }
    }
}

Write-Host "比对 $compared 天，不一致 $mismatch，换算失败 $failed，未覆盖（早于 $($min.ToString('yyyy-MM-dd'))）$skipped 天"
if ($mismatch -ne 0 -or $failed -ne 0) { exit 1 }
