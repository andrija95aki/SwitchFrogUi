param(
    [Parameter(Mandatory=$true)][string]$StageDirectory,
    [Parameter(Mandatory=$true)][string]$ZipPath,
    [string]$PreviousArchive
)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$stage=(Resolve-Path -LiteralPath $StageDirectory).Path.TrimEnd('\')
$zip=[IO.Path]::GetFullPath($ZipPath)
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $zip) | Out-Null
if (Test-Path -LiteralPath $zip) {throw "Output already exists: $zip"}
$oldHashes=@{}
if ($PreviousArchive) {
    Copy-Item -LiteralPath $PreviousArchive -Destination $zip
    $stream=[IO.File]::Open($zip,[IO.FileMode]::Open)
    $archive=New-Object IO.Compression.ZipArchive($stream,[IO.Compression.ZipArchiveMode]::Update,$false)
    $reader=New-Object IO.StreamReader($archive.GetEntry('SHA256SUMS.txt').Open())
    try {while(($line=$reader.ReadLine()) -ne $null){
        if($line -match '^([A-Fa-f0-9]{64})  (.+)$'){$oldHashes[$Matches[2]]=$Matches[1]}
    }} finally {$reader.Dispose()}
    foreach($entry in @($archive.Entries)) {
        if (-not(Test-Path -LiteralPath (Join-Path $stage $entry.FullName))) {$entry.Delete()}
    }
} else {
    $stream=[IO.File]::Open($zip,[IO.FileMode]::CreateNew)
    $archive=New-Object IO.Compression.ZipArchive($stream,[IO.Compression.ZipArchiveMode]::Create,$false)
}
try {
    foreach($dir in Get-ChildItem -LiteralPath $stage -Recurse -Directory -Force){
        $name=$dir.FullName.Substring($stage.Length+1).Replace('\','/')+'/'
        if (!$PreviousArchive -or !$archive.GetEntry($name)) {$archive.CreateEntry($name) | Out-Null}
    }
    foreach($file in Get-ChildItem -LiteralPath $stage -Recurse -File -Force){
        $name=$file.FullName.Substring($stage.Length+1).Replace('\','/')
        if ($PreviousArchive) {
            if ($oldHashes[$name] -eq (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash) {continue}
            $oldEntry=$archive.GetEntry($name)
            if($oldEntry){$oldEntry.Delete()}
        }
        [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive,$file.FullName,$name,[IO.Compression.CompressionLevel]::Optimal) | Out-Null
    }
} finally {$archive.Dispose(); $stream.Dispose()}

$archive=[IO.Compression.ZipFile]::OpenRead($zip)
try {
    foreach($required in @('rootfs/etc/init.d/rcS','rootfs/dev/','rootfs/proc/',
        'rootfs/sys/','cubegm/rkgame','cubegm/cores/frogui_libretro.so',
        'cubegm/cores/.pcsx4all/','MD/dummy.md','roms/rockbox/.rockbox/rockbox')){
        if(-not $archive.GetEntry($required)){throw "ZIP missing required entry: $required"}
    }
    $expected=@{}
    foreach($line in [IO.File]::ReadAllLines((Join-Path $stage 'SHA256SUMS.txt'))){
        if($line -match '^([A-Fa-f0-9]{64})  (.+)$'){$expected[$Matches[2]]=$Matches[1]}
    }
    $verified=0
    foreach($entry in $archive.Entries){
        if($entry.FullName.Contains('\') -or $entry.FullName.StartsWith('/') -or $entry.FullName.Contains('../')){throw 'Non-portable/unsafe ZIP path'}
        if($entry.FullName.EndsWith('/')){continue}
        if($entry.FullName -in @('SHA256SUMS.txt','PUBLISH-AUDIT.txt')){continue}
        if(-not $expected.ContainsKey($entry.FullName)){throw "Unmanifested file: $($entry.FullName)"}
        $reader=$entry.Open(); $sha=[Security.Cryptography.SHA256]::Create()
        try {$actual=([BitConverter]::ToString($sha.ComputeHash($reader))).Replace('-','')}
        finally {$reader.Dispose(); $sha.Dispose()}
        if($actual -ne $expected[$entry.FullName]){throw "Archive hash mismatch: $($entry.FullName)"}
        $verified++
    }
    if($verified -ne $expected.Count){throw 'Missing manifest files in archive'}
} finally {$archive.Dispose()}
$hash=(Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash
$utf8=New-Object Text.UTF8Encoding($false)
[IO.File]::WriteAllText("$zip.sha256","$hash  $([IO.Path]::GetFileName($zip))`n",$utf8)
Write-Host "PASS: $verified archived file hashes; standard paths; hidden files and boot directories. $zip"
