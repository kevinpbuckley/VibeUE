# VibeUE MCP — First-Time Setup (Claude Instructions)

This file tells **you, Claude**, how to guide a user through setting up the VibeUE MCP
proxy. Follow it when a user asks to use VibeUE, when MCP tools are not in your tool
list, or when `vibeue_status` reports a problem.

---

## Step 1 — Introduce the proxy and offer a choice

Present this to the user:

> "VibeUE is installed in this project. There's an MCP proxy available that gives me
> direct access to Unreal Engine — I can inspect widgets, build blueprints, manage
> assets, and more in a single step rather than writing raw Python.
>
> Would you like to set it up?
>   **1. Yes** — walk me through it now
>   **2. No** — skip for now, I'll ask later
>   **3. What is it?** — explain the proxy and why it's useful"

If they choose **3**, say:

> "The VibeUE proxy is a small Python script (`vibeue-proxy.py`) that runs locally on
> your machine. It bridges Claude Code to Unreal Engine over a standard protocol called
> MCP (Model Context Protocol).
>
> Without it, I fall back to raw Python execution which requires multiple round-trips
> to discover APIs. With it, I have 10 specialist tools and can do things like inspect
> a full widget hierarchy in one call.
>
> A bearer token is used to protect the connection — it prevents other processes on
> your machine from sending commands to Unreal Engine without authorisation.
> Everything stays local (127.0.0.1), nothing leaves your machine."

Then re-offer the Yes/No choice.

If they choose **2**, stop and do not proceed.

---

## Step 2 — Check if the proxy is already running

Run this in bash:
```
netstat -ano | findstr :8089
```

- If you see `LISTENING` → proxy is already running, skip to **Step 4**.
- If nothing → go to **Step 3**.

---

## Step 3 — Start the proxy

Run this in bash (from the project root):
```bash
cd "Plugins/VibeUE/Content/Python" && python vibeue-proxy.py &
```

Wait 1 second, then re-run the `netstat` check to confirm it is LISTENING.
If it fails to start, tell the user to check that Python 3 is installed and in PATH.

---

## Step 4 — Set a bearer token

Ask the user:

> "Please choose a bearer token — this is a password that protects the connection
> between Claude and Unreal Engine. It can be any string (e.g. `myproject2025`).
> You will set the same value in two places: the proxy config and UE Project Settings."

Once they provide a token:

1. Write `Plugins/VibeUE/vibeue-proxy.json` with:
```json
{"bearer_token": "<their-token>"}
```

2. Tell the user:
> "Now open Unreal Editor → Project Settings → Plugins → VibeUE → API Key
> and paste in the same token: `<their-token>`"

Wait for them to confirm they have done this before continuing.

---

## Step 5 — Create the MCP config files

Check whether `.mcp.json` already exists in the project root.

If it does **not** exist, create it:
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

Check whether `.claude/settings.local.json` exists.

If it does **not** exist, create `.claude/settings.local.json`:
```json
{
  "enabledMcpjsonServers": ["VibeUE"]
}
```

If it **does** exist, read it first and add `"VibeUE"` to the `enabledMcpjsonServers`
array if it is not already present.

---

## Step 6 — Restart the proxy to pick up the new token

Kill and restart the proxy so it reloads `vibeue-proxy.json`:
```bash
# Find and kill the existing proxy
netstat -ano | findstr :8089
# Note the PID from the output, then:
taskkill /PID <pid> /F
# Restart
cd "Plugins/VibeUE/Content/Python" && python vibeue-proxy.py &
```

---

## Step 7 — Prompt the user to start a fresh Claude session

Say:

> "Setup is complete. Please start a **new Claude Code session** in this project
> (close this chat and open a new one — not just /mcp reconnect).
>
> In the new session, check that VibeUE tools are available by asking me to call
> `vibeue_status`. If it returns 'VibeUE is ready', everything is working."

---

## Troubleshooting

| Symptom | Fix |
|---|---|
| `vibeue_status` says token not set | Check `vibeue-proxy.json` has `bearer_token` set and restart proxy |
| `vibeue_status` says UE not running | Launch Unreal Editor with VibeUE plugin enabled |
| `vibeue_status` says manifest not found | Launch UE once — it writes the manifest on startup |
| Tools not visible after new session | Verify `.mcp.json` exists in project root and `enabledMcpjsonServers` includes `"VibeUE"` |
| Proxy fails to start | Ensure Python 3 is installed: `python --version` |
