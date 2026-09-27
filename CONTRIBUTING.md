# Contributing

This project is intended for a small trusted team working on a rooted Harmony Hub replacement runtime.

## Rules

- Keep the repository focused on the post-root web UI and runtime.
- Do not add rooting tools, vulnerability writeups, firmware dumps, or device backups.
- Do not commit SSH keys, MQTT passwords, Home Assistant tokens, hub-specific credentials, or personal network details.
- Keep the HTML page unauthenticated unless the team explicitly decides to change the product policy.
- Prefer small pull requests with a short test note.

## Suggested Branch Flow

```text
main
  feature/ir-import-polish
  feature/bthid-script-runner
  fix/mqtt-state-publish
```

Before handing a change to someone else, run at least:

```powershell
.\install_webui.ps1 -HubHost <test-hub-ip> -KeyPath "$env:USERPROFILE\.ssh\<root-key-file>"
```

Then verify the page loads and the relevant feature works.
