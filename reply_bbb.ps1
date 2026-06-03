$utf8 = [System.Text.UTF8Encoding]::new($false)
$OutputEncoding = [Console]::OutputEncoding = $utf8
$text = "東京の天気: Patchy rain nearby (霧雨), 気温 22°C, 湿度 82%, 風速 西北西 24km/h"
Write-Output $text | .\bin\Release\plugins\localmsg-cli.exe --send --agent bbb --to aaa --stdin
