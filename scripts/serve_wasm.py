#!/usr/bin/env python3
import http.server
import socketserver
import os
import sys
import subprocess
import platform
import time
import threading
from pathlib import Path

SUPPORTED_TARGETS = [
    "wasm-debug",
    "wasm-release",
    "wasm-publish",
]


def auto_detect_build_type(project_root):
    build_dirs = {}

    for target in SUPPORTED_TARGETS:
        target_dir = project_root / "build" / target / "webgpu_app"
        if target_dir.exists():
            build_dirs[target] = target_dir.stat().st_mtime

    if not build_dirs:
        return None

    most_recent_target = max(build_dirs.items(), key=lambda x: x[1])[0]
    return most_recent_target


def get_html_file(build_dir):
    html_file = build_dir / "webgpu_app.html"
    return html_file if html_file.exists() else None


def get_css_files(build_dir):
    return list(build_dir.glob("*.css"))


def open_browser(url):
    try:
        if platform.system() == "Windows":
            subprocess.Popen(["cmd", "/c", "start", url], shell=True)
        elif platform.system() == "Darwin":
            subprocess.Popen(["open", url])
        else:
            subprocess.Popen(["xdg-open", url])
    except Exception as e:
        print(f"Could not open browser: {e}")
        print(f"Open manually: {url}\n")


def serve_wasm(port=8000):
    project_root = Path(__file__).parent.parent

    while True:
        build_type = auto_detect_build_type(project_root)
        if build_type is None:
            print("Error: No WebAssembly build found.")
            sys.exit(1)

        print(f"Auto-detected build target: {build_type}")
        build_dir = project_root / "build" / build_type / "webgpu_app"

        html_file = get_html_file(build_dir)
        if not html_file:
            print(f"Error: No HTML file found in {build_dir}")
            sys.exit(1)

        css_files = get_css_files(build_dir)
        css_mtimes = {f: f.stat().st_mtime for f in css_files}

        last_mtime = html_file.stat().st_mtime
        last_build_type = build_type
        pending_change_time = None
        should_restart = False

        os.chdir(build_dir)

        class WasmHandler(http.server.SimpleHTTPRequestHandler):
            def end_headers(self):
                self.send_header("Cross-Origin-Opener-Policy", "same-origin")
                self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
                # Disable all caching
                self.send_header("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0")
                self.send_header("Pragma", "no-cache")
                self.send_header("Expires", "0")
                super().end_headers()

            extensions_map = {
                **http.server.SimpleHTTPRequestHandler.extensions_map,
                '.wasm': 'application/wasm',
                '.js': 'application/javascript',
            }

            def log_message(self, format, *args):
                pass

        def monitor_changes():
            nonlocal last_mtime, last_build_type, build_dir, html_file, css_files, css_mtimes, pending_change_time, should_restart

            while True:
                time.sleep(1)

                current_build_type = auto_detect_build_type(project_root)
                if current_build_type != last_build_type:
                    print(f"\n[RELOAD] Build type changed: {last_build_type} -> {current_build_type}")
                    print("Restarting server...\n")
                    should_restart = True
                    httpd.shutdown()
                    return

                if html_file.exists():
                    current_mtime = html_file.stat().st_mtime
                    if current_mtime != last_mtime:
                        pending_change_time = time.time()
                        last_mtime = current_mtime
                    elif pending_change_time is not None:
                        if time.time() - pending_change_time >= 1.0:
                            print(f"\n[RELOAD] Build completed, opening browser...")
                            pending_change_time = None
                            url = f"http://localhost:{port}/webgpu_app.html"
                            open_browser(url)

                current_css_files = get_css_files(build_dir)
                for css_file in current_css_files:
                    if css_file.exists():
                        current_mtime = css_file.stat().st_mtime
                        if css_file not in css_mtimes or current_mtime != css_mtimes[css_file]:
                            pending_change_time = time.time()
                            css_mtimes[css_file] = current_mtime

        with socketserver.TCPServer(("", port), WasmHandler) as httpd:
            url = f"http://localhost:{port}/webgpu_app.html"
            print(f"Serving: {build_dir}")
            print(f"Monitoring: {html_file.name}")
            if css_files:
                print(f"Monitoring CSS: {[f.name for f in css_files]}")
            print(f"Opening: {url}\n")

            open_browser(url)

            monitor_thread = threading.Thread(target=monitor_changes, daemon=True)
            monitor_thread.start()

            try:
                httpd.serve_forever()
            except KeyboardInterrupt:
                print("\nServer stopped.")
                sys.exit(0)

        if not should_restart:
            break


if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8000
    serve_wasm(port)
