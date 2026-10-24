[CmdletBinding()]
param(
    [string]$DeployRoot = (Join-Path $PSScriptRoot 'dist\HeelsFixNG-NoESP-SE1597'),
    [switch]$SkipBuild
)
$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $PSScriptRoot

function Invoke-Checked([string]$Command, [string[]]$Arguments, [string]$Failure) {
    & $Command @Arguments
    if ($LASTEXITCODE -ne 0) { throw $Failure }
}

python tools/verify_source.py
if ($LASTEXITCODE -ne 0) { throw 'Source preflight failed.' }
if (-not (Get-Command git -ErrorAction SilentlyContinue)) { throw 'Git is required.' }
if (-not (Get-Command xmake -ErrorAction SilentlyContinue)) { throw 'xmake 3.0.0 is required.' }

New-Item -ItemType Directory -Force -Path (Join-Path $PSScriptRoot 'lib') | Out-Null
$commonlib = Join-Path $PSScriptRoot 'lib\CommonLibSSE-NG'
$menuApi = Join-Path $PSScriptRoot 'lib\SKSE-Menu-Framework-3-API'
if (-not (Test-Path -LiteralPath (Join-Path $commonlib 'xmake.lua'))) {
    if (Test-Path -LiteralPath $commonlib) { Remove-Item -LiteralPath $commonlib -Recurse -Force }
    Invoke-Checked 'git' @('clone','--depth','1','--branch','ng','--recurse-submodules','https://github.com/alandtse/CommonLibVR.git',$commonlib) 'CommonLibSSE-NG checkout failed.'
}
if (-not (Test-Path -LiteralPath (Join-Path $menuApi 'SKSEMenuFramework.h'))) {
    if (Test-Path -LiteralPath $menuApi) { Remove-Item -LiteralPath $menuApi -Recurse -Force }
    Invoke-Checked 'git' @('clone','--depth','1','https://github.com/QTR-Modding/SKSE-Menu-Framework-3-API.git',$menuApi) 'SKSE Menu Framework API checkout failed.'
}

# Keep these CommonLib prebuilt-ABI flags aligned with .github/workflows/build.yml.
xmake f -m releasedbg --skyrim_se=y --skyrim_ae=y --skyrim_vr=y --rex_json=n --rex_toml=n -y
if ($LASTEXITCODE -ne 0) { throw 'xmake configuration failed.' }
python tools/verify_source.py
if ($LASTEXITCODE -ne 0) { throw 'Source preflight failed after xmake configure.' }
if ($SkipBuild) { Write-Host 'Source and xmake configuration checks passed; C++ targets were not built.'; exit 0 }

Invoke-Checked 'xmake' @('build','InteractionStateTests') 'InteractionStateTests compilation failed.'
python tools/verify_source.py
if ($LASTEXITCODE -ne 0) { throw 'Source preflight failed after test compilation.' }
Invoke-Checked 'xmake' @('run','InteractionStateTests') 'InteractionStateTests failed.'
python tools/verify_source.py
if ($LASTEXITCODE -ne 0) { throw 'Source preflight failed after regression tests.' }

if (Test-Path -LiteralPath $DeployRoot) { Remove-Item -LiteralPath $DeployRoot -Recurse -Force }
$env:HFN_DEPLOY_DIR = [System.IO.Path]::GetFullPath($DeployRoot)
Invoke-Checked 'xmake' @('build','-r','HeelsFixNative') 'HeelsFixNative.dll compilation failed.'
python tools/verify_source.py
if ($LASTEXITCODE -ne 0) { throw 'Final source preflight failed after plugin compilation.' }
$pluginDir = Join-Path $DeployRoot 'SKSE\Plugins'
$dllPath = Join-Path $pluginDir 'HeelsFixNative.dll'
if (-not (Test-Path -LiteralPath $dllPath)) { throw 'Build reported success but the HFN_DEPLOY_DIR DLL was not created.' }
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'Runtime\HeelsFixNative.ini') -Destination (Join-Path $pluginDir 'HeelsFixNative.ini') -Force
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'README_INSTALL_KO.txt') -Destination $DeployRoot -Force
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'LICENSE-MIT.txt') -Destination $DeployRoot -Force
if (Get-ChildItem -Path $DeployRoot -Recurse -File | Where-Object { $_.Extension.ToLowerInvariant() -in @('.esp','.esm','.esl','.pex') }) { throw 'Forbidden plugin or Papyrus payload found in package.' }
$zipPath = Join-Path (Split-Path -Parent $DeployRoot) 'HeelsFixNG-NoESP-SE1597.zip'
if (Test-Path -LiteralPath $zipPath) { Remove-Item -LiteralPath $zipPath -Force }
Compress-Archive -Path (Join-Path $DeployRoot '*') -DestinationPath $zipPath -Force
python tools/verify_source.py
if ($LASTEXITCODE -ne 0) { throw 'Final source preflight failed; do not distribute the ZIP.' }
Write-Host "Build, tests, and final static checks passed. Candidate package: $zipPath" -ForegroundColor Green
