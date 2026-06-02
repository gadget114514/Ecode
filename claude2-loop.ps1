$cli = "D:\ws\Ecode\bin\Release\plugins\localmsg-cli.exe"
$agent = "claude2"
$logFile = "D:\ws\Ecode\claude2-loop.log"

function Log($msg) {
    $ts = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    "$ts $msg" | Out-File -Append -Encoding utf8 $logFile
}

Log "claude2 loop started"

while ($true) {
    $output = & $cli --wait --agent $agent --timeout 30 2>&1
    $exitCode = $LASTEXITCODE

    if ($exitCode -eq 0) {
        Log "Received: $output"
        try {
            $msg = $output | ConvertFrom-Json
            $from = $msg.from
            $text = $msg.text

            if ($text -eq "STOP") {
                Log "STOP received, logging out"
                & $cli --logout --agent $agent
                break
            }

            if ($from -and $text) {
                $reply = "claude2: received your message: $text"
                & $cli --send --agent $agent --to $from $reply
                Log "Replied to ${from}: $reply"
            }
        } catch {
            Log "Parse error: $_"
        }
    } elseif ($exitCode -eq 1) {
        # timeout, continue loop
    } else {
        Log "Unexpected exit code: $exitCode"
        Start-Sleep -Seconds 5
    }
}

Log "claude2 loop ended"
