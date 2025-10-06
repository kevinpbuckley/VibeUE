"""Unit tests for manage_blueprint_node payload construction."""

from __future__ import annotations

import unittest
from typing import Any, Dict, List, Tuple
from unittest.mock import patch

from tools.node_tools import register_blueprint_node_tools


class MockMCP:
    """Minimal MCP stub that records registered tools."""

    def __init__(self) -> None:
        self.tools: Dict[str, Any] = {}

    def tool(self):
        def decorator(func):
            self.tools[func.__name__] = func
            return func

        return decorator


class FakeConnection:
    """Captures outgoing commands without touching a live Unreal instance."""

    def __init__(self) -> None:
        self.calls: List[Tuple[str, Dict[str, Any]]] = []

    def send_command(self, command: str, payload: Dict[str, Any]) -> Dict[str, Any]:
        self.calls.append((command, payload))
        # Mirror the success envelope that the tooling expects
        return {"success": True, "node_id": "StubNode"}


class ManageBlueprintNodePayloadTests(unittest.TestCase):
    def setUp(self) -> None:
        mock_mcp = MockMCP()
        register_blueprint_node_tools(mock_mcp)
        self.manage_blueprint_node = mock_mcp.tools["manage_blueprint_node"]

    @patch("vibe_ue_server.get_unreal_connection")
    def test_function_call_payload_includes_external_class(self, mock_get_connection):
        fake_connection = FakeConnection()
        mock_get_connection.return_value = fake_connection

        result = self.manage_blueprint_node(
            ctx=None,
            blueprint_name="BP_Player2",
            action="create",
            node_type="CallFunction",
            node_params={
                "function_name": "GetPlayerController",
                "function_class": "/Script/Engine.GameplayStatics",
            },
        )

        self.assertTrue(result["success"])
        self.assertEqual(len(fake_connection.calls), 1)
        command, payload = fake_connection.calls[0]
        self.assertEqual(command, "manage_blueprint_node")
        self.assertEqual(payload["blueprint_name"], "BP_Player2")
        self.assertEqual(payload["node_params"]["function_name"], "GetPlayerController")
        self.assertEqual(
            payload["node_params"]["function_class"], "/Script/Engine.GameplayStatics"
        )

    @patch("vibe_ue_server.get_unreal_connection")
    def test_cast_payload_includes_target_descriptor(self, mock_get_connection):
        fake_connection = FakeConnection()
        mock_get_connection.return_value = fake_connection

        result = self.manage_blueprint_node(
            ctx=None,
            blueprint_name="BP_Player2",
            action="create",
            node_type="Cast To BP_MicrosubHUD",
            node_params={
                "cast_target": "/Game/Blueprints/UI/BP_MicrosubHUD.BP_MicrosubHUD_C",
            },
        )

        self.assertTrue(result["success"])
        self.assertEqual(len(fake_connection.calls), 1)
        command, payload = fake_connection.calls[0]
        self.assertEqual(command, "manage_blueprint_node")
        self.assertEqual(
            payload["node_params"]["cast_target"],
            "/Game/Blueprints/UI/BP_MicrosubHUD.BP_MicrosubHUD_C",
        )
        # Node identifier fallback mirrors node_type to support legacy callers
        self.assertEqual(payload["node_type"], "Cast To BP_MicrosubHUD")
        self.assertEqual(payload["node_identifier"], "Cast To BP_MicrosubHUD")

    @patch("vibe_ue_server.get_unreal_connection")
    def test_refresh_node_payload_supports_compile_toggle(self, mock_get_connection):
        fake_connection = FakeConnection()
        mock_get_connection.return_value = fake_connection

        result = self.manage_blueprint_node(
            ctx=None,
            blueprint_name="/Game/Blueprints/BP_Player2",
            action="refresh_node",
            node_id="{ABC123}",
            extra={"compile": False},
        )

        self.assertTrue(result["success"])
        command, payload = fake_connection.calls[0]
        self.assertEqual(command, "manage_blueprint_node")
        self.assertEqual(payload["action"], "refresh_node")
        self.assertFalse(payload["compile"])
        self.assertEqual(payload["node_id"], "{ABC123}")


if __name__ == "__main__":
    unittest.main()
