[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$SourceRoot,
    [Parameter(Mandatory = $true)][string]$BuildRoot,
    [Parameter(Mandatory = $true)][string]$CMakeExe,
    [Parameter(Mandatory = $true)][string]$PythonExe,
    [string]$GitBin = 'C:\msys64\usr\bin',
    [Parameter(Mandatory = $true)][string]$Rom,
    [Parameter(Mandatory = $true)][string]$GeneratedDir,
    [Parameter(Mandatory = $true)][string]$GenerationManifest,
    [Parameter(Mandatory = $true)][string]$RspRecompSource,
    [Parameter(Mandatory = $true)][string]$GraphicsSnapshot,
    [Parameter(Mandatory = $true)][string]$GraphicsCommands,
    [Parameter(Mandatory = $true)][string]$AudioSnapshot
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Require-File([string]$Path, [string]$Name) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "$Name is missing: $Path"
    }
}

function Require-Directory([string]$Path, [string]$Name) {
    if (-not (Test-Path -LiteralPath $Path -PathType Container)) {
        throw "$Name is missing: $Path"
    }
}

$SourceRoot = (Resolve-Path -LiteralPath $SourceRoot).Path
$CMakeExe = (Resolve-Path -LiteralPath $CMakeExe).Path
$PythonExe = (Resolve-Path -LiteralPath $PythonExe).Path
Require-File (Join-Path $GitBin 'git.exe') 'Git executable'
$env:PATH = "$GitBin;$env:PATH"
$gitExe = Join-Path $GitBin 'git.exe'
$SourceCommit = (& $gitExe -C $SourceRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $SourceCommit -notmatch '^[0-9a-f]{40}$') {
    throw 'source commit could not be derived from the checkout'
}
$sourceChanges = (& $gitExe -C $SourceRoot status --porcelain --untracked-files=no)
if ($LASTEXITCODE -ne 0 -or $sourceChanges) {
    throw 'source checkout has tracked changes; refusing an unbound Windows build'
}
foreach ($dependency in @('N64ModernRuntime', 'rt64')) {
    $checkout = Join-Path $SourceRoot ".local\deps\$dependency"
    Require-Directory $checkout "$dependency checkout"
    & $gitExe -C $checkout config core.filemode false
    if ($LASTEXITCODE -ne 0) { throw "$dependency Git filemode setup failed" }
    & $gitExe -C $checkout submodule foreach --recursive 'git config core.filemode false' |
        Out-Null
    if ($LASTEXITCODE -ne 0) { throw "$dependency submodule filemode setup failed" }
}
Require-Directory $GeneratedDir 'N64Recomp generated directory'
foreach ($item in @(
    @($Rom, 'authorized ROM'),
    @($GenerationManifest, 'generation manifest'),
    @($RspRecompSource, 'RSPRecomp source'),
    @($GraphicsSnapshot, 'graphics snapshot'),
    @($GraphicsCommands, 'graphics commands'),
    @($AudioSnapshot, 'audio snapshot')
)) {
    Require-File $item[0] $item[1]
}

New-Item -ItemType Directory -Force -Path $BuildRoot | Out-Null
$BuildRoot = (Resolve-Path -LiteralPath $BuildRoot).Path
$diagnostics = [ordered]@{
    schema_version = 1
    platform = 'windows-x86_64'
    source_commit = $SourceCommit
    os = (Get-CimInstance Win32_OperatingSystem | Select-Object Caption, Version, BuildNumber)
    cpu_architecture = $env:PROCESSOR_ARCHITECTURE
    gpu = @(Get-CimInstance Win32_VideoController | Select-Object Name, DriverVersion)
    audio = @(Get-CimInstance Win32_SoundDevice | Select-Object Name, Status)
    cmake = (& $CMakeExe --version | Select-Object -First 1)
    generator = 'Visual Studio 17 2022'
    architecture = 'x64'
}
$diagnostics | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (
    Join-Path $BuildRoot 'windows-build-diagnostics.json') -Encoding UTF8

$configure = @(
    '-S', $SourceRoot,
    '-B', $BuildRoot,
    '-G', 'Visual Studio 17 2022',
    '-A', 'x64',
    "-DPython3_EXECUTABLE=$PythonExe",
    "-DTETRISPHERE_ROM=$Rom",
    "-DTETRISPHERE_GENERATED_DIR=$GeneratedDir",
    "-DTETRISPHERE_GENERATION_MANIFEST=$GenerationManifest",
    "-DTETRISPHERE_RSP_RECOMP_SOURCE=$RspRecompSource",
    "-DTETRISPHERE_GRAPHICS_SNAPSHOT=$GraphicsSnapshot",
    "-DTETRISPHERE_GRAPHICS_COMMANDS=$GraphicsCommands",
    "-DTETRISPHERE_AUDIO_SNAPSHOT=$AudioSnapshot"
)
& $CMakeExe @configure
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed: $LASTEXITCODE" }

& $CMakeExe --build $BuildRoot --config Release --target tetrisphere-m1 tetrisphere-config --parallel
if ($LASTEXITCODE -ne 0) { throw "CMake build failed: $LASTEXITCODE" }

$binary = Join-Path $BuildRoot 'Release\tetrisphere-m1.exe'
Require-File $binary 'Windows M1 executable'
$configurator = Join-Path $BuildRoot 'Release\tetrisphere-config.exe'
Require-File $configurator 'Windows configurator executable'
Require-File (Join-Path (Split-Path $binary) 'SDL2.dll') 'SDL2 runtime'
Require-File (Join-Path (Split-Path $binary) 'dxcompiler.dll') 'RT64 DXC runtime'
Require-File (Join-Path (Split-Path $binary) 'dxil.dll') 'RT64 DXIL runtime'
$receipt = [ordered]@{
    schema_version = 1
    platform = 'windows-x86_64'
    source_commit = $SourceCommit
    binary = 'Release/tetrisphere-m1.exe'
    binary_sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $binary).Hash.ToLowerInvariant()
    configurator = 'Release/tetrisphere-config.exe'
    configurator_sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $configurator).Hash.ToLowerInvariant()
    runtime_sha256 = [ordered]@{
        'SDL2.dll' = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path (Split-Path $binary) 'SDL2.dll')).Hash.ToLowerInvariant()
        'dxcompiler.dll' = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path (Split-Path $binary) 'dxcompiler.dll')).Hash.ToLowerInvariant()
        'dxil.dll' = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path (Split-Path $binary) 'dxil.dll')).Hash.ToLowerInvariant()
    }
    rt64_receipt = 'rt64-verification.json'
}
$receipt | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (
    Join-Path $BuildRoot 'windows-build-receipt.json') -Encoding UTF8
$receipt | ConvertTo-Json -Compress
