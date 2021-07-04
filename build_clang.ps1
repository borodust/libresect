param (
    [parameter(Mandatory=$false)]
    [int] $BuildThreadCount=0,
    [parameter(Mandatory=$false)]
    [String] $BuildType="MinSizeRel"
)

### CONFIGURATION START ###
$ErrorActionPreference = "Stop"

if ( $BuildThreadCount -eq 0 ) {
    $ComputerSystem = Get-WmiObject -class Win32_ComputerSystem
    $BuildThreadCount = $ComputerSystem.NumberOfLogicalProcessors - 1
    if ( $BuildThreadCount -le 0) {
        $BuildThreadCount = 1
    }
}

 Write-Output "Build thread count: $BuildThreadCount"

$WorkDir = $PSScriptRoot
$BuildDir = "$WorkDir/build/llvm"

$LlvmProjectDir = "$WorkDir/llvm"
$LlvmDir = "$LlvmProjectDir/llvm"
$ClangDir = "$LlvmProjectDir/clang"
$LlvmInstallDir = "$WorkDir/llvm-bin"

Write-Output "LLVM install directory: $LlvmInstallDir"

$LibClangStaticBuild = "$WorkDir/libclang-static-build"
$LibClangStaticBuildDir = "$WorkDir/build/libclang-static"
$LibClangStaticInstallDir = "$WorkDir/libclang-bundle"

Write-Output "LibClang Install directory: $LibClangStaticInstallDir"

### CONFIGURATION END ###

md $BuildDir -Force | Out-Null
pushd $BuildDir

cmake -G "Visual Studio 16 2019" -A x64 -Thost=x64 `
      -DCMAKE_INSTALL_PREFIX="$LlvmInstallDir" `
      -DCMAKE_BUILD_TYPE="$BuildType" `
      -DLLVM_ENABLE_PROJECTS="clang;" `
      -DLLVM_TARGETS_TO_BUILD="X86;AArch64;" `
      -DLLVM_ENABLE_LLD=OFF `
      -DLLVM_INCLUDE_TESTS=OFF `
      -DLLVM_INCLUDE_BENCHMARKS=OFF `
      -DLLVM_ENABLE_LIBCXX=OFF `
      -DLLVM_BUILD_DOCS=OFF `
      -DLIBCLANG_BUILD_STATIC=ON `
      -DBUILD_SHARED_LIBS=OFF `
      "$LlvmDir"
cmake --build "$BuildDir" --target install --config MinSizeRel --parallel $BuildThreadCount

popd

md $LibClangStaticBuildDir -Force | Out-Null
pushd $LibClangStaticBuildDir

cmake -G "Visual Studio 16 2019" -A x64 -Thost=x64 `
      -DCMAKE_INSTALL_PREFIX="$LibClangStaticInstallDir" `
      -DCMAKE_BUILD_TYPE="MinSizeRel" `
      -DCMAKE_POSITION_INDEPENDENT_CODE=ON `
      -DBUILD_SHARED_LIBS=OFF `
      -DLIBCLANG_SOURCES_DIR="$ClangDir" `
      -DLIBCLANG_PREBUILT_DIR="$LlvmInstallDir" `
      "$LibClangStaticBuild"
cmake --build "$LibClangStaticBuildDir" --target install --config MinSizeRel --parallel $BuildThreadCount

popd