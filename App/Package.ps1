param([string]$DriverInstaller)
$ErrorActionPreference='Stop'
$DriverInstaller=& (Join-Path $PSScriptRoot 'PackageDriver.ps1') -DriverInstaller $DriverInstaller
$root=Split-Path $PSScriptRoot -Parent
$exe=Join-Path $root 'App\.build\Release\bin\CurveController.exe'
if(!(Test-Path -LiteralPath $exe)){throw 'Build the ordinary Release executable first.'}
if((Get-Item -LiteralPath $exe).VersionInfo.FileVersion -cne '1.1.0'){throw 'Unexpected app version.'}
$binary=[IO.File]::ReadAllBytes($exe)
if([Text.Encoding]::ASCII.GetString($binary).IndexOf('clang_rt.asan',[StringComparison]::OrdinalIgnoreCase)-ge 0){throw 'Do not distribute an AddressSanitizer executable.'}
$pe=[BitConverter]::ToInt32($binary,0x3c)
if([BitConverter]::ToUInt16($binary,$pe+24)-ne 0x20b -or [BitConverter]::ToUInt32($binary,$pe+24+112+14*8)-ne 0){throw 'Expected a native x64 executable without a CLR runtime.'}
$stage=Join-Path $root ('App\.build\Package-'+[guid]::NewGuid().ToString('N'))
$dist=Join-Path $root 'App\.build\dist'
New-Item -ItemType Directory -Force -Path $stage,$dist|Out-Null
Copy-Item -LiteralPath $exe -Destination $stage
Copy-Item -LiteralPath $DriverInstaller -Destination (Join-Path $stage 'InterceptorSetup.exe')
Copy-Item -LiteralPath $DriverInstaller -Destination (Join-Path $dist 'InterceptorSetup.exe') -Force
Copy-Item -LiteralPath $DriverInstaller -Destination (Join-Path (Split-Path $exe -Parent) 'InterceptorSetup.exe') -Force
foreach($name in 'README.md','LICENSE','DRIVER-BINARY-LICENSE.txt','THIRD-PARTY-NOTICES.md'){Copy-Item -LiteralPath (Join-Path $root $name) -Destination $stage}
Copy-Item -LiteralPath (Join-Path $root 'App\ThirdParty\json.LICENSE.MIT') -Destination (Join-Path $stage 'JSON-LICENSE.txt')
foreach($name in 'Examples','docs'){Copy-Item -LiteralPath (Join-Path $root $name) -Destination $stage -Recurse}
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zipPath=Join-Path $dist 'Curve-Controller-1.1.0-Windows-x64.zip'
$file=[IO.File]::Open($zipPath,[IO.FileMode]::Create)
try{$zip=[IO.Compression.ZipArchive]::new($file,[IO.Compression.ZipArchiveMode]::Create,$true);try{foreach($item in Get-ChildItem -LiteralPath $stage -Recurse -File){[void][IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip,$item.FullName,$item.FullName.Substring($stage.Length+1).Replace('\','/'),[IO.Compression.CompressionLevel]::Optimal)}}finally{$zip.Dispose()}}finally{$file.Dispose()}
Copy-Item -LiteralPath $exe -Destination $dist -Force
$lines=foreach($path in @($zipPath,(Join-Path $dist 'CurveController.exe'),(Join-Path $dist 'InterceptorSetup.exe'))){(Get-FileHash -LiteralPath $path).Hash.ToLowerInvariant()+'  '+[IO.Path]::GetFileName($path)}
[IO.File]::WriteAllText((Join-Path $dist 'SHA256SUMS.txt'),(($lines -join "`n")+"`n"),[Text.UTF8Encoding]::new($false))
Write-Host ('Portable app, seven JSON examples, documentation and checksums: '+$dist)
Write-Host 'Verified Gamepad & Controller setup included inside the ZIP and as a separate download. Kernel source is not packaged.'
