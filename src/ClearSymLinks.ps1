<############################################################################

.SYNOPSYS
    Clear project symlinks

.USAGE
    Create a file named "includes.txt" into the sketch's folder.
    All ".cpp" and ".hpp" files will be deleted if the "includes.txt"
    file is found in the same folder.

.AUTHOR
    Ángel Fernández Pineda. Madrid. Spain. 2026.

.LICENSE
    Licensed under the EUPL

#############################################################################>

# Parameters

param (
    [Parameter(HelpMessage = "Path to the 'src' folder of this project")]
    [string]$RootPath = $null
)

# global constants
$_includesFile = "includes.txt"

# Initialize
$ErrorActionPreference = 'Stop'
$VerbosePreference = "continue"
$InformationPreference = "continue"

if ($RootPath.Length -eq 0) {
    $RootPath = Split-Path $($MyInvocation.MyCommand.Path) -Parent
    $RootPath = Split-Path $RootPath -Parent
}

<#############################################################################
# Auxiliary functions
#############################################################################>

function Find-Includes {
    param(
        [Parameter(Mandatory)]
        [string]$RootPath
    )
    $files = Get-ChildItem -Recurse -File -Path $RootPath | Where-Object { $_.Name -eq $_includesFile }
    $files | ForEach-Object { $_.FullName }
}

<#############################################################################
# MAIN
#############################################################################>

Write-Host "🛈 Root path = " -NoNewline -ForegroundColor Cyan
Write-Host $RootPath

try {
    $commonPath = Join-Path $RootPath "src/common"
    $includePath = Join-Path $RootPath "src/include"

    $spec_files = Find-Includes -RootPath $RootPath
    foreach ($specFile in $spec_files) {
        $specFolder = Split-Path $specFile

        # Avoid destructive mistakes
        if ($specFolder.Equals($commonPath) -or `
                $specFolder.Equals($includePath)) {
            continue;
        }

        Write-Host $specFolder
        $cpp = Join-Path $specFolder "*.cpp"
        $h = Join-Path $specFolder "*.h"
        $hpp = Join-Path $specFolder "*.hpp"
        Remove-Item $cpp -Force
        Remove-Item $h -Force
        Remove-Item $hpp -Force
    } # foreach ($specFile ...
}
finally {
}
