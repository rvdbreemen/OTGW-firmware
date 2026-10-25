"""Header echo backend for the ingress rig (TASK-1378).

Stands in for the OTGW to record exactly which Host, Origin, Referer and
X-Forwarded-* headers a proxy passes to the device. GET returns a small page
that POSTs to ./probe from the browser (so the browser sets Origin), POST
returns the request headers as JSON. Every request is also printed to stdout
(`docker logs otgw-rig-echo`).
"""
import json
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

PAGE = b"""<!doctype html><html><body><pre id="out">waiting</pre>
<script>
fetch('probe', {method: 'POST', headers: {'Content-Type': 'application/json'}, body: '{}'})
  .then(r => r.json()).then(j => { document.getElementById('out').textContent = JSON.stringify(j); })
  .catch(e => { document.getElementById('out').textContent = 'ERR ' + e; });
</script></body></html>"""


class Handler(BaseHTTPRequestHandler):
    def _record(self):
        rec = {"method": self.command, "path": self.path, "headers": {k: v for k, v in self.headers.items()}}
        print(json.dumps(rec), flush=True)
        return rec

    def do_GET(self):
        self._record()
        self.send_response(200)
        self.send_header("Content-Type", "text/html")
        self.send_header("Content-Length", str(len(PAGE)))
        self.end_headers()
        self.wfile.write(PAGE)

    def do_POST(self):
        length = int(self.headers.get("Content-Length") or 0)
        if length:
            self.rfile.read(length)
        body = json.dumps(self._record()).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *args):
        pass


ThreadingHTTPServer(("0.0.0.0", 8080), Handler).serve_forever()
