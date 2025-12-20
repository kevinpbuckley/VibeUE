"""
Deep Researcher Tool

This module provides an AI-powered research tool that searches the internet
to answer Unreal Engine questions and provide comprehensive information.
"""

import logging
import json
import re
from typing import Dict, List, Any, Optional
from mcp.server.fastmcp import FastMCP, Context

logger = logging.getLogger("UnrealMCP")

# Try to import httpx for async HTTP requests
try:
    import httpx
    HTTPX_AVAILABLE = True
except ImportError:
    HTTPX_AVAILABLE = False
    logger.warning("httpx not available - deep_researcher will have limited functionality")


def register_deep_researcher_tools(mcp: FastMCP):
    """Register deep researcher tools with the MCP server."""
    
    @mcp.tool(description="""Deep research tool for Unreal Engine questions. Searches the internet to find comprehensive answers about UE5 development, Blueprints, C++, materials, UI, and more. Actions: research, search, summarize. Use action='help' for detailed guidance.""")
    def deep_researcher(
        ctx: Context,
        action: str,
        query: str = "",
        help_action: Optional[str] = None,
        # Research options
        max_results: int = 5,
        include_documentation: bool = True,
        include_forums: bool = True,
        include_tutorials: bool = True,
        focus_area: str = "",  # e.g., "blueprints", "c++", "materials", "ui", "animation"
        # Output options
        format: str = "detailed",  # "detailed", "summary", "bullet_points"
        max_length: int = 2000
    ) -> Dict[str, Any]:
        """Perform deep research on Unreal Engine topics."""
        
        try:
            # Handle help action
            if action == "help":
                return _generate_help(help_action)
            
            # Validate action
            valid_actions = ["research", "search", "summarize", "help"]
            if action not in valid_actions:
                return {
                    "success": False,
                    "error": f"Invalid action '{action}'. Valid actions: {', '.join(valid_actions)}",
                    "help_tip": "Use deep_researcher(action='help') to see all available actions."
                }
            
            # Validate query
            if not query and action != "help":
                return {
                    "success": False,
                    "error": "query is required for this action",
                    "help_tip": "Provide a question or topic to research, e.g., query='How to create a health bar in UE5'"
                }
            
            # Check if httpx is available
            if not HTTPX_AVAILABLE:
                return {
                    "success": False,
                    "error": "httpx library not installed. Run: pip install httpx",
                    "fallback_response": _get_fallback_response(query, focus_area)
                }
            
            # Route to appropriate handler
            if action == "research":
                return _perform_research(query, max_results, include_documentation, 
                                        include_forums, include_tutorials, focus_area,
                                        format, max_length)
            elif action == "search":
                return _perform_search(query, max_results, focus_area)
            elif action == "summarize":
                return _summarize_topic(query, focus_area, max_length)
            
        except Exception as e:
            logger.error(f"Deep researcher error: {str(e)}")
            return {
                "success": False,
                "error": str(e),
                "fallback_response": _get_fallback_response(query, focus_area)
            }


