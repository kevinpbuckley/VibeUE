# check_unreal_connection - Test Prompt

## Purpose

This test validates the connection diagnostic tool that verifies the MCP server can communicate with the Unreal Engine plugin. This is a fundamental tool that should be run before any other testing to ensure the system is properly configured.

## Prerequisites

- VibeUE plugin source files are present in your project's Plugins folder
- MCP client (VS Code, Claude Desktop, Cursor, or Windsurf) is configured
- Python MCP server is installed and configured

## Test Steps

### Test 1: Check Connection When Unreal is Running

1. **Ensure Unreal Engine is running** with your project loaded and VibeUE plugin enabled

2. Ask your AI assistant: "Check the connection to Unreal Engine"

3. Review the connection status

### Test 2: Check Connection Details

4. Ask your AI assistant: "Verify that VibeUE is connected to Unreal Engine and show connection details"

5. Review the detailed connection information

### Test 3: Check Connection When Unreal is Not Running

6. **Close Unreal Engine** completely

7. Ask your AI assistant: "Check the connection to Unreal Engine"

8. Review the error message or disconnection status

### Test 4: Check Connection After Restarting Unreal

9. **Restart Unreal Engine** with your project

10. **Wait for the project to fully load** (approximately 30-60 seconds)

11. Ask your AI assistant: "Check the connection to Unreal Engine"

12. Verify the connection is restored

### Test 5: Connection Status in Error Messages

13. With Unreal running, ask your AI assistant to perform an invalid operation: "Get information about a blueprint named 'BP_NonExistent'"

14. Observe if connection status is mentioned in the error message

### Test 6: Multiple Connection Checks

15. Ask your AI assistant: "Check Unreal connection" 

16. Immediately ask again: "Check Unreal connection"

17. Verify both checks complete successfully

## Expected Outcomes

### Test 1 - Connection Active
- ✅ Returns "Connected" or similar positive status
- ✅ Confirms Unreal Engine is running
- ✅ Shows plugin is active
- ✅ May include connection details (port number, host)
- ✅ May include Unreal Engine version information
- ✅ Response time is quick (< 1 second)

### Test 2 - Connection Details
- ✅ Returns comprehensive connection information
- ✅ Shows server address and port (typically localhost:55557)
- ✅ Confirms TCP connection is established
- ✅ May show plugin version
- ✅ May show server uptime or connection duration

### Test 3 - Connection Failed
- ✅ Returns "Disconnected" or "Connection Failed" status
- ✅ Clear error message indicating Unreal is not running
- ✅ Suggests starting Unreal Engine
- ✅ May suggest checking plugin is enabled
- ✅ No crash or unexpected behavior
- ✅ Error message is helpful and actionable

### Test 4 - Connection Restored
- ✅ Returns "Connected" status after restart
- ✅ Connection is automatically re-established
- ✅ No manual intervention required
- ✅ All tools become functional again

### Test 5 - Error Message Context
- ✅ Error message clearly indicates the operation failed
- ✅ May include connection check suggestion
- ✅ Distinguishes between connection issues and operation issues
- ✅ Error is specific to the invalid blueprint, not connection

### Test 6 - Multiple Checks
- ✅ Both connection checks succeed
- ✅ No performance degradation
- ✅ Consistent results
- ✅ No connection instability

## Notes

- The MCP server connects to Unreal Engine on TCP port 55557 by default
- Connection is established when Unreal Engine starts with the plugin enabled
- The plugin automatically starts its TCP server when Unreal Engine loads
- If connection fails, verify:
  - Unreal Engine is running
  - VibeUE plugin is enabled (Edit > Plugins)
  - No firewall blocking port 55557
  - Plugin is properly compiled and loaded
  - Python MCP server is running
- Connection checks are lightweight and can be run frequently
- This tool should be the **first test** you run when setting up VibeUE
- This tool is useful for debugging when other tools fail unexpectedly

## Troubleshooting

If connection fails even with Unreal running:

1. **Check Plugin Status**:
   - In Unreal: Edit > Plugins
   - Search for "VibeUE"
   - Verify it's enabled with a checkmark
   - Restart Unreal if you just enabled it

2. **Check Output Log**:
   - In Unreal: Window > Developer Tools > Output Log
   - Look for VibeUE plugin messages
   - Check for TCP server startup messages
   - Look for any error messages

3. **Check Port Availability**:
   - Ensure no other application is using port 55557
   - On Windows: `netstat -an | findstr 55557`
   - On Mac/Linux: `netstat -an | grep 55557`

4. **Restart Everything**:
   - Close Unreal Engine
   - Restart MCP client
   - Restart Unreal Engine
   - Wait for full project load
   - Try connection check again

## Success Criteria

You should be able to:
- ✅ Successfully check connection when Unreal is running
- ✅ Receive clear error when Unreal is not running
- ✅ Automatically reconnect when Unreal is restarted
- ✅ Use this tool as a diagnostic before other operations
