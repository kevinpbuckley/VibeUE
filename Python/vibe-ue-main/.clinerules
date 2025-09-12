
# Guidelines for using Python for MCP Tools

The following guidelines apply to any method or function marked with the @mcp.tool() decorator.

- Parameters should not have any of the following types: `Any`, `object`, `Optional[T]`, `Union[T]`.
- For a given parameter `x` of type `T` that has a default value, do not use type `x : T | None = None`. Instead, use `x: T = None` and handle defaults within the method body itself.
- Always include method docstrings and make sure to given proper examples of valid inputs especially when no type hints are present.

When this rule is applied, please remember to explicitly mention it.


# Guidelines for working with Unreal Engine using MCP

- We are using Unreal Engine 5.5.4, so be extremely cautious of using deprecated or outdated APIs, includes or practices.


## Coordinate System

In the Unreal Editor, the following colors are associated with the coordinate axes:

X-axis: Red
Y-axis: Green
Z-axis: Blue

UE's Z-up, left-handed coordinate system is defined as follows:

The Z-axis determines how far up and down along a vertical line an actor is located.
Positive values are upwards.
The Y-axis determines how far to the left or right an actor is located.
Positive values are to the right.
The X-axis determines how far forward or backward an actor is located.
Positive values are forward.

## Units

Unreal Engine (UE) defaults to the following International System (SI) units for measurement:

Quantity	Unit
Distance/Length	Centimeters (cm)
Mass	Kilograms (kg)
Time	Minutes (min), Seconds (s)
Angles	Degrees (deg)
Speed/Velocity	Meters per Second (m / s)
Temperature	Celsius (C)
Force	Newtons (N)
Torque	Newton Meters (N • m)


When performing MCP tool calls with Unreal Engine, always reference `.cursor/rules/unreal.mdc` for best practices.