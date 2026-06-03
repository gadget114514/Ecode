param([string]$agent = "abc", [int]$timeout = 120)

$utf8 = [System.Text.UTF8Encoding]::new($false)
$OutputEncoding = [Console]::OutputEncoding = $utf8

$exe = ".\bin\Release\plugins\localmsg-cli.exe"

function Get-Weather($city) {
    try {
        $url = "https://wttr.in/$city`?format=%l:+%C+%t+%h+%w"
        $w = (Invoke-WebRequest -Uri $url -UseBasicParsing -TimeoutSec 10).Content.Trim()
        return "${city}: $w"
    } catch {
        return "${city}: weather unavailable"
    }
}

function Get-WeatherJapan($query) {
    $cities = @("Tokyo", "Kyoto", "Osaka", "Nagoya", "Sapporo", "Fukuoka", "Yokohama", "Naha", "Kobe", "Hiroshima", "Sendai")
    $result = $null
    foreach ($c in $cities) {
        if ($query -match $c) {
            $result = Get-Weather $c
            break
        }
    }
    return $result
}

while ($true) {
    $r = & $exe --wait --agent $agent --timeout $timeout 2>$null
    if ($LASTEXITCODE -eq 0) {
        Write-Host ">>> $r"
        try {
            $msgs = $r | ConvertFrom-Json
            foreach ($m in $msgs) {
                $from = $m.from
                $text = $m.text
                Write-Host "  from=$from text=$text"
                $reply = $null
                if ($text -match "weather|tenki|temp|kion") {
                    $reply = Get-WeatherJapan $text
                }
                if ($reply) {
                    Write-Output $reply | & $exe --send --agent $agent --to $from --stdin 2>$null
                    Write-Host "  replied: $reply"
                }
            }
        } catch {
            # ignore parse errors
        }
    }
}
