"""Serve the existing website with local Web build overlays, without modifying it."""
import argparse
import socket
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--site-root', type=Path, default=Path(r'G:\Coding\Projects\Akil0814.github.io'))
    parser.add_argument('--port', type=int, default=8765)
    parser.add_argument('--host', default='127.0.0.1')
    parser.add_argument('--fail-package', action='store_true', help='Local QA only: return 404 for the first data package')
    parser.add_argument('--disable-storage', action='store_true', help='Local QA only: simulate unavailable IndexedDB')
    args = parser.parse_args()
    site = args.site_root.resolve(strict=True)
    prefix = '/projects/FlyMe2TheMoon/web/'
    class Handler(SimpleHTTPRequestHandler):
        def translate_path(self, path):
            route = unquote(urlsplit(path).path)
            if route.startswith(prefix + 'runtime-portrait/'):
                base, suffix = ROOT / 'out/web-portrait/runtime', route[len(prefix + 'runtime-portrait/'):]
            elif route.startswith(prefix + 'runtime/'):
                base, suffix = site / 'projects/FlyMe2TheMoon/web/runtime', route[len(prefix + 'runtime/'):]
            elif route.startswith(prefix):
                base, suffix = ROOT / 'game/platform/web', route[len(prefix):] or 'index.html'
            elif route in [f'/i18n/{lang}/flyme2themoon_web.json' for lang in ['en', 'zh', 'ja']]:
                base, suffix = ROOT / 'game/platform/web', route[1:]
            else:
                base, suffix = site, route.lstrip('/')
            resolved = (base / suffix).resolve()
            if not resolved.is_relative_to(base.resolve()):
                return str(ROOT / 'out/nonexistent')
            return str(resolved)

        def do_GET(self):
            route = urlsplit(self.path).path
            if args.fail_package and route.endswith('assets-00.data'):
                self.send_error(404, 'Intentional QA missing resource')
                return
            if args.disable_storage and route.endswith('flyme2themoon_web.js'):
                content = b"Object.defineProperty(globalThis, 'indexedDB', {get() {throw new Error('Intentional QA blocked storage')}});\n"
                content += Path(self.translate_path(self.path)).read_bytes()
                self.send_response(200)
                self.send_header('Content-Type', 'text/javascript; charset=utf-8')
                self.send_header('Content-Length', str(len(content)))
                self.end_headers()
                self.wfile.write(content)
                return
            super().do_GET()

        def end_headers(self):
            self.send_header('Cache-Control', 'no-cache')
            super().end_headers()

    print(f'Local: http://127.0.0.1:{args.port}{prefix}', flush=True)
    if args.host=='0.0.0.0':
        for ip in sorted({item[4][0] for item in socket.getaddrinfo(socket.gethostname(),None,socket.AF_INET)}):
            if not ip.startswith('127.'):
                print(f'LAN: http://{ip}:{args.port}{prefix}',flush=True)
    ThreadingHTTPServer((args.host, args.port), Handler).serve_forever()


if __name__ == '__main__':
    main()
