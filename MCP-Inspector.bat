@echo off
echo Starting MCPJam Inspector for VibeUE...
echo.
echo MCPJam Inspector provides:
echo - Full MCP Spec Compliance
echo - Advanced debugging and logging
echo - LLM Playground with OpenAI, Claude, and Ollama
echo - Modern UI/UX for testing MCP servers
echo.

REM Change to the main project directory
cd ../..

REM Get the absolute path to the Python server
set "SERVER_PATH=%CD%\Plugins\VibeUE\Python\vibe-ue-main\Python\vibe_ue_server.py"

REM Launch MCPJam Inspector with VibeUE MCP server using absolute path
echo Launching MCPJam Inspector with VibeUE MCP server...
echo Server path: %SERVER_PATH%
npx @mcpjam/inspector@latest python "%SERVER_PATH%"

REM Alternative commands for different scenarios:
REM npx @mcpjam/inspector@latest --port 4000 python "%SERVER_PATH%"
REM npx @mcpjam/inspector@latest --config mcp.json
REM npx @mcpjam/inspector@latest --ollama llama3.2 python "%SERVER_PATH%"

pause