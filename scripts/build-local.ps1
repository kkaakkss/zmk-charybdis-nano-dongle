param(
    [Parameter(Mandatory=$true)][string]$Workspace,
    [Parameter(Mandatory=$true)][string]$Python,
    [Parameter(Mandatory=$true)][string]$Sdk,
    [ValidateRange(0,5)][int]$Target,
    [string]$Manifest = 'build.yaml',
    [switch]$Pristine
)
$ErrorActionPreference = 'Stop'
$pythonPath = (Resolve-Path -LiteralPath $Python).Path
$sdkPath = (Resolve-Path -LiteralPath $Sdk).Path.Replace('\','/')
$workspacePath = (Resolve-Path -LiteralPath $Workspace).Path
if ($workspacePath -match '\s') { throw 'Use a workspace path without spaces; Zephyr 3.5 splits overlay paths at spaces.' }
$repoPath = Split-Path -Parent $PSScriptRoot
$sourcePath = $repoPath
if ($repoPath -match '\s') {
    # NTFS junctions do not require the symlink privilege. The source stays in place.
    $sourcePath = Join-Path $workspacePath 'nano35-source'
    if (Test-Path -LiteralPath $sourcePath) {
        $sourceAlias = Get-Item -LiteralPath $sourcePath
        if ($sourceAlias.LinkType -ne 'Junction' -or $sourceAlias.Target -ne $repoPath) {
            throw "Source alias already exists with a different target: $sourcePath"
        }
    } else {
        New-Item -ItemType Junction -Path $sourcePath -Target $repoPath | Out-Null
    }
}
$previousPath = $env:PATH
$previousSdk = $env:ZEPHYR_SDK_INSTALL_DIR
$previousParallel = $env:CMAKE_BUILD_PARALLEL_LEVEL
try {
    $env:PATH = (Split-Path -Parent $pythonPath) + ';' + $env:PATH
    $env:ZEPHYR_SDK_INSTALL_DIR = $sdkPath
    $env:CMAKE_BUILD_PARALLEL_LEVEL = '4'
    $buildArgs = @((Join-Path $PSScriptRoot 'build.py'), '--workspace', $workspacePath, '--source-dir', $sourcePath)
    if ($PSBoundParameters.ContainsKey('Target')) { $buildArgs += @('--target', "$Target") }
    $buildArgs += @('--manifest', $Manifest)
    if ($Pristine) { $buildArgs += '--pristine' }
    & $pythonPath @buildArgs
    if ($LASTEXITCODE -ne 0) { throw "Firmware build failed: $LASTEXITCODE" }
} finally {
    $env:PATH = $previousPath
    $env:ZEPHYR_SDK_INSTALL_DIR = $previousSdk
    $env:CMAKE_BUILD_PARALLEL_LEVEL = $previousParallel
}
