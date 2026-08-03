param(
    [string]$GitPath = 'C:\Program Files\Git\cmd\git.exe'
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$buildRoot = Join-Path $repoRoot '.r36sx-build'
$frogRoot = Join-Path $repoRoot 'frogui'
$picoRoot = Join-Path $buildRoot 'picoarch'
$frogCommit = '6c74b5cceeaf98c8d1ef0f12e78b4259e4f65df1'
$picoCommit = 'f8ff5ba'

if (-not (Test-Path -LiteralPath $GitPath)) {
    $cmd = Get-Command git -ErrorAction SilentlyContinue
    if (-not $cmd) { throw 'Git was not found. Pass -GitPath.' }
    $GitPath = $cmd.Source
}

function Invoke-Git {
    param([string]$WorkingTree, [string[]]$Arguments, [switch]$AllowFailure)
    & $GitPath -C $WorkingTree @Arguments
    if (($LASTEXITCODE -ne 0) -and -not $AllowFailure) {
        throw "git failed in ${WorkingTree}: $($Arguments -join ' ')"
    }
    return $LASTEXITCODE
}

New-Item -ItemType Directory -Force -Path $buildRoot | Out-Null

if (-not (Test-Path -LiteralPath (Join-Path $frogRoot '.git'))) {
    Invoke-Git $repoRoot @('submodule', 'update', '--init', '--recursive', 'frogui') | Out-Null
}
$frogHead = (& $GitPath -C $frogRoot rev-parse HEAD).Trim()
if ($frogHead -ne $frogCommit) {
    throw "Unexpected FrogUI revision $frogHead; expected $frogCommit. No reset was performed."
}

$frogPatch = Join-Path $repoRoot 'patches\r36sx-frogui.patch'
$savedPreference = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
& $GitPath -C $frogRoot apply --whitespace=nowarn --reverse --check $frogPatch 2>$null
$alreadyApplied = ($LASTEXITCODE -eq 0)
$ErrorActionPreference = $savedPreference
if ($alreadyApplied) {
    Write-Host 'FrogUI R36SX patch is already applied.'
} else {
    Invoke-Git $frogRoot @('apply', '--whitespace=nowarn', '--check', $frogPatch) | Out-Null
    Invoke-Git $frogRoot @('apply', '--whitespace=nowarn', $frogPatch) | Out-Null
    Write-Host 'Applied FrogUI R36SX patch.'
}

if (-not (Test-Path -LiteralPath $picoRoot)) {
    & $GitPath clone --no-checkout https://github.com/tzubertowski/TreeFrogUI_picoarch.git $picoRoot
    if ($LASTEXITCODE -ne 0) { throw 'Unable to clone TreeFrogUI PicoArch.' }
    Invoke-Git $picoRoot @('checkout', '--detach', $picoCommit) | Out-Null
}
$picoHead = (& $GitPath -C $picoRoot rev-parse HEAD).Trim()
if (-not $picoHead.StartsWith($picoCommit)) {
    throw "Unexpected PicoArch revision $picoHead; expected $picoCommit. No reset was performed."
}

$picoPatch = Join-Path $repoRoot 'patches\r36sx-picoarch.patch'
$savedPreference = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
& $GitPath -C $picoRoot apply --whitespace=nowarn --reverse --check $picoPatch 2>$null
$alreadyApplied = ($LASTEXITCODE -eq 0)
$ErrorActionPreference = $savedPreference
if ($alreadyApplied) {
    Write-Host 'PicoArch R36SX patch is already applied.'
} else {
    Invoke-Git $picoRoot @('apply', '--whitespace=nowarn', '--check', $picoPatch) | Out-Null
    Invoke-Git $picoRoot @('apply', '--whitespace=nowarn', $picoPatch) | Out-Null
    Write-Host 'Applied PicoArch R36SX patch.'
}

Copy-Item -Force -LiteralPath (Join-Path $repoRoot 'toolchain\mips_syscalls.S') `
    -Destination (Join-Path $picoRoot 'mips_syscalls.S')

Write-Host "Prepared R36SX sources under $buildRoot"
