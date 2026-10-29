param([string]$QtBin = '', [string]$CompilerBin = '')
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

# Windows PowerShell 5.1 / VS Code. No Qt Creator is required.
# Optional explicit kit: .\build-run.ps1 -QtBin C:\Qt\6.x\mingw_64\bin -CompilerBin C:\Qt\Tools\mingw...\bin
function Find-QMake {
    $candidates = @()
    if ($QtBin) {
        $candidates += (Join-Path $QtBin 'qmake6.exe')
        $candidates += (Join-Path $QtBin 'qmake.exe')
    } else {
        foreach ($bin in @('C:\msys64\ucrt64\bin','C:\msys64\mingw64\bin')) {
            $candidates += (Join-Path $bin 'qmake6.exe')
            $candidates += (Join-Path $bin 'qmake.exe')
        }
        foreach ($name in @('qmake6.exe','qmake.exe')) {
            $cmd = Get-Command $name -ErrorAction SilentlyContinue
            if ($cmd) { $candidates += $cmd.Source }
        }
        if (Test-Path 'C:\Qt') {
            $candidates += @(Get-ChildItem 'C:\Qt' -Filter 'qmake*.exe' -Recurse -ErrorAction SilentlyContinue | Select-Object -ExpandProperty FullName)
        }
    }
    foreach ($candidate in ($candidates | Select-Object -Unique)) {
        if (!(Test-Path $candidate -PathType Leaf)) { continue }
        $spec = & $candidate -query QMAKE_XSPEC 2>$null
        if ($LASTEXITCODE -eq 0 -and "$spec" -match 'win32-g\+\+') { return $candidate }
    }
    throw 'Qt for MinGW was not found. Install/select a Qt MinGW kit, or run build-run.ps1 -QtBin <Qt-bin-folder>. Send this message if you need help. Plain g++ cannot build Qt without Qt headers and libraries.'
}

$originalPath = $env:PATH
$originalPlugins = $env:QT_QPA_PLATFORM_PLUGIN_PATH
try {
    $qmake = Find-QMake
    $qtDirectory = Split-Path $qmake
    $compilerCandidates = @()
    if ($CompilerBin) {
        $compilerCandidates += $CompilerBin
    } else {
        # MSYS2 Qt must use the compiler from the same MSYS2 environment.
        $compilerCandidates += $qtDirectory
        if ($qtDirectory -notmatch '\\msys64\\') {
            if (Test-Path 'C:\Qt\Tools') {
                $compilerCandidates += @(Get-ChildItem 'C:\Qt\Tools' -Filter 'g++.exe' -Recurse -ErrorAction SilentlyContinue | ForEach-Object { $_.DirectoryName })
            }
            $compiler = Get-Command 'g++.exe' -ErrorAction SilentlyContinue
            if ($compiler) { $compilerCandidates += (Split-Path $compiler.Source) }
        }
    }
    $toolBin = $null
    foreach ($dir in ($compilerCandidates | Select-Object -Unique)) {
        if ((Test-Path (Join-Path $dir 'g++.exe')) -and (Test-Path (Join-Path $dir 'mingw32-make.exe'))) {
            $toolBin = $dir; break
        }
    }
    if (!$toolBin) { throw 'Matching g++.exe and mingw32-make.exe were not found. Run with -CompilerBin <MinGW-bin-folder> from your Qt kit.' }
    $env:PATH = "$toolBin;$qtDirectory;$originalPath"
    $plugins = & $qmake -query QT_INSTALL_PLUGINS
    if ($LASTEXITCODE -ne 0) { throw 'qmake could not query Qt plugins.' }
    $env:QT_QPA_PLATFORM_PLUGIN_PATH = Join-Path "$plugins" 'platforms'
    Write-Host "Qt:       $qmake"
    Write-Host "Compiler: $toolBin"
    Write-Host 'Building all project files...'
    $build = Join-Path $PSScriptRoot 'build-vscode'
    New-Item -ItemType Directory -Force $build | Out-Null
    Push-Location $build
    try {
        & $qmake (Join-Path $PSScriptRoot 'project.pro') 'CONFIG+=release' 'CONFIG-=debug' 'CONFIG-=debug_and_release'
        if ($LASTEXITCODE -ne 0) { throw 'qmake failed. See the compiler/Qt messages above.' }
        & (Join-Path $toolBin 'mingw32-make.exe') '-j2'
        if ($LASTEXITCODE -ne 0) { throw 'Compilation failed. See the errors above.' }
        $executable = Join-Path $build 'release\OpenGL_Lab5_Variant2.exe'
        if (!(Test-Path $executable)) { $executable = Join-Path $build 'OpenGL_Lab5_Variant2.exe' }
        if (!(Test-Path $executable)) { throw 'The executable was not found after building.' }
        Write-Host 'Starting OpenGL Lab 5...'
        & $executable
        if ($LASTEXITCODE -ne 0) { throw "The application exited with code $LASTEXITCODE." }
    } finally { Pop-Location }
} catch {
    Write-Host "ERROR: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
} finally {
    $env:PATH = $originalPath
    $env:QT_QPA_PLATFORM_PLUGIN_PATH = $originalPlugins
}
