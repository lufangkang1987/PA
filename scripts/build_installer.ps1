[CmdletBinding()]
param(
    [ValidatePattern('^\d+\.\d+\.\d+(\.\d+)?$')]
    [string]$Version = '1.0.0',
    [switch]$SkipBuild,
    [string]$QtBin = 'D:\Qt\6.11.1\msvc2022_64\bin',
    [string]$InnoSetupDir = 'D:\Inno Setup 6'
)

$ErrorActionPreference = 'Stop'
$root = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$stage = [System.IO.Path]::GetFullPath((Join-Path $root 'dist\PA'))
$releaseExe = Join-Path $root 'x64\Release\PA.exe'

function Find-MSBuild {
    $command = Get-Command MSBuild.exe -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        $found = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
        if ($found) { return $found }
    }

    throw 'MSBuild.exe was not found. Install the Visual Studio Desktop development with C++ workload.'
}

function Find-Tool([string]$PreferredPath, [string]$CommandName, [string]$Description) {
    if (Test-Path -LiteralPath $PreferredPath) { return $PreferredPath }
    $command = Get-Command $CommandName -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }
    throw "$Description was not found: $PreferredPath"
}

if (-not $SkipBuild) {
    $msbuild = Find-MSBuild
    $qtRoot = [System.IO.Path]::GetFullPath((Join-Path $QtBin '..'))
    Write-Host "[1/4] Building Release: $msbuild"
    & $msbuild (Join-Path $root 'PA.vcxproj') /t:Build /m /p:Configuration=Release /p:Platform=x64 "/p:QtInstall=$qtRoot"
    if ($LASTEXITCODE -ne 0) { throw "Release build failed with exit code $LASTEXITCODE" }
}

if (-not (Test-Path -LiteralPath $releaseExe)) {
    throw "Release executable was not found: $releaseExe"
}

if (-not $stage.StartsWith($root, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to clean a directory outside the workspace: $stage"
}

Write-Host "[2/4] Preparing deployment directory: $stage"
if (Test-Path -LiteralPath $stage) {
    Remove-Item -LiteralPath $stage -Recurse -Force
}
New-Item -ItemType Directory -Path $stage | Out-Null
Copy-Item -LiteralPath $releaseExe -Destination (Join-Path $stage 'PA.exe')

$windeployqt = Find-Tool (Join-Path $QtBin 'windeployqt.exe') 'windeployqt.exe' 'Qt windeployqt'
& $windeployqt --release --compiler-runtime --no-translations --dir $stage (Join-Path $stage 'PA.exe')
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed with exit code $LASTEXITCODE" }

foreach ($name in @('params', 'data', 'logs', 'screenshots')) {
    New-Item -ItemType Directory -Path (Join-Path $stage $name) -Force | Out-Null
}

$defaultParams = Join-Path $root 'x64\Debug\params\default.json'
if (Test-Path -LiteralPath $defaultParams) {
    Copy-Item -LiteralPath $defaultParams -Destination (Join-Path $stage 'params\default.json')
} else {
    Write-Warning 'Default parameter file was not found; the installer will create an empty params directory.'
}

Write-Host '[3/4] Verifying deployment files'
$required = @('PA.exe', 'Qt6Core.dll', 'Qt6Gui.dll', 'Qt6Network.dll', 'Qt6Widgets.dll', 'platforms\qwindows.dll')
foreach ($relativePath in $required) {
    if (-not (Test-Path -LiteralPath (Join-Path $stage $relativePath))) {
        throw "Required deployment file is missing: $relativePath"
    }
}

$iscc = Find-Tool (Join-Path $InnoSetupDir 'ISCC.exe') 'ISCC.exe' 'Inno Setup compiler'
Write-Host "[4/4] Compiling installer: $iscc"
& $iscc "/DMyAppVersion=$Version" (Join-Path $root 'installer\PA.iss')
if ($LASTEXITCODE -ne 0) { throw "Inno Setup failed with exit code $LASTEXITCODE" }

$installer = Join-Path $root "installer-output\PA-Setup-$Version-x64.exe"
if (-not (Test-Path -LiteralPath $installer)) {
    throw "Installer output was not found after compilation: $installer"
}

Write-Host "Installer created: $installer"
