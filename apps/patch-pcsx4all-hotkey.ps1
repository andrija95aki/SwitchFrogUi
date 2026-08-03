param(
    [Parameter(Mandatory = $true)]
    [string]$Path
)

$ErrorActionPreference = 'Stop'
$OriginalSha256 = 'AC67AF3B8D0D1C35B0BFFAD810FF791D6CB71738F0D3A832CF992CDF53688433'

$resolved = (Resolve-Path -LiteralPath $Path).Path
$bytes = [IO.File]::ReadAllBytes($resolved)

# MIPS32 little-endian patches for the exact Jul 30 2026 PCSX4ALL build:
#
# 0x0040A7DC: bne a2,zero,exit_path -> bne a2,zero,menu_path
#   Start+Select used the clean-exit branch, which looked like a crash because
#   FrogUI immediately restarted. Route that combination through the existing
#   PCSX4ALL menu path instead.
#
# 0x0040A828: andi v0,v0,0x0400 -> andi v0,v0,0x0408
#   The existing release debounce waited for Select+L1. Also wait for Start so
#   the held combo cannot fall through into an item/action in the opened menu.
$patches = @(
    @{ Offset = 0xA81C; Old = [byte[]](0x2C,0x01,0xC0,0x14); New = [byte[]](0x07,0x00,0xC0,0x14) },
    @{ Offset = 0xA868; Old = [byte[]](0x00,0x04,0x42,0x30); New = [byte[]](0x08,0x04,0x42,0x30) }
)

function Test-Bytes([byte[]]$Data, [int]$Offset, [byte[]]$Expected) {
    if ($Offset + $Expected.Length -gt $Data.Length) { return $false }
    for ($i = 0; $i -lt $Expected.Length; $i++) {
        if ($Data[$Offset + $i] -ne $Expected[$i]) { return $false }
    }
    return $true
}

$alreadyPatched = $true
foreach ($patch in $patches) {
    if (-not (Test-Bytes $bytes $patch.Offset $patch.New)) {
        $alreadyPatched = $false
        break
    }
}
if ($alreadyPatched) {
    Write-Host "PCSX4ALL hotkey patch already present: $resolved"
    exit 0
}

$actualSha256 = (Get-FileHash -LiteralPath $resolved -Algorithm SHA256).Hash
if ($actualSha256 -ne $OriginalSha256) {
    throw "Refusing to patch unknown PCSX4ALL build: SHA-256 $actualSha256"
}
foreach ($patch in $patches) {
    if (-not (Test-Bytes $bytes $patch.Offset $patch.Old)) {
        throw ('PCSX4ALL instruction mismatch at file offset 0x{0:X}' -f $patch.Offset)
    }
}

foreach ($patch in $patches) {
    [Array]::Copy($patch.New, 0, $bytes, $patch.Offset, $patch.New.Length)
}
[IO.File]::WriteAllBytes($resolved, $bytes)
Write-Host "Patched PCSX4ALL Start+Select menu hotkey: $resolved"
