$libs = "Neo Steelgear Graphics Core 64.lib", "Neo Steelgear Graphics Core 64D.lib", "Neo Steelgear Graphics Core 32.lib", "Neo Steelgear Graphics Core 32D.lib"

Get-ChildItem -Path "Distribute" -Include *.* -File -Recurse | foreach { $_.Delete()}

foreach ($file in $libs)
{
	

	$path = Get-ChildItem -Path "" -Filter $file -Recurse -ErrorAction SilentlyContinue -Force
	if ($path)
	{
		Copy-Item $path.FullName -Destination "Distribute\\Libraries\\"
	}
	else
	{
		Write-Host "Could not find file:" $file
	}
}

Get-ChildItem -Path "Neo Steelgear Graphics Core" -Include *.h* -File -Recurse | foreach { Copy-Item $_.FullName -Destination "Distribute\\Headers\\"}