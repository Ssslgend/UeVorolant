"""Start Blender MCP addon server for Blender 5.1.

Usage:
  blender -P Tools/start_blender_mcp.py
"""

import bpy


def enable_and_start():
    import addon_utils

    # Enable the MCP for Blender addon.
    try:
        addon_utils.enable("blender_mcp", default_set=True, persistent=True)
    except Exception as exc:
        print(f"[MCP] enable failed: {exc}")

    # Prefer the operator so the socket server actually starts.
    scene = bpy.context.scene
    if not hasattr(scene, "blendermcp_port"):
        import bpy as _bpy
        _bpy.types.Scene.blendermcp_port = _bpy.props.IntProperty(default=9877, min=1024, max=65535)
    if hasattr(scene, "blendermcp_auto_start_server"):
        scene.blendermcp_auto_start_server = True
    # Correct property is blendermcp_port (NOT blendermcp_server_port).
    # 9876 is often taken by ddns-go → WinError 10013; use 9877.
    scene.blendermcp_port = 9877

    if hasattr(bpy.ops, "blendermcp"):
        try:
            result = bpy.ops.blendermcp.start_server()
            print(f"[MCP] start_server result: {result}")
        except Exception as exc:
            print(f"[MCP] start_server failed: {exc}")

    # Fallback: instantiate the server class directly if operator unavailable.
    try:
        import importlib
        mod = importlib.import_module("blender_mcp")
        server = getattr(bpy.types, "blendermcp_server", None)
        if server is None and hasattr(mod, "BlenderMCPServer"):
            server = mod.BlenderMCPServer(host="localhost", port=9877)
            bpy.types.blendermcp_server = server
            server.start()
            print("[MCP] fallback server start on 9877")
        elif server is not None and not getattr(server, "running", False):
            server.port = 9877
            server.start()
            print("[MCP] restarted existing server on 9877")
    except Exception as exc:
        print(f"[MCP] fallback failed: {exc}")

    print("[MCP] bootstrap complete")
    return None


# Delay until UI/context is ready.
bpy.app.timers.register(enable_and_start, first_interval=1.0)
print("[MCP] bootstrap scheduled")
