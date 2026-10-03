$ErrorActionPreference = 'Stop'
$folder = $PSScriptRoot
$exe = Join-Path $folder 'MegaManXSNESRecomp.exe'
if (-not (Test-Path -LiteralPath $exe)) {
    throw 'Place this launcher beside MegaManXSNESRecomp.exe.'
}
$logs = Join-Path $folder 'logs'
New-Item -ItemType Directory -Path $logs -Force | Out-Null
$trace = Join-Path $logs ('coop-physics-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [Guid]::NewGuid().ToString('N').Substring(0, 8) + '.csv')
$previous = $env:MMX_COOP_DIAGNOSTICS
try {
    $env:MMX_COOP_DIAGNOSTICS = $trace
    Start-Process -FilePath $exe -ArgumentList '--launcher' -WorkingDirectory $folder
} finally {
    $env:MMX_COOP_DIAGNOSTICS = $previous
}
