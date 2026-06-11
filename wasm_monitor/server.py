import socket
import json
import struct
import threading
import os
from datetime import datetime
from flask import Flask, request, jsonify

# ── Configuration ──────────────────────────────────────────────
# Renode exposes UART as a TCP socket on port 4321
RENODE_UART_HOST = '127.0.0.1'
RENODE_UART_PORT = 4321
LOG_FILE         = 'heartbeat_log.txt'

app        = Flask(__name__)
log        = []
uart_sock  = None

# ── Connect to Renode UART socket ──────────────────────────────
def connect_uart():
    global uart_sock
    try:
        uart_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        uart_sock.connect((RENODE_UART_HOST, RENODE_UART_PORT))
        print(f"[UART] Connected to Renode on {RENODE_UART_HOST}:{RENODE_UART_PORT}")
    except Exception as e:
        print(f"[UART] Could not connect: {e}")
        uart_sock = None

# ── Read heartbeat messages from Renode UART ───────────────────
def read_heartbeats():
    connect_uart()
    if not uart_sock:
        print("[UART] Running without Renode connection - log only mode")
        return
    buf = ""
    while True:
        try:
            data = uart_sock.recv(256).decode('utf-8', errors='replace')
            buf += data
            while '\n' in buf:
                line, buf = buf.split('\n', 1)
                line = line.strip()
                if not line:
                    continue
                entry = {
                    'raw': line,
                    'received_at': datetime.utcnow().isoformat()
                }
                # Try to parse JSON if the WASM module sends JSON
                try:
                    parsed = json.loads(line)
                    entry.update(parsed)
                except:
                    pass
                log.append(entry)
                print(f"[HB] {line}")
                with open(LOG_FILE, 'a') as f:
                    f.write(json.dumps(entry) + '\n')
        except Exception as e:
            print(f"[UART] Read error: {e}")
            break

threading.Thread(target=read_heartbeats, daemon=True).start()

# ── REST: push a new WASM module to MCU via Renode UART ────────
@app.route('/update', methods=['POST'])
def update_module():
    """
    POST /update with raw .wasm binary as body.
    Writes MAGIC + size + bytes into the MCU RAM update region
    by sending a special command over the UART socket.
    """
    wasm_bytes = request.data
    if not wasm_bytes:
        return jsonify({'error': 'no wasm data in body'}), 400

    size = len(wasm_bytes)
    # Protocol: send marker + 4-byte LE size + wasm bytes
    # main.c watches for 0xDEADBEEF at 0x80000000 to trigger update
    update_payload = (
        b'WASMUPDATE'                  +  # text marker for logs
        struct.pack('<I', size)        +  # 4-byte size
        wasm_bytes
    )

    if uart_sock:
        uart_sock.sendall(update_payload)
        return jsonify({'status': 'update_sent', 'bytes': size})
    else:
        # Save to file so it can be loaded manually into Renode
        with open('pending_update.wasm', 'wb') as f:
            f.write(wasm_bytes)
        return jsonify({'status': 'saved_to_file', 'bytes': size,
                        'note': 'No Renode connection - load pending_update.wasm manually'})

# ── REST: view recent heartbeats ───────────────────────────────
@app.route('/heartbeats')
def get_heartbeats():
    return jsonify(log[-50:])

@app.route('/status')
def status():
    last = log[-1] if log else None
    return jsonify({
        'total_received': len(log),
        'last_heartbeat': last,
        'renode_connected': uart_sock is not None
    })

@app.route('/')
def index():
    return '''<h2>WASM Heartbeat Monitor</h2>
    <p><a href="/heartbeats">View heartbeats (JSON)</a></p>
    <p><a href="/status">Status</a></p>
    <p>POST /update with .wasm binary to replace WASM module</p>'''

if __name__ == '__main__':
    print("Heartbeat server running on http://0.0.0.0:5000")
    app.run(host='0.0.0.0', port=5000)
