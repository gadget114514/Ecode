param([string]$text)
$OutputEncoding = [Console]::OutputEncoding = [Text.Encoding]::UTF8
Write-Output $text | .\bin\Release\plugins\localmsg-cli.exe --send --agent test --to abc --stdin
