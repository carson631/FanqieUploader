"""对已开启 --remote-debugging-port 的浏览器做一次真机勘察。

用法：
    python cdp_probe.py <port> <js文件> [表达式 ...]

会挑第一个 http(s) 页 target，注入 <js文件>（比如 js/site.js），
再依次执行给定表达式并把结果 JSON 打到标准输出。
"""
import json
import sys
import urllib.request

import websocket


def targets(port):
    # 本机环境有系统代理，必须强制直连 127.0.0.1
    opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
    with opener.open(f"http://127.0.0.1:{port}/json/list", timeout=5) as r:
        return json.load(r)


def main():
    port = int(sys.argv[1])
    js_path = sys.argv[2]
    exprs = sys.argv[3:]
    page = None
    for t in targets(port):
        if t.get("type") == "page" and str(t.get("url", "")).startswith("http"):
            page = t
            break
    if not page:
        print(json.dumps({"error": "no http page target"}))
        return 1

    ws = websocket.create_connection(page["webSocketDebuggerUrl"], timeout=30)
    mid = [0]

    def call(method, params):
        mid[0] += 1
        ws.send(json.dumps({"id": mid[0], "method": method, "params": params}))
        while True:
            msg = json.loads(ws.recv())
            if msg.get("id") == mid[0]:
                return msg

    r = call("Runtime.evaluate", {"expression": open(js_path, encoding="utf-8").read(), "returnByValue": True})
    if r.get("result", {}).get("exceptionDetails"):
        print(json.dumps({"inject": r["result"]["exceptionDetails"]}, ensure_ascii=False))
        return 1
    print(json.dumps({"url": page["url"], "inject": r["result"]["result"].get("value")}, ensure_ascii=False))

    for e in exprs:
        r = call("Runtime.evaluate", {"expression": e, "awaitPromise": True, "returnByValue": True})
        res = r.get("result", {})
        if res.get("exceptionDetails"):
            print(json.dumps({"expr": e, "error": res["exceptionDetails"]}, ensure_ascii=False))
            continue
        print(json.dumps({"expr": e, "value": res.get("result", {}).get("value")}, ensure_ascii=False))
    ws.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
