param(
    [Parameter(Mandatory = $true)][string]$QtDir,
    [string]$BuildDir = 'build',
    [string]$OutputDir = 'dist',
    [string]$IsccPath
)

$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$build = if ([IO.Path]::IsPathRooted($BuildDir)) { $BuildDir } else { Join-Path $repo $BuildDir }
$output = [IO.Path]::GetFullPath($(if ([IO.Path]::IsPathRooted($OutputDir)) { $OutputDir } else { Join-Path $repo $OutputDir }))
$qt = (Resolve-Path -LiteralPath $QtDir).Path
$deploy = Join-Path $qt 'bin\windeployqt.exe'
if (-not (Test-Path -LiteralPath $deploy -PathType Leaf)) { throw "windeployqt was not found: $deploy" }

$versionMatch = [regex]::Match((Get-Content -LiteralPath (Join-Path $repo 'CMakeLists.txt') -Raw), 'project\(peekpass\s+VERSION\s+(\d+\.\d+\.\d+)')
if (-not $versionMatch.Success) { throw 'Could not read the application version.' }
$version = $versionMatch.Groups[1].Value
$exe = @((Join-Path $build 'peekpass.exe'), (Join-Path $build 'Release\peekpass.exe')) |
    Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
if (-not $exe) { throw 'Build the Release executable before packaging.' }

if (-not $IsccPath) {
    $IsccPath = Get-ChildItem -LiteralPath ${env:ProgramFiles(x86)}, $env:ProgramFiles, "$env:LOCALAPPDATA\Programs" -Directory -Filter 'Inno Setup *' -ErrorAction SilentlyContinue |
        Sort-Object Name -Descending | ForEach-Object { Join-Path $_.FullName 'ISCC.exe' } |
        Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
}
if (-not $IsccPath -or -not (Test-Path -LiteralPath $IsccPath -PathType Leaf)) { throw 'Inno Setup compiler was not found.' }

# Use a new staging folder on every run so older DLLs cannot enter a release.
New-Item -ItemType Directory -Force -Path $output | Out-Null
$stage = Join-Path $output ('.stage-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $stage | Out-Null
Copy-Item -LiteralPath $exe -Destination (Join-Path $stage 'peekpass.exe')
foreach ($name in @('README.md', 'LICENSE')) {
    Copy-Item -LiteralPath (Join-Path $repo $name) -Destination $stage
}
$translations = Join-Path (Split-Path -Parent $exe) 'translations'
if (-not (Test-Path -LiteralPath (Join-Path $translations 'peekpass_ko.qm'))) { throw 'Build the app translations before packaging.' }
Copy-Item -LiteralPath $translations -Destination $stage -Recurse
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'THIRD_PARTY_NOTICES.md') -Destination $stage
$licenseDirectory = Join-Path $stage 'licenses'
New-Item -ItemType Directory -Path $licenseDirectory | Out-Null
$qtLicenses = Join-Path (Split-Path -Parent (Split-Path -Parent $qt)) 'Licenses'
if (Test-Path -LiteralPath $qtLicenses -PathType Container) {
    Copy-Item -LiteralPath $qtLicenses -Destination (Join-Path $licenseDirectory 'Qt') -Recurse
}
$sbomDirectory = Join-Path $qt 'sbom'
if (Test-Path -LiteralPath $sbomDirectory -PathType Container) {
    foreach ($pattern in @('qtbase-*.spdx.json', 'qtsvg-*.spdx.json', 'qttranslations-*.spdx.json')) {
        Get-ChildItem -LiteralPath $sbomDirectory -Filter $pattern -File | Copy-Item -Destination $licenseDirectory
    }
}

$compilerMatch = [regex]::Match((Get-Content -LiteralPath (Join-Path $build 'CMakeCache.txt') -Raw), '(?m)^CMAKE_CXX_COMPILER:[^=]+=(.+)$')
$oldPath = $env:PATH
try {
    $env:PATH = (Join-Path $qt 'bin') + ';' + $env:PATH
    if ($compilerMatch.Success) { $env:PATH = (Split-Path -Parent $compilerMatch.Groups[1].Value.Trim()) + ';' + $env:PATH }
    & $deploy --release --compiler-runtime --no-system-d3d-compiler --no-opengl-sw (Join-Path $stage 'peekpass.exe')
    if ($LASTEXITCODE -ne 0) { throw "windeployqt failed: $LASTEXITCODE" }
} finally {
    $env:PATH = $oldPath
}
foreach ($name in @('Qt6Core.dll', 'Qt6Gui.dll', 'Qt6Widgets.dll', 'Qt6Network.dll', 'platforms\qwindows.dll', 'translations\peekpass_ko.qm')) {
    if (-not (Test-Path -LiteralPath (Join-Path $stage $name) -PathType Leaf)) { throw "Missing deployment file: $name" }
}

$zip = Join-Path $output "peekpass-$version-win64.zip"
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip -Force
& $IsccPath "/DAppVersion=$version" "/DStageDir=$stage" "/DInstallerOutputDir=$output" (Join-Path $PSScriptRoot 'peekpass.iss')
if ($LASTEXITCODE -ne 0) { throw "Inno Setup failed: $LASTEXITCODE" }
$installer = Join-Path $output "peekpass-$version-win64-setup.exe"
if (-not (Test-Path -LiteralPath $installer -PathType Leaf)) { throw 'Installer executable was not created.' }
Get-FileHash -LiteralPath $installer -Algorithm SHA256 | Format-List
Write-Output "Installer: $installer"
Write-Output "Portable ZIP: $zip"
Write-Output "Staging folder: $stage"
