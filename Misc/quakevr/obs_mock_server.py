"""A mock of OBS Studio's obs-websocket v5 server, for the game's OBS row (Quake/vr/vr_obs.cpp).

    python Misc/quakevr/obs_mock_server.py [--port 4455] [--password secret] [--recording-ms 754000]

Standard library only. Implements what the game uses: the WebSocket upgrade (subprotocol obswebsocket.json), Hello
(op 0, with an authentication challenge when a password is set), Identify (op 1: a wrong or missing authentication
closes with 4009, as OBS), Identified (op 2), the requests GetRecordStatus and ToggleRecord (op 6 -> 7), and the
RecordStateChanged events (op 5: STARTING then STARTED, STOPPING then STOPPED) a toggle sends. Pings are answered.
Each step is printed ("MOCK ..."), and counted in MockObs.counts (obs_test.py imports it).
"""

import argparse
import base64
import hashlib
import json
import os
import socket
import struct
import threading
import time

GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"


class MockObs:
    def __init__(self, port, password="", recording_ms=None, quiet=False):
        self.port = port
        self.password = password
        self.active = recording_ms is not None
        self.paused = False
        self.started = time.time() - (recording_ms or 0) / 1000.0
        self.counts = {}
        self.quiet = quiet
        self.lock = threading.Lock()
        self.server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.server.bind(("127.0.0.1", port))
        self.server.listen(4)
        self.closed = False
        self.clients = []

    def count(self, what):
        with self.lock:
            self.counts[what] = self.counts.get(what, 0) + 1
        if not self.quiet:
            print(f"MOCK {time.strftime('%H:%M:%S')}.{int(time.time() * 1000) % 1000:03d} {what}", flush=True)

    def start(self):
        threading.Thread(target=self.serve, daemon=True).start()
        return self

    def stop(self):
        """OBS quit: the listener and every connection closed."""
        self.closed = True
        try:
            self.server.close()
        except OSError:
            pass
        for c in list(self.clients):
            try:
                c.shutdown(socket.SHUT_RDWR)
                c.close()
            except OSError:
                pass

    def serve(self):
        while not self.closed:
            try:
                c, _ = self.server.accept()
            except OSError:
                return
            self.clients.append(c)
            threading.Thread(target=self.client, args=(c,), daemon=True).start()

    # ---- frames
    @staticmethod
    def recv_exact(c, n):
        b = b""
        while len(b) < n:
            part = c.recv(n - len(b))
            if not part:
                raise ConnectionError("closed")
            b += part
        return b

    def recv_frame(self, c):
        h = self.recv_exact(c, 2)
        op = h[0] & 0x0F
        n = h[1] & 0x7F
        if n == 126:
            n = struct.unpack(">H", self.recv_exact(c, 2))[0]
        elif n == 127:
            n = struct.unpack(">Q", self.recv_exact(c, 8))[0]
        if not h[1] & 0x80:
            raise ConnectionError("client frame not masked")
        key = self.recv_exact(c, 4)
        data = bytes(b ^ key[i & 3] for i, b in enumerate(self.recv_exact(c, n)))
        return op, data

    @staticmethod
    def send_frame(c, op, data):
        n = len(data)
        head = bytes([0x80 | op])
        head += bytes([n]) if n < 126 else bytes([126]) + struct.pack(">H", n) if n < 65536 else bytes([127]) + struct.pack(">Q", n)
        c.sendall(head + data)

    def send_json(self, c, op, d):
        self.send_frame(c, 1, json.dumps({"op": op, "d": d}).encode())

    # ---- the session
    def status(self):
        ms = int((time.time() - self.started) * 1000) if self.active else 0
        return {"outputActive": self.active, "outputPaused": self.paused, "outputTimecode": "00:00:00.000",
                "outputDuration": ms, "outputCongestion": 0, "outputBytes": 0, "outputSkippedFrames": 0, "outputTotalFrames": 0}

    def client(self, c):
        try:
            head = b""
            while b"\r\n\r\n" not in head:
                part = c.recv(4096)
                if not part:
                    return
                head += part
            key = ""
            for line in head.decode("latin-1").split("\r\n"):
                if line.lower().startswith("sec-websocket-key:"):
                    key = line.split(":", 1)[1].strip()
            accept = base64.b64encode(hashlib.sha1((key + GUID).encode()).digest()).decode()
            c.sendall(("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                       f"Sec-WebSocket-Accept: {accept}\r\nSec-WebSocket-Protocol: obswebsocket.json\r\n\r\n").encode())
            self.count("upgraded")
            hello = {"obsWebSocketVersion": "5.5.2", "rpcVersion": 1}
            expected = None
            if self.password:
                salt = base64.b64encode(os.urandom(32)).decode()
                challenge = base64.b64encode(os.urandom(32)).decode()
                hello["authentication"] = {"challenge": challenge, "salt": salt}
                secret = base64.b64encode(hashlib.sha256((self.password + salt).encode()).digest()).decode()
                expected = base64.b64encode(hashlib.sha256((secret + challenge).encode()).digest()).decode()
            self.send_json(c, 0, hello)
            identified = False
            while True:
                op, data = self.recv_frame(c)
                if op == 8:
                    self.count("client closed")
                    return
                if op == 9:
                    self.send_frame(c, 10, data)
                    continue
                if op != 1:
                    continue
                msg = json.loads(data)
                d = msg.get("d", {})
                if msg.get("op") == 1:
                    if expected is not None and d.get("authentication") != expected:
                        self.count("auth failed")
                        self.send_frame(c, 8, struct.pack(">H", 4009) + b"Authentication failed.")
                        return
                    identified = True
                    self.count(f"identified (eventSubscriptions {d.get('eventSubscriptions')})")
                    self.send_json(c, 2, {"negotiatedRpcVersion": 1})
                elif msg.get("op") == 6 and identified:
                    rt = d.get("requestType")
                    self.count(f"request {rt}")
                    rid = d.get("requestId")
                    if rt == "GetRecordStatus":
                        self.send_json(c, 7, {"requestType": rt, "requestId": rid,
                                              "requestStatus": {"result": True, "code": 100}, "responseData": self.status()})
                    elif rt == "ToggleRecord":
                        going = not self.active
                        state = "OBS_WEBSOCKET_OUTPUT_STARTING" if going else "OBS_WEBSOCKET_OUTPUT_STOPPING"
                        self.send_json(c, 5, {"eventType": "RecordStateChanged", "eventIntent": 64,
                                              "eventData": {"outputActive": False if going else True, "outputState": state, "outputPath": None}})
                        self.active = going
                        self.paused = False
                        self.started = time.time()
                        self.send_json(c, 7, {"requestType": rt, "requestId": rid,
                                              "requestStatus": {"result": True, "code": 100}, "responseData": {"outputActive": going}})
                        state = "OBS_WEBSOCKET_OUTPUT_STARTED" if going else "OBS_WEBSOCKET_OUTPUT_STOPPED"
                        self.send_json(c, 5, {"eventType": "RecordStateChanged", "eventIntent": 64,
                                              "eventData": {"outputActive": going, "outputState": state,
                                                            "outputPath": None if going else "C:/mock/recording.mkv"}})
                        self.count("recording" if going else "stopped")
                    else:
                        self.send_json(c, 7, {"requestType": rt, "requestId": rid,
                                              "requestStatus": {"result": False, "code": 204, "comment": "unknown request"}})
        except (ConnectionError, OSError, ValueError):
            pass
        finally:
            c.close()


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--port", type=int, default=4455)
    p.add_argument("--password", default="")
    p.add_argument("--recording-ms", type=int, default=None, help="start as recording for this long already")
    p.add_argument("--lifetime", type=float, default=0, help="seconds to serve (0: until killed)")
    a = p.parse_args()
    MockObs(a.port, a.password, a.recording_ms).start()
    print(f"MOCK listening on 127.0.0.1:{a.port}", flush=True)
    t0 = time.time()
    while not a.lifetime or time.time() - t0 < a.lifetime:
        time.sleep(0.2)


if __name__ == "__main__":
    main()
