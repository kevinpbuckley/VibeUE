# manage_tasks

Manage a structured task list to track progress and plan multi-step work.

## Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `taskList` | array | Yes | Complete array of all task items |

### Task Item Schema

Each item in the `taskList` array:

| Field | Type | Description |
|-------|------|-------------|
| `id` | number | Sequential identifier starting from 1 |
| `title` | string | Concise action label (3-7 words) |
| `status` | string | `not-started`, `in-progress`, or `completed` |

## Rules

- Provide the FULL array on every call (not deltas)
- At most ONE item may be `in-progress` at a time
- Mark items `completed` immediately when finished

## Example

```json
{
  "taskList": [
    {"id": 1, "title": "Create data model", "status": "completed"},
    {"id": 2, "title": "Register tool", "status": "in-progress"},
    {"id": 3, "title": "Build UI widget", "status": "not-started"}
  ]
}
```