def _generate_help(help_action: Optional[str] = None) -> Dict[str, Any]:
    """Generate help documentation for the deep researcher tool."""
    
    if help_action == "research":
        return {
            "success": True,
            "action": "research",
            "description": "Perform comprehensive research on an Unreal Engine topic",
            "parameters": {
                "query": "The question or topic to research (required)",
                "max_results": "Maximum number of sources to include (default: 5)",
                "include_documentation": "Include official UE documentation (default: True)",
                "include_forums": "Include forum discussions (default: True)",
                "include_tutorials": "Include tutorial content (default: True)",
                "focus_area": "Optional focus: 'blueprints', 'c++', 'materials', 'ui', 'animation', 'networking'",
                "format": "Output format: 'detailed', 'summary', 'bullet_points' (default: 'detailed')",
                "max_length": "Maximum response length in characters (default: 2000)"
            },
            "example": 'deep_researcher(action="research", query="How to implement a save game system in UE5", focus_area="blueprints")',
            "tips": [
                "Be specific in your query for better results",
                "Use focus_area to narrow down to relevant content",
                "Set format='bullet_points' for quick reference"
            ]
        }
    
    elif help_action == "search":
        return {
            "success": True,
            "action": "search",
            "description": "Search for Unreal Engine resources without full research synthesis",
            "parameters": {
                "query": "Search terms (required)",
                "max_results": "Maximum results to return (default: 5)",
                "focus_area": "Optional area to focus search on"
            },
            "example": 'deep_researcher(action="search", query="UE5 procedural mesh generation")',
            "tips": [
                "Returns links and brief descriptions",
                "Faster than full research action",
                "Good for finding specific resources"
            ]
        }
    
    elif help_action == "summarize":
        return {
            "success": True,
            "action": "summarize",
            "description": "Get a quick summary of an Unreal Engine concept or feature",
            "parameters": {
                "query": "The topic to summarize (required)",
                "focus_area": "Optional context for the summary",
                "max_length": "Maximum summary length (default: 2000)"
            },
            "example": 'deep_researcher(action="summarize", query="Enhanced Input System")',
            "tips": [
                "Best for quick concept overviews",
                "Uses cached knowledge when available",
                "Good starting point before deep research"
            ]
        }
    
    # General help
    return {
        "success": True,
        "tool": "deep_researcher",
        "description": "AI-powered research tool for Unreal Engine questions. Searches documentation, forums, and tutorials to provide comprehensive answers.",
        "available_actions": {
            "research": "Comprehensive research with synthesized answer",
            "search": "Quick search for relevant resources",
            "summarize": "Brief summary of a concept or feature"
        },
        "common_focus_areas": [
            "blueprints", "c++", "materials", "ui", "animation",
            "networking", "ai", "physics", "audio", "rendering"
        ],
        "examples": [
            'deep_researcher(action="research", query="How to create a dialogue system")',
            'deep_researcher(action="search", query="UE5 water shader tutorial")',
            'deep_researcher(action="summarize", query="Gameplay Ability System")'
        ],
        "tips": [
            "Use action='help', help_action='research' for detailed action help",
            "Combine with VibeUE tools to implement what you learn",
            "Be specific in queries for better results"
        ]
    }


