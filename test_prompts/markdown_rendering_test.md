# Markdown Rendering Test

A single prompt designed to produce a response that exercises every markdown feature supported by the VibeUE In-Editor Chat. Copy/paste the prompt below into the chat window and verify the response renders correctly.

---

## The Prompt


Give me a comprehensive cheat sheet for Unreal Engine C++ best practices. Structure it EXACTLY like this:

1. Start with a level-1 heading "Unreal Engine C++ Cheat Sheet"
2. A short italic intro paragraph with a bold keyword in it
3. A horizontal rule separator
4. A level-2 heading "Memory Management" with a few bullet points (use bold for key terms, inline code for types like TSharedPtr)
5. A level-2 heading "Common Patterns" with a numbered list where some items have inline code and bold text mixed in
6. A blockquote with a tip about UPROPERTY, and a nested blockquote inside it with a warning
7. A horizontal rule separator
8. A level-3 heading "Actor Lifecycle" with a small markdown table (3 columns: Function, When Called, Notes) with at least 4 rows
9. A C++ code block (use the cpp language tag) showing a simple AActor subclass with BeginPlay and Tick
10. A Python code block (use the python language tag) showing a 3-line unreal script example
11. A level-2 heading "Quick Reference" with a table of 5 common macros (Macro, Purpose columns)
12. End with a blockquote containing a bold summary sentence and a link to the Unreal docs at https://docs.unrealengine.com

Keep each section short (2-4 lines max per bullet). This is a formatting reference, not a full tutorial.


---

## What To Verify

### Text Formatting
- [ ] **Bold** text renders with heavier weight
- [ ] *Italic* text renders with slant
- [ ] `Inline code` renders in green monospace
- [ ] [Links](https://example.com) render in cyan and are clickable

### Headers
- [ ] H1 renders large and bold (size 18)
- [ ] H2 renders medium bold (size 15)
- [ ] H3 renders smaller bold (size 13)

### Lists
- [ ] Bullet points show with bullet character and inline formatting preserved
- [ ] Numbered list shows with numbers and inline formatting preserved

### Code Blocks
- [ ] C++ code block has box-drawing border frame (top and bottom)
- [ ] Language label "cpp" appears in top border
- [ ] Python code block has "python" label in top border
- [ ] Code text is green monospace
- [ ] Streaming partial code blocks show top border but no bottom

### Tables
- [ ] Table headers render in bold monospace
- [ ] Table body renders in regular monospace
- [ ] Columns are aligned with consistent spacing
- [ ] Separator row renders as horizontal box-drawing line

### Blockquotes
- [ ] Blockquote text has blue vertical bar accent on left
- [ ] Blockquote text is slightly muted color
- [ ] Nested blockquotes show multiple vertical bars
- [ ] Inline formatting works inside blockquotes

### Horizontal Rules
- [ ] Renders as a thin muted horizontal line
- [ ] Clearly separates sections visually

---

## Minimal Quick Test Prompts

If you want to test individual features in isolation:

### Tables Only
Show me a markdown table with 3 columns (Name, Type, Default) and 4 rows of common UPROPERTY types.

### Blockquotes Only
Give me a tip as a blockquote, and inside that blockquote nest another blockquote with a warning. Use bold for emphasis.

### Code Blocks Only
Show me a short C++ code block and a short Python code block, each with their language tags.

### Horizontal Rules Only
List 3 unrelated Unreal tips, separated by horizontal rules (use ---).

### Mixed Formatting Stress Test
In a single bullet list of 5 items, use bold, italic, inline code, and a link - all mixed within the same list items.
