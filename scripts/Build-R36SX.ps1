param(
    [Parameter(Mandatory = $true)][string]$StockCardRoot,
    [Parameter(Mandatory = $true)][string]$HcrtosRoot,
    [Parameter(Mandatory = $true)][string]$Pcsx4allRoot,
    [Parameter(Mandatory = $true)][string]$DependencyRoot,
    [Parameter(Mandatory = $true)][string]$ZigPath,
    [Parameter(Mandatory = $true)][string]$GnuRuntimeRoot,
    [Parameter(Mandatory = $true)][string]$UpstreamReleaseArchive,
    [switch]$BuildPicoarch
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$buildRoot = Join-Path $repoRoot '.r36sx-build'
$output = Join-Path $buildRoot 'out'
$frogRoot = Join-Path $repoRoot 'frogui'
$picoRoot = Join-Path $buildRoot 'picoarch'
$appsRoot = Join-Path $repoRoot 'apps'
$hijackRoot = Join-Path $repoRoot 'hijack'
$sysLib = Join-Path $StockCardRoot 'cubegm\usr\lib'
$rootLib = Join-Path $StockCardRoot 'rootfs\usr\lib'
$hcSysInclude = Join-Path $HcrtosRoot 'components\prebuilts\sysroot\usr\include'
$hcUapiInclude = Join-Path $HcrtosRoot 'components\kernel\source\include\uapi'
$hcFfmpegInclude = Join-Path $HcrtosRoot 'components\ffmpeg\source'
$pcsxFont = Join-Path $Pcsx4allRoot 'src\port\sf3000\fonts.c'
$syscalls = Join-Path $repoRoot 'toolchain\mips_syscalls.S'

$required = @(
    $ZigPath,
    (Join-Path $DependencyRoot 'compat\SDL.h'),
    (Join-Path $DependencyRoot 'png\libpng-1.6.37\png.h'),
    (Join-Path $DependencyRoot 'zlib\zlib-1.2.11\zlib.h'),
    $sysLib,
    $rootLib,
    (Join-Path $hcSysInclude 'ffplayer.h'),
    $pcsxFont,
    $syscalls,
    (Join-Path $frogRoot 'frogui_libretro.c'),
    (Join-Path $GnuRuntimeRoot 'video_player'),
    (Join-Path $GnuRuntimeRoot 'pcsx4all'),
    $UpstreamReleaseArchive
)
if ($BuildPicoarch) { $required += (Join-Path $picoRoot 'main.c') }
foreach ($path in $required) {
    if (-not (Test-Path -LiteralPath $path)) { throw "Missing build input: $path" }
}
New-Item -ItemType Directory -Force -Path $output | Out-Null

# Reuse the official v1.0.12 MuPDF reader and the two upstream cores missing
# from the H.OS 1.2 stock payload. Their complete corresponding-source links
# and licenses are documented in docs/r36sx/FEATURES.md and upstream itself.
Add-Type -AssemblyName System.IO.Compression.FileSystem
$upstreamZip = [IO.Compression.ZipFile]::OpenRead((Resolve-Path -LiteralPath $UpstreamReleaseArchive).Path)
try {
    $runtimeEntries = @{
        'release/cubegm/ebook' = (Join-Path $output 'ebook')
        'release/cubegm/cores/o2em_libretro.so' = (Join-Path $output 'o2em_libretro.so')
        'release/cubegm/cores/vecx_libretro.so' = (Join-Path $output 'vecx_libretro.so')
    }
    foreach ($entryName in $runtimeEntries.Keys) {
        $entry = $upstreamZip.GetEntry($entryName)
        if (-not $entry) { throw "Missing upstream release entry: $entryName" }
        [IO.Compression.ZipFileExtensions]::ExtractToFile($entry,$runtimeEntries[$entryName],$true)
    }
} finally { $upstreamZip.Dispose() }

function Invoke-Zig {
    param([string[]]$Arguments)
    & $ZigPath @Arguments
    if ($LASTEXITCODE -ne 0) { throw "Zig failed with exit code $LASTEXITCODE" }
}

$target = 'mipsel-linux-gnueabihf.2.19'

if ($BuildPicoarch) {
    $picoSources = @(
        'libpicofe/input.c','libpicofe/in_sdl.c','libpicofe/linux/in_evdev.c',
        'libpicofe/linux/plat.c','libpicofe/fonts.c','libpicofe/readpng.c',
        'libpicofe/config_file.c','cheat.c','config.c','content.c','core.c',
        'menu.c','menu_font.c','main.c','options.c','overrides.c','patch.c',
        'scale.c','scaler_neon.c','unzip.c','util.c','plat_sf3000.c',
        'hwdisp.c','mips_syscalls.S'
    )
    $picoFlags = @(
        'cc','-target',$target,'-march=mips32r2','-O3','-fdata-sections',
        '-ffunction-sections','-D_GNU_SOURCE=1','-D_REENTRANT',
        '-DPICO_HOME_DIR="/.picoarch/"','-DCONTENT_DIR="/mnt/sdcard/roms"',
        '-DUSE_C_SCALER','-DPLATFORM_SF3000','-DNDEBUG','-I.',
        '-Ilibretro-common/include',"-I$(Join-Path $DependencyRoot 'compat')",
        "-I$(Join-Path $DependencyRoot 'png\libpng-1.6.37')",
        "-I$(Join-Path $DependencyRoot 'zlib\zlib-1.2.11')"
    )
    $picoLibs = @(
        "-L$sysLib","-L$rootLib",'-Wl,--gc-sections','-s','-lSDL',
        '-lpng','-lz','-ldl','-lm','-lpthread'
    )
    Push-Location $picoRoot
    try {
        Invoke-Zig (@($picoFlags + $picoSources + $picoLibs + @('-o',(Join-Path $output 'picoarch'))))
        Invoke-Zig (@($picoFlags + $picoSources + @('-Wl,--image-base=0x20000000') +
            $picoLibs + @('-o',(Join-Path $output 'picoarch_hi'))))
    } finally { Pop-Location }
}

$frogSources = @(
    'frogui_libretro.c','io_diagnostics.c','render.c','font.c','recent_games.c','settings.c',
    'theme.c','favorites.c','banner.c','backlight.c','input.c','core_override.c',
    $syscalls
)
Push-Location $frogRoot
try {
    Invoke-Zig (@('cc','-target',$target,'-march=mips32r2','-fPIC','-G0','-O3',
        '-DPLATFORM_SF3000','-DNDEBUG','-D__LIBRETRO__','-I.','-shared',
        '-Wl,--no-undefined','-s') + $frogSources + @('-lm','-lc','-ldl',
        '-lpthread','-o',(Join-Path $output 'frogui_libretro.so')))
} finally { Pop-Location }

Push-Location $appsRoot
try {
    Invoke-Zig @('cc','-target',$target,'-march=mips32r2','-fPIC','-G0','-O2','-DNDEBUG',
        '-Icompat',"-I$hcSysInclude","-I$hcUapiInclude","-I$hcFfmpegInclude",
        '-shared','-Wl,--no-undefined','-s','video_player.c',$pcsxFont,
        '-ldl','-lm','-lpthread','-o',(Join-Path $output 'video_player_impl.so'))
    # Zig/LLD MIPS executables fault before main() in the H.OS 1.2 loader.
    # Use the small launcher built with the official SF3000 GNU SDK instead;
    # the full player remains the shared module compiled above.
    Copy-Item -Force -LiteralPath (Join-Path $GnuRuntimeRoot 'video_player') `
        -Destination (Join-Path $output 'video_player')
    Copy-Item -Force -LiteralPath (Join-Path $GnuRuntimeRoot 'pcsx4all') `
        -Destination (Join-Path $output 'pcsx4all')
} finally { Pop-Location }

Push-Location $hijackRoot
try {
    Invoke-Zig @('cc','-target',$target,'-march=mips32r2','-fPIC','-G0','-O2',
        '-DNDEBUG','-shared','-Wl,--gc-sections','-s','tfhijack.c',$syscalls,
        '-o',(Join-Path $output 'libemu_tfhijack.so'))
    Invoke-Zig @('cc','-target',$target,'-march=mips32r2','-O2','-DNDEBUG','-s',
        'nosleep.c',$syscalls,'-o',(Join-Path $output 'nosleep'))
    Invoke-Zig @('cc','-target',$target,'-march=mips32r2','-fPIC','-G0','-O2',
        '-shared','-s',(Join-Path $appsRoot 'r36sx_displayfix.c'),'-ldl',
        '-o',(Join-Path $output 'r36sx_displayfix.so'))
} finally { Pop-Location }

Get-ChildItem -LiteralPath $output -File |
    Where-Object { $_.Name -ne 'SHA256SUMS.txt' } |
    Get-FileHash -Algorithm SHA256 |
    ForEach-Object { "$($_.Hash)  $([IO.Path]::GetFileName($_.Path))" } |
    Set-Content -Encoding ascii (Join-Path $output 'SHA256SUMS.txt')

# Also stage the files in their final SD-card layout. This keeps source builds
# genuinely plug-and-play and ensures redistributable UI assets are not missed.
$cardFiles = Join-Path $output 'card-files'
$cardCore = Join-Path $cardFiles 'cubegm\cores'
$cardFonts = Join-Path $cardFiles 'frogui\fonts'
$cardSounds = Join-Path $cardFiles 'frogui\sounds'
$cardIconPacks = Join-Path $cardFiles 'frogui\icon-packs'
New-Item -ItemType Directory -Force -Path $cardCore,$cardFonts,$cardSounds,$cardIconPacks | Out-Null
$optionalRomFolders = @('Ebook','doom','heretic','hexen','arcade','fbneo','mame2003',
    'lynx','snes9x','vectrex','odyssey2','videopac')
foreach ($folder in $optionalRomFolders) {
    New-Item -ItemType Directory -Force -Path (Join-Path $cardFiles "roms\$folder") | Out-Null
}
Copy-Item -Force -LiteralPath (Join-Path $output 'frogui_libretro.so') -Destination $cardCore
Copy-Item -Force -LiteralPath (Join-Path $output 'video_player') -Destination (Join-Path $cardFiles 'cubegm')
Copy-Item -Force -LiteralPath (Join-Path $output 'video_player_impl.so') -Destination (Join-Path $cardFiles 'cubegm')
Copy-Item -Force -LiteralPath (Join-Path $output 'pcsx4all') -Destination (Join-Path $cardFiles 'cubegm')
Copy-Item -Force -LiteralPath (Join-Path $output 'ebook') -Destination (Join-Path $cardFiles 'cubegm')
Copy-Item -Force -LiteralPath (Join-Path $output 'o2em_libretro.so') -Destination $cardCore
Copy-Item -Force -LiteralPath (Join-Path $output 'vecx_libretro.so') -Destination $cardCore
Copy-Item -Force -LiteralPath (Join-Path $appsRoot 'video_player.sh') -Destination (Join-Path $cardFiles 'cubegm')
Copy-Item -Force -Path (Join-Path $frogRoot 'fonts\*') -Destination $cardFonts
$extraFonts = Join-Path $repoRoot 'assets\ui-fonts'
if (Test-Path -LiteralPath $extraFonts) { Copy-Item -Recurse -Force -Path (Join-Path $extraFonts '*') -Destination $cardFonts }
$uiSounds = Join-Path $repoRoot 'assets\sounds'
if (Test-Path -LiteralPath $uiSounds) { Copy-Item -Recurse -Force -Path (Join-Path $uiSounds '*') -Destination $cardSounds }
$iconPacks = Join-Path $repoRoot 'assets\icon-packs'
if (Test-Path -LiteralPath $iconPacks) { Copy-Item -Recurse -Force -Path (Join-Path $iconPacks '*') -Destination $cardIconPacks }
Write-Host "R36SX build complete: $output"
