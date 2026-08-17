"""Pure HTTP MCP SDK preflight verification for VibeUE Unreal Editor sessions."""

from __future__ import annotations

import argparse
import asyncio
import json
import os
import sys
from pathlib import Path

from mcp import ClientSession
from mcp.client.streamable_http import streamable_http_client


DEFAULT_ENDPOINT = "http://127.0.0.1:8000/mcp"
REQUIRED_TOOLS = {
    "execute_python_code",
    "list_toolsets",
    "describe_toolset",
    "call_tool",
}


def parse_args() -> argparse.Namespace:
    repo_root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(
        description="Verify VibeUE MCP HTTP connectivity, required tools, and open Unreal project."
    )
    parser.add_argument("--url", default=DEFAULT_ENDPOINT, help="VibeUE MCP HTTP endpoint.")
    parser.add_argument(
        "--project",
        type=Path,
        default=repo_root / "Terra.uproject",
        help="Expected .uproject path.",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=60.0,
        help="MCP operation timeout in seconds.",
    )
    return parser.parse_args()


def normalized_path(path: str | Path) -> str:
    return os.path.normcase(os.path.abspath(os.fspath(path))).replace("\\", "/")


def content_text(result: object) -> str:
    return "".join(
        getattr(item, "text", "")
        for item in getattr(result, "content", [])
    )


def format_error(error: BaseException) -> str:
    if isinstance(error, BaseExceptionGroup):
        return "\n".join(format_error(item) for item in error.exceptions)
    return f"{type(error).__name__}: {error}"


async def run_preflight(args: argparse.Namespace, status: dict[str, bool]) -> None:
    timeout = float(args.timeout)
    async with streamable_http_client(args.url) as client_res:
        read, write = client_res[0], client_res[1]
        async with ClientSession(read, write, read_timeout_seconds=timeout) as session:
            init_result = await session.initialize()

            tools_result = await session.list_tools()
            available_tools = {tool.name for tool in tools_result.tools}

            missing_tools = sorted(REQUIRED_TOOLS - available_tools)
            if missing_tools:
                raise RuntimeError(
                    f"VibeUE MCP is missing required tools: {', '.join(missing_tools)}"
                )

            project_result = await session.call_tool(
                "execute_python_code",
                {
                    "code": (
                        "import unreal\n"
                        "print(unreal.Paths.get_project_file_path())"
                    )
                },
            )
            if project_result.is_error:
                raise RuntimeError(
                    f"Could not query the Editor project: {content_text(project_result)}"
                )

            raw_content = content_text(project_result)
            payload = json.loads(raw_content)
            if not payload.get("success"):
                raise RuntimeError(
                    f"Editor project query failed: {payload.get('output', payload)}"
                )

            actual_project = str(payload.get("output", "")).strip()
            expected_project = str(args.project.resolve())
            if normalized_path(actual_project) != normalized_path(expected_project):
                raise RuntimeError(
                    "MCP is connected to the wrong Unreal project: "
                    f"expected '{expected_project}', got '{actual_project}'"
                )

            protocol_version = getattr(
                init_result,
                "protocol_version",
                getattr(init_result, "protocolVersion", "unknown"),
            )
            sys.stdout.write("VibeUE MCP preflight: PASS\n")
            sys.stdout.write(f"Endpoint:         {args.url}\n")
            sys.stdout.write(f"Protocol Version: {protocol_version}\n")
            sys.stdout.write(f"Project:          {actual_project}\n")
            sys.stdout.write(f"Tools Available:  {len(available_tools)} ({', '.join(sorted(available_tools))})\n")
            sys.stdout.flush()
            status["passed"] = True


def main() -> int:
    import logging
    logging.getLogger("mcp").setLevel(logging.CRITICAL)
    logging.getLogger("httpx").setLevel(logging.CRITICAL)
    args = parse_args()
    status = {"passed": False}
    try:
        asyncio.run(run_preflight(args, status))
    except ModuleNotFoundError as error:
        print(
            f"VibeUE MCP preflight: FAIL\nMissing Python dependency: {error}",
            file=sys.stderr,
        )
        return 2
    except BaseException as error:
        # Some MCP client versions report a TaskGroup cleanup error after every
        # preflight check has completed. Do not hide an error before that point.
        err_repr = repr(error)
        err_msg = str(error)
        is_cleanup_error = "TaskGroup" in err_repr and (
            "Session termination" in err_repr
            or "Session termination" in err_msg
            or "202" in err_repr
        )
        if status["passed"] and is_cleanup_error:
            print("VibeUE MCP preflight: PASS with a stream cleanup warning.", file=sys.stderr)
            return 0
        print(f"VibeUE MCP preflight: FAIL\n{format_error(error)}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
