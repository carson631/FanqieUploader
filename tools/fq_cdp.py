"""极简 CDP 客户端：可以对某个 target 发任意 CDP 命令。

用法：
    python tools/fq_cdp.py <port> list
    python tools/fq_cdp.py <port> eval <urlPart> <js文件> [表达式 ...]
    python tools/fq_cdp.py <port> click <urlPart> <文本>      # 真实鼠标点击（带 user gesture）
"""
import json
import sys
import time
import urllib.request

import websocket


def op():
    return urllib.request.build_opener(urllib.request.ProxyHandler({}))


def targets(port):
    with op().open(f"http://127.0.0.1:{port}/json/list", timeout=5) as r:
        return json.load(r)


class Tab:
    def __init__(self, t):
        self.t = t
        self.ws = websocket.create_connection(t["webSocketDebuggerUrl"], timeout=40)
        self.mid = 0

    def call(self, method, params=None):
        self.mid += 1
        self.ws.send(json.dumps({"id": self.mid, "method": method, "params": params or {}}))
        while True:
            m = json.loads(self.ws.recv())
            if m.get("id") == self.mid:
                return m

    def ev(self, expr, await_promise=True):
        r = self.call("Runtime.evaluate",
                      {"expression": expr, "returnByValue": True, "awaitPromise": await_promise})
        res = r.get("result", {})
        if res.get("exceptionDetails"):
            return "EXC:" + json.dumps(res["exceptionDetails"], ensure_ascii=False)[:400]
        return res.get("result", {}).get("value")

    def close(self):
        try:
            self.ws.close()
        except Exception:
            pass


def find(port, part):
    pages = [t for t in targets(port) if t.get("type") == "page"]
    for p in pages:
        if part in (p.get("url") or ""):
            return p
    return None


def main():
    port = int(sys.argv[1])
    cmd = sys.argv[2]
    if cmd == "list":
        for t in targets(port):
            print(t.get("type"), "|", (t.get("url") or "")[:140])
        return 0

    part = sys.argv[3]
    t = find(port, part)
    if not t:
        print(json.dumps({"error": "no target", "part": part}, ensure_ascii=False))
        return 1
    tab = Tab(t)

    if cmd == "eval":
        js = open(sys.argv[4], encoding="utf-8").read()
        print(json.dumps({"url": t["url"][:120], "inject": tab.ev(js)}, ensure_ascii=False))
        for e in sys.argv[5:]:
            v = tab.ev(e)
            print(json.dumps({"expr": e, "value": v}, ensure_ascii=False, default=str))
        tab.close()
        return 0

    if cmd == "click":
        text = sys.argv[4]
        box = tab.ev(
            "(function(){const b=[...document.querySelectorAll('button,[role=\"button\"],a,div,span')]"
            ".filter(e=>{const r=e.getBoundingClientRect();return r.width>2&&r.height>2;})"
            ".filter(e=>(e.innerText||'').trim()===" + json.dumps(text) + ");"
            "if(!b.length)return null;const r=b[b.length-1].getBoundingClientRect();"
            "return {x:r.x+r.width/2,y:r.y+r.height/2};})()")
        if not box:
            print(json.dumps({"error": "element not found", "text": text}, ensure_ascii=False))
            return 1
        for typ in ("mousePressed", "mouseReleased"):
            tab.call("Input.dispatchMouseEvent",
                     {"type": typ, "x": box["x"], "y": box["y"],
                      "button": "left", "clickCount": 1})
            time.sleep(0.08)
        print(json.dumps({"clicked": text, "at": box}, ensure_ascii=False))
        tab.close()
        return 0
    print("unknown cmd")
    return 2


if __name__ == "__main__":
    sys.exit(main())
