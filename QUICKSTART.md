# VibeUE Quick Start Card

## Installation (5 minutes)

### Step 1: Clone Plugin
```bash
cd YourProject/Plugins
git clone https://github.com/kevinpbuckley/VibeUE.git
```

### Step 2: Build Plugin
**Windows:** Double-click `Plugins/VibeUE/BuildPlugin.bat`

The script auto-detects your Unreal Engine and project.

### Step 3: Enable in Unreal
1. Open Unreal Engine
2. Edit → Plugins
3. Enable "VibeUE"
4. Restart editor

### Step 4: Configure MCP Client
Create `.vscode/mcp.json`:
```json
{
  "servers": {
    "VibeUE": {
      "type": "stdio",
      "command": "python",
      "args": ["Plugins\\VibeUE\\Python\\vibe-ue-main\\Python\\vibe_ue_server.py"],
      "cwd": "${workspaceFolder}"
    }
  }
}
```

### Step 5: Test
Ask AI: "Search for widgets in my project"

---

## Common Tasks

### Build Plugin
Just double-click: `BuildPlugin.bat`

### Troubleshooting

**"Missing Modules" error?**
→ Double-click `BuildPlugin.bat` in the VibeUE folder

**"Could not find Unreal Engine"?**
→ Edit BuildPlugin.bat and add your UE path to the search list

**"Could not find .uproject file"?**
→ Make sure plugin is in YourProject/Plugins/VibeUE/

**Build fails?**
→ Delete Binaries and Intermediate folders, then run BuildPlugin.bat again

---

## What Was Built?

After successful build:
```
VibeUE/
  Binaries/Win64/
    UnrealEditor-VibeUE.dll  ← Plugin binary (4-5 MB)
    UnrealEditor-VibeUE.pdb  ← Debug symbols
```

---

## Need Help?

- 📖 [Full Documentation](README.md)
- 🔧 [Build Guide](docs/BUILD_PLUGIN.md)
- 🐛 [Issues](https://github.com/kevinpbuckley/VibeUE/issues)
