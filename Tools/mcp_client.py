"""Talk to Blender MCP socket (mcp-for-blender addon protocol)."""
import json
import socket
import sys
import time


def call(cmd, timeout=60, host="127.0.0.1", port=9877):
    s = socket.create_connection((host, port), timeout=timeout)
    try:
        s.sendall(json.dumps(cmd).encode("utf-8"))
        s.settimeout(timeout)
        chunks = []
        while True:
            data = s.recv(65536)
            if not data:
                break
            chunks.append(data)
            try:
                return json.loads(b"".join(chunks).decode("utf-8"))
            except Exception:
                continue
        raw = b"".join(chunks)
        return {"status": "eof", "raw": raw.decode("utf-8", "replace")[:2000]}
    finally:
        s.close()


def execute(code, timeout=120):
    return call({"type": "execute_code", "params": {"code": code}}, timeout=timeout)


if __name__ == "__main__":
    cmd = sys.argv[1] if len(sys.argv) > 1 else "ping"
    if cmd == "ping":
        print(json.dumps(call({"type": "ping", "params": {}}), ensure_ascii=False, indent=2))
    elif cmd == "scene":
        print(json.dumps(call({"type": "get_scene_info", "params": {}}), ensure_ascii=False, indent=2)[:3000])
    elif cmd == "eval":
        code = sys.argv[2]
        print(json.dumps(execute(code), ensure_ascii=False, indent=2)[:5000])
    else:
        print("usage: mcp_client.py [ping|scene|eval CODE]")
