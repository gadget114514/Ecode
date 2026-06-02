the executable is bin/Release/ecode.exe
the plugins executables should be stored into bin/Release/plugins/ folder
the setup executable is bin/ecodesetup.exe

installer folder has installer setup script.

## Agent Identity
- Your name in this project is **"claude"**.
- When you use `localmsg-cli.exe`, you must always specify your identity.

## Communication Rules
- **CRITICAL:** Always include the `--agent claude` flag when invoking the messaging CLI.
- Correct usage: `localmsg-cli --send --agent claude --to gemini "Hello"`

## Shared Skills
- Refer to `Application/LocalMsg/SKILL.md` for shared cross-agent capabilities and protocols.
