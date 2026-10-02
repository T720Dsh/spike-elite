# Scoped build environment: local Zen requests must not pass through the user's
# inherited HTTP proxy. Never change the user's persisted proxy/DDC settings.
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
$uat='D:\Epic\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat'
$savedNoProxy=$env:NO_PROXY
$savedHttpProxy=$env:HTTP_PROXY
$savedHttpsProxy=$env:HTTPS_PROXY
$savedAllProxy=$env:ALL_PROXY
try {
    $env:NO_PROXY = 'localhost,127.0.0.1,::1,[::1]' + $(if($savedNoProxy){','+$savedNoProxy}else{''})
    # Some .NET proxy versions mishandle the IPv6 loopback bypass. The build
    # requires local Zen, not external web access: clear proxies for this build
    # process only and restore even on failure.
    [Environment]::SetEnvironmentVariable('HTTP_PROXY',$null,'Process')
    [Environment]::SetEnvironmentVariable('HTTPS_PROXY',$null,'Process')
    [Environment]::SetEnvironmentVariable('ALL_PROXY',$null,'Process')
    # Windows environment names are case-insensitive; this also supplies no_proxy.
    & $uat BuildCookRun "-project=$projectRoot\SpikeElite.uproject" -noP4 -platform=Win64 -clientconfig=Development -cook -allmaps -build -stage -pak -archive "-archivedirectory=$projectRoot\Dist\M11g"
    if($LASTEXITCODE -ne 0){throw "BuildCookRun failed: $LASTEXITCODE"}
} finally {
    [Environment]::SetEnvironmentVariable('NO_PROXY',$savedNoProxy,'Process')
    [Environment]::SetEnvironmentVariable('HTTP_PROXY',$savedHttpProxy,'Process')
    [Environment]::SetEnvironmentVariable('HTTPS_PROXY',$savedHttpsProxy,'Process')
    [Environment]::SetEnvironmentVariable('ALL_PROXY',$savedAllProxy,'Process')
}
