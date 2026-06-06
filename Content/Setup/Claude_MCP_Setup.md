# VibeUE MCP Setup — Instructions for Claude

> **MANDATORY GATE — Read before doing anything else.**
> Do NOT start the proxy, create any files, run any commands, or take any action
> until the user has explicitly chosen option 1 below.
> Present the choice first. Wait for a response. Then proceed.

---

## Step 1 — Ask permission (REQUIRED — do not skip)

Say this to the user:

> "VibeUE is installed in this project. I can connect to it via an MCP proxy, which
> gives me direct access to Unreal Engine — inspecting widgets, building blueprints,
> managing assets and more in a single step.
>
> Would you like to set it up?
>   **1. Yes** — set it up now
>   **2. No** — skip for now
>   **3. What is it?** — explain what the proxy does and why it's useful"

If they choose **3**, explain:

> "The VibeUE proxy is a small Python script that runs locally on your machine and
> bridges Claude to Unreal Engine using the MCP protocol.
>
> Without it I have to fall back to raw Python execution, which takes multiple
> round-trips to discover APIs. With it I have 10 specialist tools and can do things
> like inspect a full widget hierarchy in one call.
>
> A bearer token protects the connection — it stops other processes on your machine
> sending commands to Unreal Engine without permission. Everything stays local
> (127.0.0.1), nothing leaves your machine."

Then ask again: **1 to set up, 2 to skip.**

If they choose **2**, stop here.

---

## Step 2 — Ask for a bearer token

Say:

> "Please choose a bearer token — this is a password that protects the connection
> between Claude and Unreal Engine. It can be any string you like, for example
> `myproject2025`. You will set the same value in two places."

**Do not suggest, reuse, or recall a token from memory or from any existing file.
Always ask the user to provide one fresh. Wait for them to type it before continuing.**

---

## Step 3 — Create the config files

Using the token the user provided, create these three files:

**`Plugins/VibeUE/vibeue-proxy.json`** (overwrite if it exists):
```json
{"bearer_token": "<token>"}
```

**`.mcp.json`** in the project root (create if it does not exist):
```json
{
  "mcpServers": {
    "VibeUE": {
      "type": "http",
      "url": "http://127.0.0.1:8089/mcp"
    }
  }
}
```

**`.claude/settings.local.json`** in the project root:
- If the file does not exist, create it:
```json
{
  "enabledMcpjsonServers": ["VibeUE"]
}
```
- If it already exists, read it first and add `"VibeUE"` to `enabledMcpjsonServers` if not present.

Then tell the user:

> "Now open Unreal Editor → Project Settings → Plugins → VibeUE → API Key and enter
> the same token: `<token>`
>
> Let me know when that's done."

Wait for confirmation before continuing.

---

## Step 4 — Start the proxy

Run this command (from the project root):

```
Plugins\VibeUE\Content\Python\vibeue-proxy.py
```

Using bash:
```bash
cd "Plugins/VibeUE/Content/Python" && python vibeue-proxy.py &
```

Wait 1 second then verify it is listening:
```bash
netstat -ano | findstr :8089
```

If you see `LISTENING`, continue. If not, tell the user to check Python 3 is installed
and in PATH (`python --version`), then retry.

---

## Step 5 — Prompt the user to restart

Say:

> "Setup is complete. Please start a **new Claude Code session** in this project —
> close this chat and open a fresh one. Do not use `/mcp` reconnect.
>
> In the new session, VibeUE tools will be available. You can verify by asking me to
> check `vibeue_status` — it should report everything ready."

---

## Troubleshooting (use if something goes wrong after restart)

| Problem | Fix |
|---|---|
| `vibeue_status` not in tool list | Check `.mcp.json` exists in project root and `.claude/settings.local.json` contains `"VibeUE"` |
| `vibeue_status` says token not set | Check `Plugins/VibeUE/vibeue-proxy.json` has the token and restart the proxy |
| `vibeue_status` says UE not running | Launch Unreal Editor with VibeUE plugin enabled |
| `vibeue_status` says manifest not found | Launch UE once — it writes the tool manifest on startup |
| Proxy fails to start | Run `python --version` to confirm Python 3 is installed |
