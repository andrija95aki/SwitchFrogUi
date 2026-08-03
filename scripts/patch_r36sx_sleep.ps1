param(
    [string[]]$CardRoots = @('TESTCARD', 'E:\')
)

$ErrorActionPreference = 'Stop'
$targets = @(0x6D24, 0x701C, 0x6B50)

foreach ($root in $CardRoots) {
    $cubevol = Join-Path $root 'rootfs\usr\bin\cubevol'
    if (-not (Test-Path -LiteralPath $cubevol)) {
        throw "R36SX cubevol not found: $cubevol"
    }
    $resolved = (Resolve-Path -LiteralPath $cubevol).Path
    $backup = "$resolved.pre-treefrog-nosleep"
    $bytes = [IO.File]::ReadAllBytes($resolved)
    if ($bytes.Length -lt 0x7020) {
        throw "Unexpected cubevol size at $resolved"
    }
    if (-not (Test-Path -LiteralPath $backup)) {
        Copy-Item -LiteralPath $resolved -Destination $backup
    }
    foreach ($offset in $targets) {
        $alreadyPatched = $bytes[$offset] -eq 0 -and $bytes[$offset + 1] -eq 0 -and
                          $bytes[$offset + 2] -eq 0 -and $bytes[$offset + 3] -eq 0
        if ($alreadyPatched) { continue }
        if ($bytes[$offset] -ne 0x18 -or $bytes[$offset + 1] -ne 0xBB) {
            throw ('Refusing unknown cubevol revision at {0}, offset 0x{1:X}' -f $resolved, $offset)
        }
        $bytes[$offset] = $bytes[$offset + 1] = $bytes[$offset + 2] = $bytes[$offset + 3] = 0
    }
    [IO.File]::WriteAllBytes($resolved, $bytes)
    $verify = [IO.File]::ReadAllBytes($resolved)
    foreach ($offset in $targets) {
        if ($verify[$offset] -ne 0 -or $verify[$offset + 1] -ne 0 -or
            $verify[$offset + 2] -ne 0 -or $verify[$offset + 3] -ne 0) {
            throw ('Sleep patch verification failed at {0}, offset 0x{1:X}' -f $resolved, $offset)
        }
    }
    Write-Host "Disabled unsafe R36SX standby in $resolved"
}
