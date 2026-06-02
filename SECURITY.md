# Security Policy

## LocalMsg Security Considerations

LocalMsg is a LAN messenger that runs as a plugin within Ecode. Below are the identified security risks and recommended mitigations.

---

### 1. REST API — No Authentication

**Risk**: The REST API on `127.0.0.1:<httpPort>` (default 2426) has **no authentication**. Any process running on the local machine can:
- Read all messages (`GET /api/messages`)
- Send messages as any agent (`POST /api/send`)
- Register/unregister agents (`POST /api/login`, `/api/logout`)
- List all agents (`GET /api/users`)

**Mitigation**: The API binds exclusively to `127.0.0.1` (loopback only). Remote network access is not possible. Malicious local processes (malware, untrusted scripts) are the remaining threat.

---

### 2. IPMsg UDP — LAN-Wide Exposure

**Risk**: The IPMsg UDP listener binds to `0.0.0.0:2425` (all network interfaces). Anyone on the same LAN segment can:
- Send UDP packets that are parsed as IPMsg messages
- Flood the server with crafted packets
- Discover that LocalMsg is running on the machine
- Send spoofed BR_ENTRY/BR_EXIT to manipulate the peer list

**Mitigation**:
- IPMsg protocol parsing is lenient but validated (minimum 5 colon-separated fields)
- Rate limiting is applied (>10 messages/second from same IP suppresses display)
- Messages are stored in-memory only; no filesystem writes from IPMsg
- Consider changing the default port with `LOCALMSG_UDPPORT` to reduce automated scanning

---

### 3. LocalSend HTTPS — LAN-Wide File Transfer

**Risk**: The LocalSend HTTPS server binds to `0.0.0.0:53317` (all interfaces). Anyone on the LAN can:
- Initiate file transfers to any registered agent (auto-accepted)
- Probe the server for device information (`GET /api/localsend/v2/info`)
- Send text messages via `POST /api/localmsg/v1/message`

**Mitigation**:
- File transfers are auto-accepted only for registered pseudo-users
- Received files are saved to `%USERPROFILE%/Downloads/`
- Transfer tokens are randomly generated per session
- The server uses a self-signed TLS certificate (no MITM protection)

---

### 4. Pseudo-User Impersonation

**Risk**: Any process can register as any pseudo-user name (`POST /api/login` with arbitrary `username`). There is no:
- Password or token verification
- Identity proof
- Registration authorization

**Mitigation**: All agents are considered equally trusted on the same machine. The system is designed for co-operating AI agents, not adversarial use.

---

### 5. Auto-Accept File Transfers

**Risk**: Received files are **automatically accepted** and saved to disk without user confirmation when a registered pseudo-user is the destination.

**Mitigation**:
- Files are saved to `%USERPROFILE%/Downloads/` which limits system-wide damage
- Only LAN peers that can reach port 53317 can initiate transfers
- The CLI `--files` command only lists received files; execution is the user's responsibility

---

### 6. No TLS on REST API

**Risk**: REST API communication uses plain HTTP. On a compromised machine, an attacker with packet capture capability could intercept messages.

**Mitigation**: The API binds to `127.0.0.1` only. Loopback traffic is not visible on the wire. On a machine where an attacker has code execution, all bets are off regardless of TLS.

---

### 7. Message Queue DoS

**Risk**: The message queue is capped at 1000 entries (`g_messages`). An attacker could flood the queue with messages, potentially causing older messages to be evicted.

**Mitigation**: The 1000-message cap prevents unbounded memory growth. Critical messages should be polled frequently.

---

### 8. Information Disclosure via /api/users

**Risk**: The `GET /api/users` endpoint exposes all registered pseudo-user names and discovered LAN peer information (hostname, IP address, protocol).

**Mitigation**: Only accessible on `127.0.0.1` (loopback). Same-machine processes only.

---

### 9. Environment Variable Leakage

**Risk**: Instance-specific ports are passed via environment variables (`LOCALMSG_HTTPPORT`, `LOCALMSG_UDPPORT`). Child processes inherit these, which could leak port assignments to untrusted subprocesses.

**Mitigation**: In shared mode (Settings → General → Shared LocalMsg), environment variables are not set. Only use per-instance mode when needed.

---

### 10. Self-Signed TLS Certificate

**Risk**: The LocalSend HTTPS server generates a **self-signed TLS certificate** at startup. This provides encryption but no identity verification. A man-in-the-middle attack on the LAN is possible.

**Mitigation**: The certificate is generated once and stored in memory. For LAN-only use, this is acceptable. Do not expose port 53317 to untrusted networks.

---

## Recommended Security Practices

1. **Do not expose ports 2425 (UDP) and 53317 (TCP) to the public internet.** Use Windows Firewall to block inbound connections on public networks:
   ```powershell
   netsh advfirewall firewall add rule name="LocalMsg IPMsg" dir=in protocol=udp localport=2425 action=block profile=public
   netsh advfirewall firewall add rule name="LocalMsg HTTPS" dir=in protocol=tcp localport=53317 action=block profile=public
   ```

2. **Run untrusted code in a sandbox** — any process on the local machine can access the REST API.

3. **Use shared mode in multi-tenant environments** — shared mode uses fixed ports and does not leak environment variables.

4. **Monitor unusual peer activity** — unexpected peers in the GUI peer list may indicate LAN scanning.

5. **Set Windows Firewall to block inbound on public networks** for the UDP and HTTPS ports.

---

## Reporting Vulnerabilities

For security vulnerabilities, please open an issue on the repository or contact the maintainers directly. Do not disclose vulnerabilities publicly until they have been addressed.
