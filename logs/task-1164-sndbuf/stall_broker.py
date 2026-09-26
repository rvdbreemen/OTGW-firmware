"""A broker that completes the MQTT handshake and then stops reading.

This forces exactly the condition TASK-1154 is about. Once the server stops
draining, the peer's TCP receive window closes, the gateway's lwIP send buffer
fills, and WiFiClient::write() starts returning short counts after its
five-second no-progress timeout.

Before the fix that produced a half-written PUBLISH and a disconnect. After it,
beginMqttPublish() should refuse to start the frame and bump mqtt_sndbuf_skips
instead, leaving mqtt_desync_drops alone.

Usage: python stall_broker.py [stall_after_seconds]
"""
import socket
import sys
import threading
import time

HOST, PORT = "0.0.0.0", 1884
STALL_AFTER = float(sys.argv[1]) if len(sys.argv) > 1 else 20.0

state = {"connected_at": None, "bytes_read": 0, "stalled_at": None}


def handle(conn, addr):
    print("[broker] connection from %s:%d" % addr, flush=True)
    conn.settimeout(1.0)
    state["connected_at"] = time.time()
    start = time.time()

    # Minimal handshake: read the CONNECT packet, answer CONNACK accepted.
    try:
        data = conn.recv(4096)
        state["bytes_read"] += len(data)
        if data and data[0] == 0x10:
            conn.sendall(bytes([0x20, 0x02, 0x00, 0x00]))
            print("[broker] CONNACK sent (%d bytes of CONNECT read)" % len(data), flush=True)
        else:
            print("[broker] first packet was not CONNECT: %r" % (data[:8],), flush=True)
    except Exception as e:
        print("[broker] handshake failed: %r" % e, flush=True)
        return

    # Drain normally for a while so discovery settles and the link looks healthy.
    while time.time() - start < STALL_AFTER:
        try:
            d = conn.recv(65535)
            if not d:
                print("[broker] peer closed during drain phase", flush=True)
                return
            state["bytes_read"] += len(d)
        except socket.timeout:
            pass

    state["stalled_at"] = time.time()
    print("[broker] STALLING NOW after %d bytes. No more reads; the window will "
          "close and the gateway's send buffer will fill." % state["bytes_read"], flush=True)

    # Hold the socket open and never read again.
    while True:
        time.sleep(5)
        print("[broker] still stalled, %.0fs elapsed, total read %d B"
              % (time.time() - state["stalled_at"], state["bytes_read"]), flush=True)


def main():
    s = socket.socket()
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind((HOST, PORT))
    s.listen(4)
    print("[broker] listening on %s:%d, will stall after %.0fs" % (HOST, PORT, STALL_AFTER), flush=True)
    while True:
        conn, addr = s.accept()
        threading.Thread(target=handle, args=(conn, addr), daemon=True).start()


if __name__ == "__main__":
    main()
