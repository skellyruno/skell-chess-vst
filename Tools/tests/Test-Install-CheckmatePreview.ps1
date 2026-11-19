$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$installer = Join-Path $PSScriptRoot '..\Install-CheckmatePreview.ps1'
$installer = [System.IO.Path]::GetFullPath($installer)
if (-not (Test-Path -LiteralPath $installer -PathType Leaf)) {
    throw "Installer script not found: $installer"
}

$script:assertions = 0
function Assert-True([bool]$Condition, [string]$Message) {
    $script:assertions++
    if (-not $Condition) {
        throw "ASSERTION FAILED: $Message"
    }
}
function Assert-Equal($Actual, $Expected, [string]$Message) {
    $script:assertions++
    if ($Actual -ne $Expected) {
        throw "ASSERTION FAILED: $Message (actual='$Actual', expected='$Expected')"
    }
}
function Assert-Fails([scriptblock]$Action, [string]$Message) {
    $script:assertions++
    $failed = $false
    try {
        & $Action
    }
    catch {
        $failed = $true
    }
    if (-not $failed) {
        throw "ASSERTION FAILED: expected failure: $Message"
    }
}

function New-TestPackage([string]$Root, [byte]$Marker) {
    New-Item -ItemType Directory -Path $Root -Force | Out-Null
    $bundle = Join-Path $Root 'Checkmate.vst3'
    $resources = Join-Path $bundle 'Contents\Resources'
    $moduleDir = Join-Path $bundle 'Contents\x86_64-win'
    New-Item -ItemType Directory -Path $resources -Force | Out-Null
    New-Item -ItemType Directory -Path $moduleDir -Force | Out-Null

    $module = Join-Path $moduleDir 'Checkmate.vst3'
    $bytes = New-Object byte[] 512
    $bytes[0] = 0x4d
    $bytes[1] = 0x5a
    [System.BitConverter]::GetBytes([int]128).CopyTo($bytes, 0x3c)
    $bytes[128] = 0x50
    $bytes[129] = 0x45
    $bytes[130] = 0
    $bytes[131] = 0
    [System.BitConverter]::GetBytes([uint16]0x8664).CopyTo($bytes, 132)
    [System.BitConverter]::GetBytes([uint16]0x20b).CopyTo($bytes, 152)
    $bytes[200] = $Marker
    [System.IO.File]::WriteAllBytes($module, $bytes)

    $metadata = [ordered]@{
        Name = 'Checkmate'
        Version = '2.4.0'
        'Factory Info' = @{ Vendor = 'Skell Audio'; URL = ''; 'E-Mail' = '' }
        Classes = @(
            @{ CID = 'ABCDEF019182FAEB536B4175436B4D74'; Category = 'Audio Module Class'; Name = 'Checkmate'; Vendor = 'Skell Audio'; Version = '2.4.0' },
            @{ CID = 'ABCDEF011234ABCD536B4175436B4D74'; Category = 'Component Controller Class'; Name = 'Checkmate'; Vendor = 'Skell Audio'; Version = '2.4.0' }
        )
    }
    $metadata | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $resources 'moduleinfo.json') -Encoding UTF8

    $moduleHash = (Get-FileHash -LiteralPath $module -Algorithm SHA256).Hash.ToLowerInvariant()
    $buildInfo = @{
        product = 'Checkmate'
        release_status = 'preview-only; not a commercial release'
        format = 'VST3'
        platform = 'Windows x86_64'
        version = '2.4.0'
        vendor = 'Skell Audio'
        plugins = @(@{ module_sha256 = $moduleHash })
    }
    $buildInfo | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $Root 'build-info.json') -Encoding UTF8
    'Checkmate unsigned engineering preview.' | Set-Content -LiteralPath (Join-Path $Root 'PREVIEW-ONLY.txt') -Encoding UTF8
    'Run Install-CheckmatePreview.ps1 from this directory.' | Set-Content -LiteralPath (Join-Path $Root 'WINDOWS-VST3-INSTALL.txt') -Encoding UTF8
    'Test fixture installer placeholder.' | Set-Content -LiteralPath (Join-Path $Root 'Install-CheckmatePreview.ps1') -Encoding UTF8

    $lines = @()
    foreach ($file in @(Get-ChildItem -LiteralPath $Root -File -Force -Recurse | Sort-Object FullName)) {
        $relative = $file.FullName.Substring($Root.TrimEnd('\').Length + 1).Replace('\', '/')
        $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        $lines += "$hash  $relative"
    }
    $lines | Set-Content -LiteralPath (Join-Path $Root 'SHA256SUMS.txt') -Encoding UTF8
    return $module
}

function Get-ModuleHash([string]$Bundle) {
    return (Get-FileHash -LiteralPath (Join-Path $Bundle 'Contents\x86_64-win\Checkmate.vst3') -Algorithm SHA256).Hash.ToLowerInvariant()
}

$work = Join-Path $env:TEMP ('CheckmateInstallerTests-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $work | Out-Null
try {
    $package1 = Join-Path $work 'package-v1'
    $package2 = Join-Path $work 'package-v2'
    $dest = Join-Path $work 'user-vst3'
    $module1 = New-TestPackage $package1 1
    $module2 = New-TestPackage $package2 2

    # WhatIf must validate the package but must not create the destination.
    $whatIf = & $installer -PackageDirectory $package1 -Vst3Directory $dest -WhatIf -Confirm:$false
    Assert-True ([bool]$whatIf.WhatIf) 'WhatIf returns a preview result'
    Assert-True (-not (Test-Path -LiteralPath $dest)) 'WhatIf does not create the destination'

    # Fresh per-user install.
    $first = & $installer -PackageDirectory $package1 -Vst3Directory $dest -Confirm:$false
    $installed = Join-Path $dest 'Checkmate.vst3'
    Assert-True (Test-Path -LiteralPath $installed -PathType Container) 'fresh install creates one VST3 bundle'
    Assert-Equal (Get-ModuleHash $installed) (Get-FileHash -LiteralPath $module1 -Algorithm SHA256).Hash.ToLowerInvariant() 'fresh install matches package module'
    Assert-True ([string]::IsNullOrWhiteSpace([string]$first.BackupPath)) 'fresh install does not invent a backup'

    # Update must preserve the previous bundle in a verified backup.
    $second = & $installer -PackageDirectory $package2 -Vst3Directory $dest -Confirm:$false
    Assert-True (-not [string]::IsNullOrWhiteSpace([string]$second.BackupPath)) 'update returns a backup path'
    Assert-True (Test-Path -LiteralPath (Join-Path $second.BackupPath 'backup-info.json')) 'update writes backup provenance'
    Assert-Equal (Get-ModuleHash $installed) (Get-FileHash -LiteralPath $module2 -Algorithm SHA256).Hash.ToLowerInvariant() 'update installs new module'
    $backupBundle = Join-Path $second.BackupPath 'Checkmate.vst3'
    Assert-Equal (Get-ModuleHash $backupBundle) (Get-FileHash -LiteralPath $module1 -Algorithm SHA256).Hash.ToLowerInvariant() 'backup preserves old module'

    # Rollback restores the selected backup and preserves the newer install.
    $rollback = & $installer -Vst3Directory $dest -RollbackFrom $second.BackupPath -Confirm:$false
    Assert-Equal (Get-ModuleHash $installed) (Get-FileHash -LiteralPath $module1 -Algorithm SHA256).Hash.ToLowerInvariant() 'rollback restores previous module'
    Assert-True (Test-Path -LiteralPath (Join-Path $second.BackupPath 'Checkmate.vst3')) 'rollback source remains available'
    Assert-True (-not [string]::IsNullOrWhiteSpace([string]$rollback.BackupPath)) 'rollback backs up the current installation before replacing it'

    # A tampered package must fail before the destination is touched.
    $tamperedPackage = Join-Path $work 'package-tampered'
    $tamperedModule = New-TestPackage $tamperedPackage 3
    [System.IO.File]::AppendAllText($tamperedModule, 'tamper')
    $tamperedDest = Join-Path $work 'tampered-destination'
    Assert-Fails { & $installer -PackageDirectory $tamperedPackage -Vst3Directory $tamperedDest -Confirm:$false } 'tampered package checksum'
    Assert-True (-not (Test-Path -LiteralPath $tamperedDest)) 'tampered package leaves destination untouched'

    # A junction at the destination must be rejected without following it.
    $realDest = Join-Path $work 'real-destination'
    $junctionDest = Join-Path $work 'junction-destination'
    New-Item -ItemType Directory -Path $realDest | Out-Null
    New-Item -ItemType Junction -Path $junctionDest -Target $realDest | Out-Null
    Assert-Fails { & $installer -PackageDirectory $package1 -Vst3Directory $junctionDest -Confirm:$false } 'destination junction'
    Assert-True (-not (Test-Path -LiteralPath (Join-Path $realDest 'Checkmate.vst3'))) 'junction target remains untouched'

    # Missing LOCALAPPDATA must not silently fall back to Program Files.
    $savedLocalAppData = $env:LOCALAPPDATA
    try {
        $env:LOCALAPPDATA = $null
        Assert-Fails { & $installer -PackageDirectory $package1 -Confirm:$false } 'missing LOCALAPPDATA'
    }
    finally {
        $env:LOCALAPPDATA = $savedLocalAppData
    }

    Write-Output ("PASS Checkmate installer integration: {0}/{0} assertions" -f $script:assertions)
}
finally {
    if (Test-Path -LiteralPath $work) {
        Remove-Item -LiteralPath $work -Recurse -Force
    }
}
