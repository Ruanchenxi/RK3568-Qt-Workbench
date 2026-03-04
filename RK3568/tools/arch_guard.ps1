param(
    [int]$Phase = 1
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$srcRoot = Join-Path $repoRoot "src"

$violations = @()
$warnings = @()

function Add-Violation([string]$msg) {
    $script:violations += $msg
}

function Add-Warning([string]$msg) {
    $script:warnings += $msg
}

$uiFiles = Get-ChildItem -Path $srcRoot -Recurse -File | Where-Object {
    $_.Name -match "(.*page.*|mainwindow)\.(h|cpp)$" -and $_.FullName -notmatch "[\\/]+_archive[\\/]+"
}
$protocolFiles = Get-ChildItem -Path $srcRoot -Recurse -File | Where-Object {
    $_.Extension -in @(".h", ".hpp", ".cpp") -and
    $_.FullName -match "[\\/]+protocol[\\/]+" -and
    $_.FullName -notmatch "[\\/]+_archive[\\/]+"
}
$sourceFiles = Get-ChildItem -Path $srcRoot -Recurse -File | Where-Object {
    $_.Extension -in @(".h", ".hpp", ".cpp") -and $_.FullName -notmatch "[\\/]+_archive[\\/]+"
}

$forbiddenRegexRules = @(
    @{ Name = "QSerialPort include"; Pattern = "#include\s*<QSerialPort>" },
    @{ Name = "QSerialPortInfo in UI layer"; Pattern = "#include\s*<QSerialPortInfo>" },
    @{ Name = "QNetworkAccessManager include"; Pattern = "#include\s*<QNetworkAccessManager>" },
    @{ Name = "QProcess include"; Pattern = "#include\s*<QProcess>" },
    @{ Name = "core/AppContext include"; Pattern = "#include\s*""core/AppContext\.h""" },
    @{ Name = "AppContext singleton access"; Pattern = "AppContext::instance\s*\(" },
    @{ Name = "core/authservice include"; Pattern = "#include\s*""core/authservice\.h""" },
    @{ Name = "auth feature authservice include"; Pattern = "#include\s*""features/auth/infra/password/authservice\.h""" },
    @{ Name = "AuthService singleton access"; Pattern = "AuthService::instance\s*\(" }
)

foreach ($file in $uiFiles) {
    $text = Get-Content -Path $file.FullName -Raw -Encoding UTF8
    foreach ($rule in $forbiddenRegexRules) {
        if ($text -match $rule.Pattern) {
            Add-Violation("$($file.FullName): forbidden pattern '$($rule.Name)'")
        }
    }
}

$legacyIncludeRules = @(
    @{ Name = "legacy services include path"; Pattern = "#include\s*""services/" },
    @{ Name = "legacy controllers include path"; Pattern = "#include\s*""controllers/" },
    @{ Name = "legacy protocol include path"; Pattern = "#include\s*""protocol/" },
    @{ Name = "legacy contracts include path"; Pattern = "#include\s*""contracts/" },
    @{ Name = "legacy infra include path"; Pattern = "#include\s*""infra/" }
)

foreach ($file in $sourceFiles) {
    $text = Get-Content -Path $file.FullName -Raw -Encoding UTF8
    foreach ($rule in $legacyIncludeRules) {
        if ($text -match $rule.Pattern) {
            Add-Violation("$($file.FullName): forbidden pattern '$($rule.Name)'")
        }
    }
}

$protocolForbiddenRules = @(
    @{ Name = "protocol should not include QtSerialTransport"; Pattern = "#include\s*""(infra|platform)/serial/QtSerialTransport\.h""" },
    @{ Name = "protocol should not include ReplaySerialTransport"; Pattern = "#include\s*""(infra|platform)/serial/ReplaySerialTransport\.h""" }
)

foreach ($file in $protocolFiles) {
    $text = Get-Content -Path $file.FullName -Raw -Encoding UTF8
    foreach ($rule in $protocolForbiddenRules) {
        if ($text -match $rule.Pattern) {
            Add-Violation("$($file.FullName): forbidden pattern '$($rule.Name)'")
        }
    }
}

$keyManageCppFiles = Get-ChildItem -Path $srcRoot -Recurse -File | Where-Object {
    $_.Name -ieq "keymanagepage.cpp"
}
foreach ($keyManageCpp in $keyManageCppFiles) {
    $kmText = Get-Content -Path $keyManageCpp.FullName -Raw -Encoding UTF8
    if ($kmText -match "KeySerialClient") {
        if ($Phase -ge 3) {
            Add-Violation("$($keyManageCpp.FullName): contains 'KeySerialClient' in phase >= 3")
        } else {
            Add-Warning("$($keyManageCpp.FullName): contains 'KeySerialClient' (phase 1~2 warning)")
        }
    }
}

Write-Host "=== Architecture Guard Report ==="
Write-Host "Phase: $Phase"

if ($warnings.Count -gt 0) {
    Write-Host "-- Warnings --"
    foreach ($w in $warnings) {
        Write-Host $w
    }
}

if ($violations.Count -gt 0) {
    Write-Host "-- Violations --"
    foreach ($v in $violations) {
        Write-Host $v
    }
    exit 1
}

Write-Host "No violations."
exit 0
