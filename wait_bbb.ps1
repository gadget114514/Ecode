$utf8 = [System.Text.UTF8Encoding]::new($false)
$OutputEncoding = [Console]::OutputEncoding = $utf8
$exe = ".\bin\Release\plugins\localmsg-cli.exe"
$r = & $exe --wait --agent bbb --timeout 120 2>$null
if ($LASTEXITCODE -eq 0) { Write-Output $r }
