[CmdletBinding(SupportsShouldProcess = $true, ConfirmImpact = 'Medium')]
param(
    [string]$PackageDirectory = $PSScriptRoot,
    [string]$Vst3Directory,
    [string]$RollbackFrom
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-FullPath([string]$Path) {
    if ([string]::IsNullOrWhiteSpace($Path)) {
        throw 'A required path is empty.'
    }
    return [System.IO.Path]::GetFullPath($Path)
}

function Assert-NoReparsePoint([string]$Path) {
    $full = Get-FullPath $Path
    $root = [System.IO.Path]::GetPathRoot($full)
    if ([string]::IsNullOrWhiteSpace($root)) {
        throw "Cannot resolve path root: $Path"
    }
    $cursor = $root
    $relative = $full.Substring($root.Length)
    $segments = @($relative -split '[\\/]' | Where-Object { $_ -ne '' })
    foreach ($segment in $segments) {
        $cursor = Join-Path $cursor $segment
        if (Test-Path -LiteralPath $cursor) {
            $item = Get-Item -LiteralPath $cursor -Force
            if (($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw "Reparse point or junction is not allowed in a release path: $cursor"
            }
        }
    }
}

function Assert-NoReparseTree([string]$Path) {
    Assert-NoReparsePoint $Path
    if (-not (Test-Path -LiteralPath $Path -PathType Container)) {
        throw "Expected a directory: $Path"
    }
    foreach ($item in @(Get-ChildItem -LiteralPath $Path -Force -Recurse -ErrorAction Stop)) {
        if (($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "Reparse point or junction found inside release tree: $($item.FullName)"
        }
    }
}

function Get-TreeManifest([string]$Root) {
    Assert-NoReparseTree $Root
    $base = Get-FullPath $Root
    $result = @()
    foreach ($file in @(Get-ChildItem -LiteralPath $Root -File -Force -Recurse -ErrorAction Stop | Sort-Object FullName)) {
        if (($file.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "Reparse point is not allowed: $($file.FullName)"
        }
        $relative = [System.IO.Path]::GetRelativePath($base, $file.FullName).Replace('\', '/')
        $result += [pscustomobject]@{
            path = $relative
            sha256 = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        }
    }
    return $result
}

function Test-Vst3Bundle([string]$BundlePath) {
    Assert-NoReparseTree $BundlePath
    if ((Split-Path -Leaf $BundlePath) -ne 'Checkmate.vst3') {
        throw "Expected the Checkmate.vst3 bundle: $BundlePath"
    }
    $contents = Join-Path $BundlePath 'Contents'
    $metadataPath = Join-Path $contents 'Resources/moduleinfo.json'
    $windowsDir = Join-Path $contents 'x86_64-win'
    $modulePath = Join-Path $windowsDir 'Checkmate.vst3'
    if (-not (Test-Path -LiteralPath $metadataPath -PathType Leaf)) {
        throw 'VST3 moduleinfo.json is missing.'
    }
    if (-not (Test-Path -LiteralPath $modulePath -PathType Leaf)) {
        throw 'Windows x86_64 VST3 module is missing.'
    }
    $modules = @(Get-ChildItem -LiteralPath $contents -Filter '*.vst3' -File -Force -Recurse)
    if ($modules.Count -ne 1 -or $modules[0].FullName -ine (Get-FullPath $modulePath)) {
        throw 'The bundle must contain exactly one VST3 module at Contents/x86_64-win/Checkmate.vst3.'
    }

    $bytes = [System.IO.File]::ReadAllBytes($modulePath)
    if ($bytes.Length -lt 160 -or $bytes[0] -ne 0x4d -or $bytes[1] -ne 0x5a) {
        throw 'The VST3 module does not contain a valid DOS MZ header.'
    }
    $peOffset = [System.BitConverter]::ToInt32($bytes, 0x3c)
    if ($peOffset -lt 64 -or ($peOffset + 26) -gt $bytes.Length) {
        throw 'The VST3 module has an invalid PE header offset.'
    }
    if ($bytes[$peOffset] -ne 0x50 -or $bytes[$peOffset + 1] -ne 0x45 -or
        $bytes[$peOffset + 2] -ne 0 -or $bytes[$peOffset + 3] -ne 0) {
        throw 'The VST3 module is missing the PE signature.'
    }
    $machine = [System.BitConverter]::ToUInt16($bytes, $peOffset + 4)
    $optionalMagic = [System.BitConverter]::ToUInt16($bytes, $peOffset + 24)
    if ($machine -ne 0x8664 -or $optionalMagic -ne 0x20b) {
        throw ('Expected AMD64 PE32+ VST3 module; found machine 0x{0:x4}, optional header 0x{1:x4}.' -f $machine, $optionalMagic)
    }

    try {
        $metadata = Get-Content -LiteralPath $metadataPath -Raw -Encoding UTF8 | ConvertFrom-Json -ErrorAction Stop
    }
    catch {
        throw "VST3 moduleinfo.json is invalid: $($_.Exception.Message)"
    }
    $factory = $metadata.'Factory Info'
    $classes = @($metadata.Classes)
    if ($metadata.Name -ne 'Checkmate' -or [string]::IsNullOrWhiteSpace([string]$metadata.Version) -or
        $null -eq $factory -or $factory.Vendor -ne 'Skell Audio' -or $classes.Count -ne 2) {
        throw 'VST3 module identity or processor/controller class inventory is invalid.'
    }
    $categories = @()
    $ids = @()
    foreach ($class in $classes) {
        if ($class.Name -ne 'Checkmate' -or $class.Vendor -ne 'Skell Audio' -or
            $class.Version -ne $metadata.Version -or [string]$class.CID -notmatch '^[0-9A-Fa-f]{32}$') {
            throw 'VST3 class name, vendor, version or class ID is invalid.'
        }
        $categories += [string]$class.Category
        $ids += ([string]$class.CID).ToUpperInvariant()
    }
    if (($categories -notcontains 'Audio Module Class') -or
        ($categories -notcontains 'Component Controller Class') -or
        (@($ids | Select-Object -Unique).Count -ne 2)) {
        throw 'VST3 processor/controller categories or unique class IDs are invalid.'
    }
    return [pscustomobject]@{
        Version = [string]$metadata.Version
        ModulePath = $modulePath
        ModuleSha256 = (Get-FileHash -LiteralPath $modulePath -Algorithm SHA256).Hash.ToLowerInvariant()
    }
}

function Test-Package([string]$Root) {
    Assert-NoReparseTree $Root
    $manifestPath = Join-Path $Root 'SHA256SUMS.txt'
    $buildInfoPath = Join-Path $Root 'build-info.json'
    $previewPath = Join-Path $Root 'PREVIEW-ONLY.txt'
    $requiredRootFiles = @('build-info.json', 'PREVIEW-ONLY.txt', 'WINDOWS-VST3-INSTALL.txt', 'Install-CheckmatePreview.ps1', 'SHA256SUMS.txt')
    foreach ($required in $requiredRootFiles) {
        if (-not (Test-Path -LiteralPath (Join-Path $Root $required) -PathType Leaf)) {
            throw "Package is missing required file: $required"
        }
    }
    $rootItems = @(Get-ChildItem -LiteralPath $Root -Force | Select-Object -ExpandProperty Name)
    $expectedRootItems = @('Checkmate.vst3') + $requiredRootFiles
    if ($rootItems.Count -ne $expectedRootItems.Count -or
        @($rootItems | Where-Object { $_ -notin $expectedRootItems }).Count -ne 0) {
        throw 'Package root contains an unexpected file or directory.'
    }
    if ((Get-Content -LiteralPath $previewPath -Raw) -notmatch 'preview') {
        throw 'Package is not marked as a preview.'
    }

    $expected = @{}
    foreach ($line in @(Get-Content -LiteralPath $manifestPath)) {
        if ($line -notmatch '^([0-9a-fA-F]{64})  (.+)$') {
            throw "Invalid SHA256SUMS.txt line: $line"
        }
        $digest = $Matches[1].ToLowerInvariant()
        $relative = $Matches[2].Replace('\', '/')
        if ([System.IO.Path]::IsPathRooted($relative) -or
            $relative -match '(^|/)\.\.?(/|$)' -or $relative.Contains(':')) {
            throw "Unsafe package manifest path: $relative"
        }
        $key = $relative.ToLowerInvariant()
        if ($expected.ContainsKey($key)) {
            throw "Duplicate package manifest path: $relative"
        }
        $rootFull = Get-FullPath $Root
        $full = Get-FullPath (Join-Path $rootFull ($relative.Replace('/', [System.IO.Path]::DirectorySeparatorChar)))
        $resolvedRelative = [System.IO.Path]::GetRelativePath($rootFull, $full)
        $parentPrefix = '..' + [System.IO.Path]::DirectorySeparatorChar
        $altParentPrefix = '..' + [System.IO.Path]::AltDirectorySeparatorChar
        if ([System.IO.Path]::IsPathRooted($resolvedRelative) -or $resolvedRelative -eq '..' -or
            $resolvedRelative.StartsWith($parentPrefix) -or $resolvedRelative.StartsWith($altParentPrefix)) {
            throw "Package manifest path escapes the package root: $relative"
        }
        Assert-NoReparsePoint $full
        if (-not (Test-Path -LiteralPath $full -PathType Leaf)) {
            throw "Package manifest entry is missing: $relative"
        }
        $actualHash = (Get-FileHash -LiteralPath $full -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($actualHash -ne $digest) {
            throw "Package checksum mismatch: $relative"
        }
        $expected[$key] = $digest
    }
    $actualFiles = @(Get-ChildItem -LiteralPath $Root -File -Force -Recurse | Where-Object {
        $_.FullName -ine (Get-FullPath $manifestPath)
    })
    if ($actualFiles.Count -ne $expected.Count) {
        throw 'Package contains files that are not covered by SHA256SUMS.txt, or the manifest contains extra entries.'
    }
    foreach ($file in $actualFiles) {
        $relative = [System.IO.Path]::GetRelativePath((Get-FullPath $Root), $file.FullName).Replace('\', '/').ToLowerInvariant()
        if (-not $expected.ContainsKey($relative)) {
            throw "Unmanifested package file: $relative"
        }
    }

    $bundleDirs = @(Get-ChildItem -LiteralPath $Root -Directory -Filter '*.vst3' -Force)
    if ($bundleDirs.Count -ne 1 -or $bundleDirs[0].Name -ne 'Checkmate.vst3') {
        throw 'Package must contain exactly one top-level Checkmate.vst3 bundle.'
    }
    if (@(Get-ChildItem -LiteralPath $Root -File -Filter '*.exe' -Force -Recurse).Count -ne 0) {
        throw 'Standalone executables are not allowed in the Windows VST3 preview.'
    }
    $bundleInfo = Test-Vst3Bundle $bundleDirs[0].FullName
    try {
        $buildInfo = Get-Content -LiteralPath $buildInfoPath -Raw -Encoding UTF8 | ConvertFrom-Json -ErrorAction Stop
    }
    catch {
        throw "build-info.json is invalid: $($_.Exception.Message)"
    }
    if ($buildInfo.product -ne 'Checkmate' -or $buildInfo.release_status -ne 'preview-only; not a commercial release' -or
        $buildInfo.format -ne 'VST3' -or $buildInfo.platform -ne 'Windows x86_64' -or
        $buildInfo.vendor -ne 'Skell Audio' -or $buildInfo.version -ne $bundleInfo.Version -or
        @($buildInfo.plugins).Count -ne 1 -or $buildInfo.plugins[0].module_sha256 -ne $bundleInfo.ModuleSha256) {
        throw 'build-info.json does not match the staged Checkmate VST3 module.'
    }
    return $bundleDirs[0].FullName
}

function Test-BackupDirectory([string]$BackupPath) {
    Assert-NoReparseTree $BackupPath
    $bundlePath = Join-Path $BackupPath 'Checkmate.vst3'
    $infoPath = Join-Path $BackupPath 'backup-info.json'
    if (-not (Test-Path -LiteralPath $bundlePath -PathType Container) -or
        -not (Test-Path -LiteralPath $infoPath -PathType Leaf)) {
        throw 'Rollback source is not a verified Checkmate backup directory.'
    }
    $info = Get-Content -LiteralPath $infoPath -Raw -Encoding UTF8 | ConvertFrom-Json -ErrorAction Stop
    if ($info.product -ne 'Checkmate') {
        throw 'Backup product identity is invalid.'
    }
    $actual = @(Get-TreeManifest $bundlePath)
    $expected = @($info.files)
    if ($actual.Count -ne $expected.Count) {
        throw 'Backup file inventory does not match backup-info.json.'
    }
    foreach ($entry in $expected) {
        $match = @($actual | Where-Object { $_.path -ieq [string]$entry.path })
        if ($match.Count -ne 1 -or $match[0].sha256 -ne [string]$entry.sha256) {
            throw "Backup checksum mismatch: $($entry.path)"
        }
    }
    $bundleInfo = Test-Vst3Bundle $bundlePath
    if ($bundleInfo.Version -ne [string]$info.version) {
        throw 'Backup VST3 version does not match backup-info.json.'
    }
    $top = @(Get-ChildItem -LiteralPath $BackupPath -Force)
    if ($top.Count -ne 2) {
        throw 'Backup directory contains unexpected files.'
    }
    return $bundlePath
}

function New-VerifiedBackup([string]$InstalledBundle, [string]$BackupRoot) {
    Assert-NoReparseTree $InstalledBundle
    Assert-NoReparsePoint $BackupRoot
    if (-not (Test-Path -LiteralPath $BackupRoot -PathType Container)) {
        New-Item -ItemType Directory -Path $BackupRoot -Force | Out-Null
    }
    Assert-NoReparsePoint $BackupRoot
    $name = 'Checkmate-{0}-{1}' -f (Get-Date -Format 'yyyyMMdd-HHmmssfff'), ([guid]::NewGuid().ToString('N'))
    $backupPath = Join-Path $BackupRoot $name
    New-Item -ItemType Directory -Path $backupPath | Out-Null
    try {
        $backupBundle = Join-Path $backupPath 'Checkmate.vst3'
        Copy-Item -LiteralPath $InstalledBundle -Destination $backupBundle -Recurse
        $files = @(Get-TreeManifest $backupBundle)
        $bundleInfo = Test-Vst3Bundle $backupBundle
        $info = [pscustomobject]@{
            product = 'Checkmate'
            version = $bundleInfo.Version
            created_utc = (Get-Date).ToUniversalTime().ToString('o')
            files = $files
        }
        $info | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $backupPath 'backup-info.json') -Encoding UTF8
        [void](Test-BackupDirectory $backupPath)
        return $backupPath
    }
    catch {
        if (Test-Path -LiteralPath $backupPath) {
            Remove-Item -LiteralPath $backupPath -Recurse -Force
        }
        throw
    }
}

function Resolve-InstallRoot([string]$RequestedPath) {
    if ([string]::IsNullOrWhiteSpace($RequestedPath)) {
        if ([string]::IsNullOrWhiteSpace($env:LOCALAPPDATA)) {
            throw 'LOCALAPPDATA is unavailable. Set -Vst3Directory explicitly; the installer will not fall back to a machine-wide path.'
        }
        $RequestedPath = Join-Path $env:LOCALAPPDATA 'Programs\Common\VST3'
    }
    $full = Get-FullPath $RequestedPath
    Assert-NoReparsePoint $full
    return $full
}

function Invoke-Install([string]$SourceBundle, [string]$TargetRoot) {
    Assert-NoReparsePoint $TargetRoot
    if (-not (Test-Path -LiteralPath $TargetRoot -PathType Container)) {
        New-Item -ItemType Directory -Path $TargetRoot -Force | Out-Null
    }
    Assert-NoReparsePoint $TargetRoot
    $targetBundle = Join-Path $TargetRoot 'Checkmate.vst3'
    $backupRoot = Join-Path (Split-Path -Parent $TargetRoot) '.skell-backups/Checkmate'
    $backupPath = $null
    $stageRoot = Join-Path $TargetRoot ('.Checkmate-stage-' + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $stageRoot | Out-Null
    try {
        $stageBundle = Join-Path $stageRoot 'Checkmate.vst3'
        Copy-Item -LiteralPath $SourceBundle -Destination $stageBundle -Recurse
        [void](Test-Vst3Bundle $stageBundle)
        if (Test-Path -LiteralPath $targetBundle) {
            $targetItem = Get-Item -LiteralPath $targetBundle -Force
            if (-not $targetItem.PSIsContainer) {
                throw 'The existing Checkmate.vst3 destination is not a directory; it was left untouched.'
            }
            $backupPath = New-VerifiedBackup $targetBundle $backupRoot
        }
        try {
            if (Test-Path -LiteralPath $targetBundle) {
                Remove-Item -LiteralPath $targetBundle -Recurse -Force
            }
            Move-Item -LiteralPath $stageBundle -Destination $targetBundle
            [void](Test-Vst3Bundle $targetBundle)
        }
        catch {
            if (Test-Path -LiteralPath $targetBundle) {
                Remove-Item -LiteralPath $targetBundle -Recurse -Force
            }
            if ($backupPath -and (Test-Path -LiteralPath (Join-Path $backupPath 'Checkmate.vst3'))) {
                Copy-Item -LiteralPath (Join-Path $backupPath 'Checkmate.vst3') -Destination $targetBundle -Recurse
            }
            throw
        }
    }
    finally {
        if (Test-Path -LiteralPath $stageRoot) {
            Remove-Item -LiteralPath $stageRoot -Recurse -Force
        }
    }
    return [pscustomobject]@{
        Product = 'Checkmate'
        InstalledPath = $targetBundle
        BackupPath = $backupPath
        RollbackSourcePreserved = [bool]$RollbackFrom
    }
}

$targetRoot = Resolve-InstallRoot $Vst3Directory
$sourceBundle = $null
if (-not [string]::IsNullOrWhiteSpace($RollbackFrom)) {
    $sourceBundle = Test-BackupDirectory (Get-FullPath $RollbackFrom)
}
else {
    $sourceBundle = Test-Package (Get-FullPath $PackageDirectory)
}
$targetBundlePath = Join-Path $targetRoot 'Checkmate.vst3'
if (-not $RollbackFrom) {
    $packageRoot = Get-FullPath $PackageDirectory
    $packageFromTarget = [System.IO.Path]::GetRelativePath($targetRoot, $packageRoot)
    $targetFromPackage = [System.IO.Path]::GetRelativePath($packageRoot, $targetRoot)
    $separator = [System.IO.Path]::DirectorySeparatorChar
    $altSeparator = [System.IO.Path]::AltDirectorySeparatorChar
    $packageIsInsideTarget = -not [System.IO.Path]::IsPathRooted($packageFromTarget) -and
        $packageFromTarget -ne '.' -and $packageFromTarget -ne '..' -and
        -not $packageFromTarget.StartsWith('..' + $separator) -and -not $packageFromTarget.StartsWith('..' + $altSeparator)
    $targetIsInsidePackage = -not [System.IO.Path]::IsPathRooted($targetFromPackage) -and
        $targetFromPackage -ne '.' -and $targetFromPackage -ne '..' -and
        -not $targetFromPackage.StartsWith('..' + $separator) -and -not $targetFromPackage.StartsWith('..' + $altSeparator)
    if ($packageRoot -ieq $targetRoot -or $packageIsInsideTarget -or $targetIsInsidePackage) {
        throw 'Package and install directories must not overlap; the preview source must remain untouched.'
    }
}
$operation = if ($RollbackFrom) { 'Restore a verified Checkmate backup' } else { 'Install or update the Checkmate VST3 preview' }
if ($PSCmdlet.ShouldProcess($targetBundlePath, $operation)) {
    $result = Invoke-Install $sourceBundle $targetRoot
    Write-Output $result
}
else {
    Write-Output ([pscustomobject]@{
        Product = 'Checkmate'
        WhatIf = $true
        TargetPath = $targetBundlePath
        NoFilesChanged = $true
    })
}
