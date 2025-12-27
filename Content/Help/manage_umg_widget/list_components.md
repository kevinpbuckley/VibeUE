# list_components

List all widget components in a Widget Blueprint.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| WidgetPath | string | Yes | Content path to the Widget Blueprint |

## Examples

### List Components
```json
{
  "Action": "list_components",
  "ParamsJson": "{\"WidgetPath\": \"/Game/UI/WBP_MainMenu\"}"
}
```

## Returns

```json
{
  "Success": true,
  "WidgetPath": "/Game/UI/WBP_MainMenu",
  "Components": [
    {
      "Name": "CanvasPanel_Root",
      "Type": "CanvasPanel",
      "Parent": null,
      "IsVariable": true
    },
    {
      "Name": "TitleText",
      "Type": "TextBlock",
      "Parent": "CanvasPanel_Root",
      "IsVariable": true
    },
    {
      "Name": "PlayButton",
      "Type": "Button",
      "Parent": "CanvasPanel_Root",
      "IsVariable": true
    },
    {
      "Name": "PlayButtonText",
      "Type": "TextBlock",
      "Parent": "PlayButton",
      "IsVariable": false
    }
  ],
  "Count": 4
}
```

## Tips

- Shows parent-child relationships
- IsVariable indicates if the component can be accessed in Blueprint
- Button widgets typically have a child TextBlock
- Use get_info for more detailed information
