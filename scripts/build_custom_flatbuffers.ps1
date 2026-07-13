[CmdletBinding()]
param(
    [string]$BuildDirectory = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = [System.IO.Path]::GetFullPath(
    (Join-Path $PSScriptRoot "..")
)
$sourceDir = Join-Path $repoRoot "ThirdParty\FlatBuffers"
$schemaDir = Join-Path $repoRoot "flatbuffers"
$runtimeSourceDir = Join-Path $sourceDir "include\flatbuffers"
$runtimeTargetDir = Join-Path $schemaDir "flatbuffers"
$promotedFlatc = Join-Path $schemaDir "flatc.exe"

if ([string]::IsNullOrWhiteSpace($BuildDirectory)) {
    $BuildDirectory = Join-Path $repoRoot "build\flatbuffers-custom"
}
$BuildDirectory = [System.IO.Path]::GetFullPath($BuildDirectory)

if (-not $BuildDirectory.StartsWith(
        $repoRoot + [System.IO.Path]::DirectorySeparatorChar,
        [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "BuildDirectory must stay inside the MMOServer repository: $BuildDirectory"
}

function Invoke-NativeChecked {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Executable,
        [Parameter(ValueFromRemainingArguments = $true)]
        [string[]]$Arguments
    )

    & $Executable @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Executable failed with exit code $LASTEXITCODE"
    }
}

function Invoke-FlatcChecked {
    param(
        [Parameter(Mandatory = $true)]
        [string]$FlatcPath,
        [Parameter(Mandatory = $true)]
        [string]$Arguments
    )

    # Windows PowerShell 5 drops an empty native argument. ProcessStartInfo
    # preserves --filename-suffix "" exactly as the existing batch files do.
    $startInfo = New-Object System.Diagnostics.ProcessStartInfo
    $startInfo.FileName = $FlatcPath
    $startInfo.Arguments = $Arguments
    $startInfo.WorkingDirectory = $repoRoot
    $startInfo.UseShellExecute = $false
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true

    $process = [System.Diagnostics.Process]::Start($startInfo)
    $stdout = $process.StandardOutput.ReadToEnd()
    $stderr = $process.StandardError.ReadToEnd()
    $process.WaitForExit()

    if ($stdout) {
        Write-Host $stdout.TrimEnd()
    }
    if ($stderr) {
        Write-Host $stderr.TrimEnd()
    }
    if ($process.ExitCode -ne 0) {
        throw "flatc failed with exit code $($process.ExitCode)"
    }
}

Write-Host "Configuring custom FlatBuffers..."
Invoke-NativeChecked -Executable cmake -Arguments @(
    '-S', $sourceDir,
    '-B', $BuildDirectory,
    '-G', 'Visual Studio 17 2022',
    '-A', 'x64',
    '-DFLATBUFFERS_BUILD_TESTS=OFF',
    '-DFLATBUFFERS_INSTALL=OFF',
    '-DFLATBUFFERS_BUILD_FLATLIB=OFF',
    '-DFLATBUFFERS_BUILD_FLATC=ON',
    '-DFLATBUFFERS_BUILD_FLATHASH=OFF',
    '-DFLATBUFFERS_BUILD_SHAREDLIB=OFF'
)

Write-Host "Building Release flatc..."
Invoke-NativeChecked -Executable cmake -Arguments @(
    '--build', $BuildDirectory,
    '--config', 'Release',
    '--target', 'flatc',
    '--parallel',
    '--',
    '/p:VcpkgApplocalDeps=false'
)

$builtFlatc = Join-Path $BuildDirectory "Release\flatc.exe"
if (-not (Test-Path -LiteralPath $builtFlatc -PathType Leaf)) {
    throw "Built flatc.exe was not found: $builtFlatc"
}

Write-Host "Synchronizing runtime headers and promoting flatc.exe..."
Copy-Item -Path (Join-Path $runtimeSourceDir "*") `
    -Destination $runtimeTargetDir `
    -Recurse `
    -Force
Copy-Item -LiteralPath $builtFlatc -Destination $promotedFlatc -Force

$cppArguments = @(
    '--cpp --cpp-std c++17',
    '--cpp-ptr-type std::shared_ptr',
    '--cpp-str-type std::string',
    '--reflect-names --gen-object-api --gen-onefile --gen-all',
    '--no-prefix --scoped-enums --no-emit-min-max-enum-values',
    '--no-warnings --filename-suffix ""',
    ('-o "{0}"' -f $schemaDir),
    '".\flatbuffers\ProtocoID.fbs"'
) -join ' '

Write-Host "Generating C++ protocol code..."
Invoke-FlatcChecked -FlatcPath $promotedFlatc -Arguments $cppArguments

$tempRoot = [System.IO.Path]::GetFullPath(
    (Join-Path $env:TEMP ("mmoserver-flatc-csharp-check-" + [guid]::NewGuid().ToString("N")))
)
$tempBase = [System.IO.Path]::GetFullPath($env:TEMP)
if (-not $tempRoot.StartsWith(
        $tempBase,
        [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Unsafe temporary path: $tempRoot"
}

New-Item -ItemType Directory -Path $tempRoot | Out-Null
try {
    $csharpArguments = @(
        '--csharp --gen-object-api --gen-onefile --gen-all',
        '--no-prefix --scoped-enums --no-warnings',
        '--filename-suffix ""',
        ('-o "{0}"' -f $tempRoot),
        '".\flatbuffers\ProtocoID.fbs"'
    ) -join ' '

    Write-Host "Verifying C# protocol generation..."
    Invoke-FlatcChecked -FlatcPath $promotedFlatc -Arguments $csharpArguments

    $generatedCSharp = @(
        Get-ChildItem -LiteralPath $tempRoot -Filter "*.cs" -File
    )
    if ($generatedCSharp.Count -eq 0) {
        throw "C# generation completed without producing a .cs file."
    }
}
finally {
    if (Test-Path -LiteralPath $tempRoot) {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force
    }
}

$flatcVersion = & $promotedFlatc --version
$flatcHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $promotedFlatc).Hash
Write-Host "Custom FlatBuffers build completed."
Write-Host "  Version: $flatcVersion"
Write-Host "  SHA-256: $flatcHash"
Write-Host "  Binary:  $promotedFlatc"
