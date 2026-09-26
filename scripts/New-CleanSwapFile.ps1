param([Parameter(Mandatory=$true)][string]$OutputPath)
$ErrorActionPreference='Stop'
# Linux swap v1, 128 MiB, 4096-byte pages, little-endian R36SX.
# Never redistribute the contents of a previously used swap file.
$header=New-Object byte[] 4096
[BitConverter]::GetBytes([uint32]1).CopyTo($header,1024)
[BitConverter]::GetBytes([uint32]32767).CopyTo($header,1028)
[Text.Encoding]::ASCII.GetBytes('SWAPSPACE2').CopyTo($header,4086)
$stream=[IO.File]::Open([IO.Path]::GetFullPath($OutputPath),[IO.FileMode]::CreateNew)
try {
    $stream.SetLength(128MB)
    $stream.Write($header,0,$header.Length)
    $stream.Flush()
} finally {$stream.Dispose()}