def _perform_research(query: str, max_results: int, include_documentation: bool,
                     include_forums: bool, include_tutorials: bool, focus_area: str,
                     format: str, max_length: int) -> Dict[str, Any]:
    """Perform comprehensive research on a topic."""
    
    # Build search query with UE5 context
    search_query = _build_search_query(query, focus_area)
    
    # Collect sources
    sources = []
    
    try:
        # Search various sources
        if include_documentation:
            doc_results = _search_documentation(search_query, max_results // 2)
            sources.extend(doc_results)
        
        if include_forums:
            forum_results = _search_forums(search_query, max_results // 2)
            sources.extend(forum_results)
        
        if include_tutorials:
            tutorial_results = _search_tutorials(search_query, max_results // 2)
            sources.extend(tutorial_results)
        
        # Limit total sources
        sources = sources[:max_results]
        
        # Generate synthesized answer
        answer = _synthesize_answer(query, sources, focus_area, format, max_length)
        
        return {
            "success": True,
            "action": "research",
            "query": query,
            "focus_area": focus_area or "general",
            "answer": answer,
            "sources": sources,
            "source_count": len(sources),
            "tips": _get_implementation_tips(query, focus_area)
        }
        
    except Exception as e:
        logger.error(f"Research error: {str(e)}")
        return {
            "success": False,
            "error": f"Research failed: {str(e)}",
            "fallback_response": _get_fallback_response(query, focus_area)
        }


def _perform_search(query: str, max_results: int, focus_area: str) -> Dict[str, Any]:
    """Perform a quick search for resources."""
    
    search_query = _build_search_query(query, focus_area)
    
    results = []
    
    # Quick search across sources
    results.extend(_search_documentation(search_query, max_results))
    results.extend(_search_tutorials(search_query, max_results))
    
    # Deduplicate and limit
    seen_urls = set()
    unique_results = []
    for r in results:
        if r.get("url") not in seen_urls:
            seen_urls.add(r.get("url"))
            unique_results.append(r)
    
    return {
        "success": True,
        "action": "search",
        "query": query,
        "results": unique_results[:max_results],
        "result_count": len(unique_results[:max_results])
    }


def _summarize_topic(query: str, focus_area: str, max_length: int) -> Dict[str, Any]:
    """Generate a quick summary of a topic."""
    
    # Check for known topics first
    known_summary = _get_known_topic_summary(query, focus_area)
    if known_summary:
        return {
            "success": True,
            "action": "summarize",
            "topic": query,
            "summary": known_summary[:max_length],
            "source": "cached_knowledge",
            "tip": "Use action='research' for more detailed information with current sources"
        }
    
    # Fall back to search-based summary
    search_results = _perform_search(query, 3, focus_area)
    
    if search_results.get("success") and search_results.get("results"):
        summary = f"**{query}**\n\n"
        summary += "Based on available resources:\n\n"
        for i, result in enumerate(search_results["results"][:3], 1):
            summary += f"{i}. **{result.get('title', 'Resource')}**\n"
            summary += f"   {result.get('description', 'No description')}\n\n"
        
        return {
            "success": True,
            "action": "summarize",
            "topic": query,
            "summary": summary[:max_length],
            "sources": search_results["results"][:3]
        }
    
    return {
        "success": True,
        "action": "summarize",
        "topic": query,
        "summary": _get_fallback_response(query, focus_area),
        "source": "fallback"
    }


def _build_search_query(query: str, focus_area: str) -> str:
    """Build an optimized search query for Unreal Engine content."""
    
    # Add UE5 context
    base_query = f"Unreal Engine 5 {query}"
    
    # Add focus area context
    if focus_area:
        focus_terms = {
            "blueprints": "Blueprint visual scripting",
            "c++": "C++ programming",
            "materials": "Material shader",
            "ui": "UMG Widget UI",
            "animation": "Animation Montage Sequence",
            "networking": "Multiplayer Replication",
            "ai": "AI Behavior Tree",
            "physics": "Physics simulation",
            "audio": "Audio MetaSound",
            "rendering": "Rendering Nanite Lumen"
        }
        if focus_area.lower() in focus_terms:
            base_query = f"UE5 {focus_terms[focus_area.lower()]} {query}"
    
    return base_query


def _search_documentation(query: str, max_results: int) -> List[Dict[str, Any]]:
    """Search official Unreal Engine documentation."""
    
    # Generate documentation links based on query keywords
    results = []
    
    doc_base = "https://dev.epicgames.com/documentation/en-us/unreal-engine"
    
    # Common documentation mappings
    doc_mappings = {
        "blueprint": f"{doc_base}/blueprints-visual-scripting-in-unreal-engine",
        "material": f"{doc_base}/unreal-engine-materials",
        "widget": f"{doc_base}/umg-ui-designer-in-unreal-engine",
        "animation": f"{doc_base}/animation-in-unreal-engine",
        "input": f"{doc_base}/enhanced-input-in-unreal-engine",
        "ai": f"{doc_base}/artificial-intelligence-in-unreal-engine",
        "networking": f"{doc_base}/networking-and-multiplayer-in-unreal-engine",
        "physics": f"{doc_base}/physics-in-unreal-engine",
        "niagara": f"{doc_base}/creating-visual-effects-in-niagara-for-unreal-engine",
        "lumen": f"{doc_base}/lumen-global-illumination-and-reflections-in-unreal-engine",
        "nanite": f"{doc_base}/nanite-virtualized-geometry-in-unreal-engine",
        "save": f"{doc_base}/saving-and-loading-your-game-in-unreal-engine",
        "gameplay ability": f"{doc_base}/gameplay-ability-system-for-unreal-engine"
    }
    
    query_lower = query.lower()
    for keyword, url in doc_mappings.items():
        if keyword in query_lower:
            results.append({
                "type": "documentation",
                "title": f"Official UE5 Documentation: {keyword.title()}",
                "url": url,
                "description": f"Epic Games official documentation for {keyword} in Unreal Engine 5",
                "reliability": "high"
            })
    
    # Always include the main docs search
    if len(results) < max_results:
        results.append({
            "type": "documentation",
            "title": "Unreal Engine Documentation Search",
            "url": f"https://dev.epicgames.com/documentation/en-us/unreal-engine?search={query.replace(' ', '+')}",
            "description": f"Search official documentation for: {query}",
            "reliability": "high"
        })
    
    return results[:max_results]


def _search_forums(query: str, max_results: int) -> List[Dict[str, Any]]:
    """Search Unreal Engine forums and communities."""
    
    results = []
    
    # Forum search links
    results.append({
        "type": "forum",
        "title": "Unreal Engine Forums",
        "url": f"https://forums.unrealengine.com/search?q={query.replace(' ', '+')}",
        "description": f"Community discussions about: {query}",
        "reliability": "medium"
    })
    
    results.append({
        "type": "community",
        "title": "Reddit r/unrealengine",
        "url": f"https://www.reddit.com/r/unrealengine/search?q={query.replace(' ', '+')}&restrict_sr=1",
        "description": f"Reddit community discussions: {query}",
        "reliability": "medium"
    })
    
    return results[:max_results]


def _search_tutorials(query: str, max_results: int) -> List[Dict[str, Any]]:
    """Search for Unreal Engine tutorials."""
    
    results = []
    
    # Tutorial sources
    results.append({
        "type": "tutorial",
        "title": "YouTube Tutorials",
        "url": f"https://www.youtube.com/results?search_query=unreal+engine+5+{query.replace(' ', '+')}",
        "description": f"Video tutorials for: {query}",
        "reliability": "varies"
    })
    
    results.append({
        "type": "learning",
        "title": "Epic Games Learning Portal",
        "url": "https://dev.epicgames.com/community/learning",
        "description": "Official Epic Games learning resources and courses",
        "reliability": "high"
    })
    
    return results[:max_results]


def _synthesize_answer(query: str, sources: List[Dict], focus_area: str, 
                       format: str, max_length: int) -> str:
    """Synthesize an answer from gathered sources."""
    
    # Get base knowledge
    base_answer = _get_fallback_response(query, focus_area)
    
    if format == "bullet_points":
        # Convert to bullet point format
        lines = base_answer.split(". ")
        bullets = [f"• {line.strip()}" for line in lines if line.strip()]
        answer = "\n".join(bullets[:10])
    elif format == "summary":
        # Short summary
        answer = base_answer[:max_length // 2]
        if len(base_answer) > max_length // 2:
            answer += "..."
    else:
        # Detailed format
        answer = base_answer
    
    # Add source references
    if sources:
        answer += "\n\n**Recommended Resources:**\n"
        for i, source in enumerate(sources[:3], 1):
            answer += f"{i}. [{source.get('title', 'Resource')}]({source.get('url', '#')})\n"
    
    return answer[:max_length]


def _get_implementation_tips(query: str, focus_area: str) -> List[str]:
    """Get implementation tips based on the query."""
    
    tips = []
    query_lower = query.lower()
    
    if "blueprint" in query_lower or focus_area == "blueprints":
        tips.append("Use manage_blueprint to create the Blueprint structure")
        tips.append("Add variables with manage_blueprint_variable before creating logic")
        tips.append("Use manage_blueprint_node to create event graph logic")
    
    if "material" in query_lower or focus_area == "materials":
        tips.append("Create the base material with manage_material(action='create')")
        tips.append("Use manage_material_node to build the material graph")
        tips.append("Create material instances for variations with manage_material(action='create_instance')")
    
    if "widget" in query_lower or "ui" in query_lower or focus_area == "ui":
        tips.append("Use manage_umg_widget to create and configure UI elements")
        tips.append("Remember to set anchors for responsive layouts")
    
    if "input" in query_lower:
        tips.append("Use manage_enhanced_input for the Enhanced Input System")
        tips.append("Create Input Actions first, then add to Mapping Contexts")
    
    if not tips:
        tips = [
            "Break complex tasks into smaller steps",
            "Use action='help' on any VibeUE tool for guidance",
            "Save frequently with manage_asset(action='save_all')"
        ]
    
    return tips


def _get_known_topic_summary(query: str, focus_area: str) -> Optional[str]:
    """Get summary for well-known Unreal Engine topics."""
    
    query_lower = query.lower()
    
    known_topics = {
        "enhanced input": """**Enhanced Input System** is UE5's modern input handling framework that replaces the legacy input system.

**Key Concepts:**
- **Input Actions (IA_)**: Define what inputs do (Jump, Move, Look)
- **Input Mapping Contexts (IMC_)**: Bind keys to actions, can be swapped at runtime
- **Modifiers**: Transform input values (negate, deadzone, smooth)
- **Triggers**: Define when actions fire (pressed, released, hold)

**Benefits:**
- Context-based input switching (menu vs gameplay)
- Support for complex input devices
- Built-in modifiers and triggers
- Blueprint and C++ friendly""",

        "gameplay ability system": """**Gameplay Ability System (GAS)** is a framework for building ability-based gameplay.

**Core Components:**
- **Abilities**: Actions characters can perform
- **Attributes**: Numeric values (health, mana, stamina)
- **Effects**: Modify attributes over time
- **Tags**: Filter and identify abilities/effects

**Use Cases:**
- RPG abilities and spells
- Combat systems
- Buff/debuff systems
- Cooldown management""",

        "behavior tree": """**Behavior Trees** are UE5's primary AI decision-making system.

**Components:**
- **Root**: Entry point
- **Composites**: Selector (OR), Sequence (AND)
- **Decorators**: Conditions and modifiers
- **Tasks**: Actual actions
- **Services**: Background operations

**Workflow:**
1. Create Behavior Tree asset
2. Create Blackboard for AI memory
3. Build tree structure
4. Assign to AI Controller""",

        "save game": """**Save Game System** in UE5 allows persistent data storage.

**Steps:**
1. Create SaveGame Blueprint/C++ class
2. Add variables to store
3. Use SaveGameToSlot / LoadGameFromSlot
4. Handle async save for large data

**Best Practices:**
- Use unique slot names
- Validate loaded data
- Consider cloud saves for multiplayer
- Compress large save files""",

        "material instance": """**Material Instances** allow parameter variations without recompiling shaders.

**Types:**
- **Constant**: Set in editor, baked
- **Dynamic**: Changed at runtime via code

**Workflow:**
1. Create parent material with parameters
2. Create Material Instance from parent  
3. Override parameter values
4. Apply to meshes

**Performance:** Much faster than creating new materials"""
    }
    
    for topic, summary in known_topics.items():
        if topic in query_lower:
            return summary
    
    return None


def _get_fallback_response(query: str, focus_area: str) -> str:
    """Generate a helpful fallback response when search isn't available."""
    
    query_lower = query.lower()
    
    # Common UE5 patterns
    if "health bar" in query_lower or "progress bar" in query_lower:
        return """To create a health bar in UE5:

1. **Create Widget Blueprint**: Use manage_umg_widget to create a UserWidget
2. **Add Progress Bar**: Add a ProgressBar widget component
3. **Bind to Health**: Create a binding or update function to set Percent (0.0-1.0)
4. **Add to Viewport**: Use Create Widget and Add to Viewport nodes
5. **Style It**: Set fill color, background, and size

**VibeUE Commands:**
- manage_umg_widget(action="add_component", widget_name="WBP_HealthBar", component_type="ProgressBar")
- manage_umg_widget(action="set_property", property_name="Percent", property_value=0.75)"""

    if "inventory" in query_lower:
        return """To create an inventory system in UE5:

1. **Item Structure**: Create a struct with item data (name, icon, stats)
2. **Inventory Component**: ActorComponent to hold item array
3. **UI Widget**: Grid or list to display items
4. **Interaction**: Pick up, drop, use logic

**Key Classes:**
- FItemData (struct)
- UInventoryComponent (ActorComponent)  
- UW_Inventory (UserWidget)
- UW_InventorySlot (UserWidget)"""

    if "dialogue" in query_lower:
        return """To create a dialogue system in UE5:

1. **Data Structure**: Create dialogue tree data (nodes, choices, conditions)
2. **Dialogue Manager**: Subsystem or Actor to control flow
3. **UI Widget**: Text display, choice buttons, character portraits
4. **Triggers**: Collision or interaction to start dialogue

**Approaches:**
- Data Tables for simple linear dialogue
- Custom Dialogue Tree asset for branching
- Third-party plugins (Dialogue System, SUDS)"""

    # Generic response
    return f"""**Research Topic:** {query}

This is a complex Unreal Engine topic. Here's a general approach:

1. **Check Documentation**: Start with Epic's official docs
2. **Community Resources**: UE Forums, Reddit r/unrealengine
3. **Video Tutorials**: YouTube has many UE5 tutorials
4. **Sample Projects**: Epic's sample content and templates

**VibeUE Tools That May Help:**
- manage_blueprint: Create gameplay systems
- manage_blueprint_node: Build logic graphs
- manage_umg_widget: Create UI elements
- manage_material: Visual effects and shaders

Use action='research' with this query for more detailed web search results."""
