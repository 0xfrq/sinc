import socket
import threading
import argparse
import time

clients: set[socket.socket] = set()
clients_lock = threading.Lock()


def broadcast(message: str, exclude: socket.socket = None) -> None:
    data = (message.strip() + "\n").encode()
    dead: list[socket.socket] = []

    with clients_lock:
        for sock in clients:
            if sock is exclude:
                continue
            try:
                sock.sendall(data)
            except OSError:
                dead.append(sock)

        for sock in dead:
            clients.discard(sock)
            sock.close()

    if dead:
        print(f"  [info] removed {len(dead)} disconnected subscriber(s)")


def handle_client(conn: socket.socket, addr: tuple) -> None:
    print(f"[+] subscriber connected: {addr}")
    with clients_lock:
        clients.add(conn)
    try:
        buf = b""
        while True:
            chunk = conn.recv(1024)
            if not chunk:
                break
            buf += chunk
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                msg = line.strip().decode(errors="ignore")
                if msg == "GAMEOVER":
                    print(f"  [event] GAMEOVER from {addr}, broadcasting to others")
                    broadcast("SHOW2", exclude=conn)
    except OSError:
        pass
    finally:
        with clients_lock:
            clients.discard(conn)
        conn.close()
        print(f"[-] subscriber disconnected: {addr}")


def accept_loop(server: socket.socket) -> None:
    while True:
        try:
            conn, addr = server.accept()
        except OSError:
            break
        t = threading.Thread(target=handle_client, args=(conn, addr), daemon=True)
        t.start()


def interactive_loop() -> None:
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

        if raw in ("show",):
            broadcast("SHOW")
            print(f"  → SHOW sent to {len(clients)} subscriber(s)")
        elif raw in ("show2",):
            broadcast("SHOW2")
            print(f"  → SHOW2 sent to {len(clients)} subscriber(s)")
            def _auto_hide():
                time.sleep(5)
                broadcast("HIDE")
                print(f"  → HIDE (auto) sent to {len(clients)} subscriber(s)")
            threading.Thread(target=_auto_hide, daemon=True).start()
        elif raw in ("hide",):
            broadcast("HIDE")
            print(f"  → HIDE sent to {len(clients)} subscriber(s)")
        elif raw in ("exit",):
            broadcast("EXIT")
            print(f"  → EXIT sent to {len(clients)} subscriber(s)")
            time.sleep(0.5)
            break
        elif raw in ("quit", "q"):
            break
        elif raw == "schedule":
            print("  → SHOW … (waiting 5 s) … HIDE")
            broadcast("SHOW")
            time.sleep(5)
            broadcast("HIDE")
        elif raw == "":
            pass
        else:
            print(f"  unknown command: '{raw}'")


def main() -> None:
    parser = argparse.ArgumentParser(description="Image-display publisher")
    parser.add_argument("--host", default="0.0.0.0", help="bind address (default 0.0.0.0)")
    parser.add_argument("--port", type=int, default=9000, help="TCP port (default 9000)")
    args = parser.parse_args()

    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind((args.host, args.port))
    server.listen(16)
    print(f"[server] listening on {args.host}:{args.port}")

    t = threading.Thread(target=accept_loop, args=(server,), daemon=True)
    t.start()

    interactive_loop()

    server.close()
    print("[server] bye")


if __name__ == "__main__":
    main()