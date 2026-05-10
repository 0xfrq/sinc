import argparse
import threading
import time
from http.server import HTTPServer, BaseHTTPRequestHandler
from urllib.parse import parse_qs
import ssl
import os


class PublisherState:
    def __init__(self):
        self.lock = threading.Lock()
        self.current_command = "NONE"
        self.last_event = None


g_state = PublisherState()


class PublisherHandler(BaseHTTPRequestHandler):
    def log_message(self, format, *args):
        """Custom logging to match original style"""
        print(f"  [http] {format % args}")

    def do_GET(self):
        """GET /command - client polls for next command"""
        if self.path == "/command":
            with g_state.lock:
                command = g_state.current_command
                # Reset to NONE after sending once
                if g_state.current_command != "NONE":
                    g_state.current_command = "NONE"

            # Build HTTP response
            response_body = command.encode()
            self.send_response(200)
            self.send_header("Content-Type", "text/plain")
            self.send_header("Content-Length", str(len(response_body)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(response_body)

            if command != "NONE":
                print(f"[poll] sent command: {command}")
        else:
            self.send_response(404)
            self.end_headers()

    def do_POST(self):
        """POST /event - client sends events (e.g., GAMEOVER)"""
        if self.path == "/event":
            content_length = int(self.headers.get("Content-Length", 0))
            body = self.rfile.read(content_length).decode(errors="ignore")

            print(f"[event] received from {self.client_address[0]}: {body}")

            # Parse the event
            if "GAMEOVER" in body or "event=ok" in body:
                print(f"  → GAMEOVER received, broadcasting SHOW to others")
                with g_state.lock:
                    g_state.current_command = "SHOW"
                    g_state.last_event = "GAMEOVER"

            self.send_response(200)
            self.send_header("Content-Type", "text/plain")
            self.send_header("Content-Length", "2")
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(b"OK")
        else:
            self.send_response(404)
            self.end_headers()


def interactive_loop(server_addr, server_port):
    """Interactive command loop"""
    print()
    print("Commands:")
    print("  show        – display image on all subscribers")
    print("  show2       – display image2 on all subscribers for 5 secs only")
    print("  hide        – hide image on all subscribers")
    print("  exit        – quit all subscribers then this server")
    print("  quit / q    – quit this server only (subscribers keep running)")
    print("  schedule    – demo: show for 5 s then hide automatically")
    print()

    while True:
        try:
            raw = input("publisher> ").strip().lower()
        except (EOFError, KeyboardInterrupt):
            print("\n[server] shutting down")
            break

        if raw == "show":
            with g_state.lock:
                g_state.current_command = "SHOW"
            print("  → SHOW sent (next poll)")

        elif raw == "show2":
            with g_state.lock:
                g_state.current_command = "SHOW2"
            print("  → SHOW2 sent (next poll)")

            def _auto_hide():
                time.sleep(5)
                with g_state.lock:
                    g_state.current_command = "HIDE"
                print("  → HIDE (auto) sent (next poll)")

            threading.Thread(target=_auto_hide, daemon=True).start()

        elif raw == "hide":
            with g_state.lock:
                g_state.current_command = "HIDE"
            print("  → HIDE sent (next poll)")

        elif raw == "exit":
            with g_state.lock:
                g_state.current_command = "EXIT"
            print("  → EXIT sent (next poll)")
            time.sleep(0.5)
            break

        elif raw in ("quit", "q"):
            break

        elif raw == "schedule":
            print("  → SHOW … (waiting 5 s) … HIDE")
            with g_state.lock:
                g_state.current_command = "SHOW"
            time.sleep(5)
            with g_state.lock:
                g_state.current_command = "HIDE"

        elif raw == "":
            pass

        else:
            print(f"  unknown command: '{raw}'")


def main():
    parser = argparse.ArgumentParser(description="HTTP(S)-based image-display publisher")
    parser.add_argument("--host", default="0.0.0.0", help="bind address (default 0.0.0.0)")
    parser.add_argument("--port", type=int, default=8000, help="HTTP port (default 8000)")
    parser.add_argument("--https", action="store_true", help="enable HTTPS (requires cert.pem and key.pem)")
    args = parser.parse_args()

    # Create HTTP server
    server = HTTPServer((args.host, args.port), PublisherHandler)
    
    # Wrap with SSL if requested
    if args.https:
        if not (os.path.exists("cert.pem") and os.path.exists("key.pem")):
            print("[server] ERROR: cert.pem and key.pem not found for HTTPS mode")
            return
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.load_cert_chain("cert.pem", "key.pem")
        server.socket = context.wrap_socket(server.socket, server_side=True)
        protocol = "https"
    else:
        protocol = "http"

    print(f"[server] listening on {protocol}://{args.host}:{args.port}")
    print(f"[server] clients poll GET /command, send events to POST /event")

    # Run server in background thread
    server_thread = threading.Thread(target=server.serve_forever, daemon=True)
    server_thread.start()

    # Run interactive loop in main thread
    interactive_loop(args.host, args.port)

    # Shutdown
    server.shutdown()
    print("[server] bye")


if __name__ == "__main__":
    main()