param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath,
    [Parameter(Mandatory = $true)]
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'
$expectedOriginal = '7C835181967FD1CEF677108EDB6D363EA7F9F6EDB941FA4A76418AA662AC85FD'
$expectedPatched = 'D7FA465824DA7841331759BEF0AF28B3B9F3F0CF316005251D94B69818F47891'
$inputHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $InputPath).Hash
if ($inputHash -eq $expectedPatched) {
    Copy-Item -LiteralPath $InputPath -Destination $OutputPath -Force
    Write-Host "Rockbox keymap is already patched: $OutputPath"
    exit 0
}
if ($inputHash -ne $expectedOriginal) {
    throw "Unsupported Rockbox binary SHA-256: $inputHash"
}

$bytes = [IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $InputPath))

function Convert-WordsToBytes {
    param([uint32[]]$Words)
    $result = New-Object byte[] ($Words.Count * 4)
    for ($i = 0; $i -lt $Words.Count; $i++) {
        $word = [BitConverter]::GetBytes($Words[$i])
        [Array]::Copy($word, 0, $result, $i * 4, 4)
    }
    return $result
}

function Find-Pattern {
    param([byte[]]$Data, [byte[]]$Pattern)
    $matches = New-Object System.Collections.Generic.List[int]
    for ($offset = 0; $offset -le $Data.Length - $Pattern.Length; $offset++) {
        $same = $true
        for ($i = 0; $i -lt $Pattern.Length; $i++) {
            if ($Data[$offset + $i] -ne $Pattern[$i]) {
                $same = $false
                break
            }
        }
        if ($same) { $matches.Add($offset) }
    }
    return $matches.ToArray()
}

function Replace-UniqueWords {
    param([uint32[]]$OldWords, [uint32[]]$NewWords, [string]$Description)
    $old = Convert-WordsToBytes $OldWords
    $new = Convert-WordsToBytes $NewWords
    if ($old.Length -ne $new.Length) { throw "$Description changes binary size" }
    $matches = Find-Pattern $bytes $old
    if ($matches.Count -ne 1) {
        throw "$Description expected one binary match, found $($matches.Count)"
    }
    [Array]::Copy($new, 0, $bytes, $matches[0], $new.Length)
    Write-Host ("Patched {0} at 0x{1:X}" -f $Description, $matches[0])
}

# These words are the compiled struct button_mapping tables from Rockbox commit
# 38cd25c5516fafb0c6e62775a24d11621b7f09ed. Each replacement is guarded by a
# full input hash and a unique surrounding sequence, so an unknown binary is
# rejected instead of being modified at a guessed offset.
Replace-UniqueWords `
    @([uint32]10,0x02010000,0x00010000, 10,0x02000008,0x00000008,
      11,0x00000004,0, 11,0x02000004,0x00000004, 12,0x00000020,0) `
    @([uint32]10,0x02000010,0x00000010, 10,0x02000008,0x00000008,
      11,0x00000040,0, 11,0x02000004,0x00000004, 12,0x00000020,0) `
    'standard A-confirm/B-back map'

Replace-UniqueWords `
    @([uint32]10,0x00010000,0, 11,0x00000040,0,
      70,0x00000100,0, 71,0x04000100,0) `
    @([uint32]10,0x00000010,0, 11,0x00000040,0,
      70,0x00000100,0, 71,0x04000100,0) `
    'Settings A-confirm map'

Replace-UniqueWords `
    @([uint32]96,0x00010000,0, 0,0,0) `
    @([uint32]96,0x00000010,0, 0,0,0) `
    'yes/no A-confirm map'

$parent = Split-Path -Parent $OutputPath
if ($parent) { New-Item -ItemType Directory -Force -Path $parent | Out-Null }
[IO.File]::WriteAllBytes($OutputPath, $bytes)
Write-Host "Wrote patched Rockbox binary: $OutputPath"
