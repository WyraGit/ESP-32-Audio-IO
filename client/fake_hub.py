#!/usr/bin/env python3
"""Pretends to be a Wyra Audio Hub in TCP server mode, for testing clients without hardware.

It behaves like the firmware does:
  * streams a "line input" to every connected client in 512-byte blocks
    (128 stereo frames at 44.1 kHz): a 1 kHz tone on the left and 500 Hz on the
    right at -6 dBFS, or silence with --signal silence;
  * once per block, takes 512 bytes from the first client that has a full block
    waiting and "plays" it, which you can save with --save;
  * accepts up to 4 clients; a fifth connects but gets nothing, like the hub.

  python fake_hub.py                        # listen on 127.0.0.1:7000
  python fake_hub.py --save line_out.wav    # also save what clients send
  python fake_hub.py --seconds 30           # stop by itself after 30 s
"""

from __future__ import annotations

import argparse
import math
import select
import socket
import sys
import time
import wave
from array import array

SAMPLE_RATE = 44100
BLOCK_FRAMES = 128
BLOCK_BYTES = BLOCK_FRAMES * 4
MAX_CLIENTS = 4
RECEIVE_WINDOW = 8192  # like the hub's small TCP window: stop reading when this much is queued


class Client:
    def __init__(self, sock: socket.socket, address) -> None:
        self.sock = sock
        self.address = f"{address[0]}:{address[1]}"
        self.inbox = bytearray()
        self.sent = 0
        self.played = 0
        sock.settimeout(2.0)  # a client that stops reading blocks us, as it would block the hub


def tone_block(phases: list, steps: list, amplitude: float) -> bytes:
    samples = array("h", bytes(BLOCK_BYTES))
    for i in range(BLOCK_FRAMES):
        for ch in (0, 1):
            samples[2 * i + ch] = int(amplitude * math.sin(phases[ch]))
            phases[ch] += steps[ch]
    for ch in (0, 1):
        phases[ch] %= 2 * math.pi
    if sys.byteorder == "big":
        samples.byteswap()
    return samples.tobytes()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=7000)
    parser.add_argument("--signal", choices=("tone", "silence"), default="tone")
    parser.add_argument("--save", help="WAV file for the audio clients send to the line output")
    parser.add_argument("--seconds", type=float, help="stop after this many seconds")
    args = parser.parse_args()

    server = socket.create_server((args.host, args.port))
    server.setblocking(False)
    out = None
    if args.save:
        out = wave.open(args.save, "wb")
        out.setnchannels(2)
        out.setsampwidth(2)
        out.setframerate(SAMPLE_RATE)

    clients: list = []
    phases = [0.0, 0.0]
    steps = [2 * math.pi * 1000 / SAMPLE_RATE, 2 * math.pi * 500 / SAMPLE_RATE]
    amplitude = 32767 * 10 ** (-6 / 20) if args.signal == "tone" else 0.0
    played_blocks = underruns = 0
    anyone_sending = False
    start = time.perf_counter()
    blocks_done = 0
    last_report = start
    print(f"Fake Audio Hub on {args.host}:{args.port} (Ctrl+C to stop)", flush=True)

    try:
        while args.seconds is None or time.perf_counter() - start < args.seconds:
            # New clients, up to the hub's limit
            while len(clients) < MAX_CLIENTS:
                try:
                    sock, address = server.accept()
                except BlockingIOError:
                    break
                clients.append(Client(sock, address))
                print(f"+ client {clients[-1].address} ({len(clients)}/{MAX_CLIENTS})", flush=True)

            due = int((time.perf_counter() - start) * SAMPLE_RATE / BLOCK_FRAMES) - blocks_done
            for _ in range(max(0, due)):
                block = tone_block(phases, steps, amplitude)
                # 1. Line input -> every client
                for client in list(clients):
                    try:
                        client.sock.sendall(block)
                        client.sent += len(block)
                    except OSError as exc:
                        print(f"- client {client.address} dropped ({exc})", flush=True)
                        client.sock.close()
                        clients.remove(client)
                # 2. Read what clients sent, without growing past the hub's window
                readable = [c.sock for c in clients if len(c.inbox) < RECEIVE_WINDOW]
                if readable:
                    ready, _, _ = select.select(readable, [], [], 0)
                    for client in [c for c in clients if c.sock in ready]:
                        try:
                            data = client.sock.recv(RECEIVE_WINDOW - len(client.inbox))
                        except OSError:
                            data = b""
                        if not data:
                            print(f"- client {client.address} disconnected", flush=True)
                            client.sock.close()
                            clients.remove(client)
                            continue
                        client.inbox += data
                        anyone_sending = True
                # 3. Play one block from the first client that has one, like the firmware
                for client in clients:
                    if len(client.inbox) >= BLOCK_BYTES:
                        if out is not None:
                            out.writeframesraw(bytes(client.inbox[:BLOCK_BYTES]))
                        del client.inbox[:BLOCK_BYTES]
                        client.played += BLOCK_BYTES
                        played_blocks += 1
                        break
                else:
                    if anyone_sending and clients:
                        underruns += 1
                blocks_done += 1

            now = time.perf_counter()
            if now - last_report >= 2.0:
                print(f"  {len(clients)} client(s) | line out played {played_blocks * BLOCK_FRAMES / SAMPLE_RATE:5.1f} s"
                      f" | underruns {underruns}", flush=True)
                last_report = now
            time.sleep(0.002)
    except KeyboardInterrupt:
        pass
    finally:
        for client in clients:
            client.sock.close()
        server.close()
        if out is not None:
            out.close()
        print(f"Stopped. Line out played {played_blocks * BLOCK_FRAMES / SAMPLE_RATE:.1f} s, underruns {underruns}.",
              flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
