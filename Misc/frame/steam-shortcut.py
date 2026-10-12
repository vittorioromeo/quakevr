#!/usr/bin/env python3
"""Add, update or remove the "Chromium XR" shortcut in the Steam library.

Talks to the Steam client's built-in DevTools port (127.0.0.1:8080, which
SteamOS starts Steam with), so Steam doesn't need restarting. Python stdlib
only.

  steam-shortcut.py ensure NAME EXE START_DIR ICON ID_FILE  -> prints the app id
  steam-shortcut.py remove NAME ID_FILE
"""
import base64, json, os, socket, struct, sys, urllib.request

DEVTOOLS = 'http://127.0.0.1:8080/json'


def target_ws():
    for t in json.load(urllib.request.urlopen(DEVTOOLS, timeout=5)):
        if t.get('title') == 'SharedJSContext':
            return t['webSocketDebuggerUrl']
    sys.exit('SharedJSContext not found: is the Steam client running?')


class WS:
    """Just enough RFC 6455 for one CDP request/response on loopback."""

    def __init__(self, url):
        host_port, path = url[len('ws://'):].split('/', 1)
        host, port = host_port.split(':')
        self.s = socket.create_connection((host, int(port)), timeout=20)
        key = base64.b64encode(os.urandom(16)).decode()
        self.s.sendall((f'GET /{path} HTTP/1.1\r\nHost: {host_port}\r\nUpgrade: websocket\r\n'
                        f'Connection: Upgrade\r\nSec-WebSocket-Key: {key}\r\n'
                        'Sec-WebSocket-Version: 13\r\n\r\n').encode())
        buf = b''
        while b'\r\n\r\n' not in buf:
            chunk = self.s.recv(4096)
            if not chunk:
                raise EOFError('connection closed during the websocket handshake')
            buf += chunk
        if b' 101 ' not in buf.split(b'\r\n', 1)[0]:
            sys.exit('websocket handshake failed')
        self.rest = buf.split(b'\r\n\r\n', 1)[1]

    def _read(self, n):
        while len(self.rest) < n:
            chunk = self.s.recv(65536)
            if not chunk:
                raise EOFError
            self.rest += chunk
        out, self.rest = self.rest[:n], self.rest[n:]
        return out

    def send(self, text):
        data = text.encode()
        mask = os.urandom(4)
        n = len(data)
        head = bytes([0x81]) + (bytes([0x80 | n]) if n < 126 else
                                bytes([0x80 | 126]) + struct.pack('>H', n) if n < 65536 else
                                bytes([0x80 | 127]) + struct.pack('>Q', n))
        self.s.sendall(head + mask + bytes(b ^ mask[i % 4] for i, b in enumerate(data)))

    def recv(self):
        msg = b''
        while True:
            b0, b1 = self._read(2)
            n = b1 & 0x7f
            if n == 126:
                n = struct.unpack('>H', self._read(2))[0]
            elif n == 127:
                n = struct.unpack('>Q', self._read(8))[0]
            msg += self._read(n)
            if b0 & 0x80:
                return msg.decode()


def evaluate(js):
    ws = WS(target_ws())
    ws.send(json.dumps({'id': 1, 'method': 'Runtime.evaluate', 'params': {
        'expression': js, 'awaitPromise': True, 'returnByValue': True}}))
    while True:
        r = json.loads(ws.recv())
        if r.get('id') == 1:
            break
    res = r.get('result', {})
    if 'exceptionDetails' in res:
        sys.exit('JS error: ' + json.dumps(res['exceptionDetails'])[:500])
    return res.get('result', {}).get('value')


def shortcuts():
    return evaluate('''(() => appStore.allApps.filter(a => a.app_type === 1073741824)
                         .map(a => ({appid: a.appid, name: a.display_name})))()''')


def read_id(path):
    try:
        with open(path) as f:
            return int(f.read().strip())
    except (OSError, ValueError):
        return None


def ensure(name, exe, start_dir, icon, id_file):
    saved = read_id(id_file)
    apps = shortcuts()
    # Match the saved app id first, so renaming the shortcut in Steam
    # doesn't make a rerun add a second one.
    found = ([a['appid'] for a in apps if a['appid'] == saved] or
             [a['appid'] for a in apps if a['name'] == name])
    if found:
        appid = found[0]
        evaluate(f'''(() => {{
          SteamClient.Apps.SetShortcutExe({appid}, {json.dumps(exe)});
          SteamClient.Apps.SetShortcutStartDir({appid}, {json.dumps(start_dir)});
          if ({json.dumps(icon)}) SteamClient.Apps.SetShortcutIcon({appid}, {json.dumps(icon)});
        }})()''')
    else:
        appid = evaluate(f'''(async () => {{
          const id = await SteamClient.Apps.AddShortcut({json.dumps(name)}, {json.dumps(exe)}, "", "");
          SteamClient.Apps.SetShortcutName(id, {json.dumps(name)});
          SteamClient.Apps.SetShortcutStartDir(id, {json.dumps(start_dir)});
          if ({json.dumps(icon)}) SteamClient.Apps.SetShortcutIcon(id, {json.dumps(icon)});
          return id;
        }})()''')
        if not isinstance(appid, int) or appid <= 0:
            sys.exit(f'Steam did not return a shortcut app id: {appid!r}')
    os.makedirs(os.path.dirname(id_file), exist_ok=True)
    with open(id_file, 'w') as f:
        f.write(f'{appid}\n')
    print(appid)


def remove(name, id_file):
    saved = read_id(id_file)
    apps = shortcuts()
    # The saved app id, plus any shortcut with the name (a lost id file, or one
    # added by hand).
    found = [a['appid'] for a in apps if a['appid'] == saved or a['name'] == name]
    for appid in found:
        evaluate(f'SteamClient.Apps.RemoveShortcut({appid})')
        print(f'removed Steam shortcut {appid}')
    if not found:
        print('no Steam shortcut to remove')
    if saved is not None:
        os.remove(id_file)


def main():
    if len(sys.argv) == 7 and sys.argv[1] == 'ensure':
        action = lambda: ensure(*sys.argv[2:])
    elif len(sys.argv) == 4 and sys.argv[1] == 'remove':
        action = lambda: remove(*sys.argv[2:])
    else:
        sys.exit(__doc__)
    try:
        action()
    except (OSError, ValueError, KeyError, EOFError) as e:
        sys.exit(f"Couldn't update the Steam shortcut ({type(e).__name__}: {e}). "
                 "Is the Steam client running?")


if __name__ == '__main__':
    main()
