
# Guidelines for using Python for MCP Tools

The following guidelines apply to any method or function marked with the @mcp.tool() decorator.

- Parameters should not have any of the following types: `Any`, `object`, `Optional[T]`, `Union[T]`.
- For a given parameter `x` of type `T` that has a default value, do not use type `x : T | None = None`. Instead, use `x: T = None` and handle defaults within the method body itself.
- Always include method docstrings and make sure to given proper examples of valid inputs especially when no type hints are present.

When this rule is applied, please remember to explicitly mention it.


# Guidelines for using Python for MCP Tools

The following guidelines apply to any method or function marked with the @mcp.tool() decorator.

- Parameters should not have any of the following types: `Any`, `object`, `Optional[T]`, `Union[T]`.
- For a given parameter `x` of type `T` that has a default value, do not use type `x : T | None = None`. Instead, use `x: T = None` and handle defaults within the method body itself.
- Always include method docstrings and make sure to given proper examples of valid inputs especially when no type hints are present.

When this rule is applied, please remember to explicitly mention it.