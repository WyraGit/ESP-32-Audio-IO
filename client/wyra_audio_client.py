#!/usr/bin/env python3
"""Wyra Audio Hub client for the TCP server mode (switch position A).

The Audio Hub streams its line input to every connected client and plays the
audio a client sends back on its line output. The protocol is raw PCM over TCP,
with no header and no handshake:

    port 7000 · 44,100 Hz · 16-bit signed little-endian · 2 channels interleaved

Commands
  listen     play the hub's line input on this computer
  record     save the hub's line input to a WAV file
  send-pc    send what this computer is playing to the hub's line output
  send-file  send a 44.1 kHz / 16-bit WAV file to the hub's line output
  tone       send a test tone to the hub's line output

record, send-file and tone only use the Python standard library. listen and
send-pc need two packages:  pip install soundcard numpy
(send-pc works on Windows and Linux.)

Press Ctrl+C to stop. If the connection drops, for example because you moved
the mode switch, the client keeps retrying every 2 seconds.
"""

from __future__ import annotations

import argparse
import math
import select
import socket
import sys
import threading
import time
import wave
import warnings
from array import array

SAMPLE_RATE = 44100
CHANNELS = 2
SAMPLE_WIDTH = 2                        # bytes per sample (16-bit)
FRAME_BYTES = CHANNELS * SAMPLE_WIDTH   # 4 bytes per stereo frame
DEFAULT_PORT = 7000
CHUNK_FRAMES = 512                      # about 11.6 ms per chunk
RETRY_SECONDS = 2.0
STALE_SECONDS = 5.0                     # the hub streams nonstop; silence this long means it's gone
SEND_LEAD_SECONDS = 0.25                # how far ahead of real time tone/file sending may run
SEND_TIMEOUT_SECONDS = 5.0

EXAMPLES = """examples:
  python wyra_audio_client.py 192.168.1.150 listen
  python wyra_audio_client.py 192.168.1.150 record show.wav --seconds 60
  python wyra_audio_client.py 192.168.1.150 send-pc
  python wyra_audio_client.py 192.168.1.150 send-file song.wav --loop
  python wyra_audio_client.py 192.168.1.150 tone --freq 440 --level -12

Use the IP address shown on the Audio Hub's OLED. Send audio from one client
at a time: the hub plays whichever connected client has audio waiting first.
"""


class ConnectionLost(Exception):
    """The hub closed the connection or stopped streaming."""


# ---------------------------------------------------------------------------
# PCM helpers
# ---------------------------------------------------------------------------

def to_samples(data: bytes) -> array:
    samples = array("h")
    samples.frombytes(data)
    if sys.byteorder == "big":
        samples.byteswap()
    return samples


def to_bytes(samples: array) -> bytes:
    if sys.byteorder == "big":
        samples = array("h", samples)
        samples.byteswap()
    return samples.tobytes()


def swap_channels(data: bytes) -> bytes:
    samples = to_samples(data)
    samples[0::2], samples[1::2] = samples[1::2], samples[0::2]
    return to_bytes(samples)


def mono_to_stereo(data: bytes) -> bytes:
    mono = to_samples(data)
    stereo = array("h", bytes(len(mono) * 2 * SAMPLE_WIDTH))
    stereo[0::2] = mono
    stereo[1::2] = mono
    return to_bytes(stereo)


class FrameAligner:
    """TCP delivers arbitrary byte counts; this hands out whole stereo frames only."""

    def __init__(self) -> None:
        self._rest = b""

    def push(self, data: bytes) -> bytes:
        data = self._rest + data
        cut = len(data) - len(data) % FRAME_BYTES
        self._rest = data[cut:]
        return data[:cut]


class LevelMeter:
    """Peak level per channel since the last read."""

    def __init__(self) -> None:
        self._lock = threading.Lock()
        self._peaks = [0, 0]

    def feed(self, data: bytes) -> None:
        samples = to_samples(data)
        peaks = []
        for channel in (samples[0::2], samples[1::2]):
            peaks.append(max(max(channel, default=0), -min(channel, default=0)))
        with self._lock:
            self._peaks = [max(a, b) for a, b in zip(self._peaks, peaks)]

    def read(self) -> list:
        with self._lock:
            peaks, self._peaks = self._peaks, [0, 0]
        return [20 * math.log10(p / 32768) if p else float("-inf") for p in peaks]


def meter_text(levels: list) -> str:
    parts = []
    for name, db in zip("LR", levels):
        filled = 0 if db == float("-inf") else int(round((max(-60.0, db) + 60.0) / 60.0 * 10))
        value = " -inf" if db == float("-inf") else f"{db:5.1f}"
        parts.append(f"{name} [{'#' * filled}{'-' * (10 - filled)}] {value} dB")
    return "  ".join(parts)


def clock(seconds: float) -> str:
    seconds = int(seconds)
    return f"{seconds // 60:02d}:{seconds % 60:02d}"


class Status:
    """Rewrites one status line twice a second while a session runs."""

    def __init__(self, line) -> None:
        self._line = line
        self._stop = threading.Event()
        self._thread = threading.Thread(target=self._run, daemon=True)

    def _run(self) -> None:
        while not self._stop.wait(0.5):
            sys.stdout.write("\r" + self._line()[:78].ljust(78))
            sys.stdout.flush()

    def __enter__(self) -> "Status":
        self._thread.start()
        return self

    def __exit__(self, *exc) -> None:
        self._stop.set()
        self._thread.join()
        sys.stdout.write("\n")
        sys.stdout.flush()


# ---------------------------------------------------------------------------
# Network
# ---------------------------------------------------------------------------

def describe(exc: OSError) -> str:
    """A short, actionable reason instead of the operating system's (localized) message."""
    if isinstance(exc, ConnectionRefusedError):
        return "connection refused: is the hub's switch on A, TCP server mode?"
    if isinstance(exc, (socket.timeout, TimeoutError)):
        return "no answer: check the IP address and that this computer is on the same Wi-Fi"
    if isinstance(exc, socket.gaierror):
        return "unknown address: use the IP shown on the hub's OLED"
    return exc.strerror or str(exc) or type(exc).__name__


def open_connection(host: str, port: int) -> socket.socket:
    sock = socket.create_connection((host, port), timeout=5.0)
    sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
    sock.settimeout(SEND_TIMEOUT_SECONDS)
    return sock


class Receiver(threading.Thread):
    """Reads the hub's line-in stream and passes whole frames to on_audio.

    The hub stalls if a client stops reading, so this always drains the
    socket, even when the audio itself is thrown away.
    """

    def __init__(self, sock: socket.socket, on_audio, swap: bool) -> None:
        super().__init__(daemon=True)
        self.sock = sock
        self.on_audio = on_audio
        self.swap = swap
        self.closed = threading.Event()
        self.error = None
        self.last_data = time.monotonic()
        self._halt = threading.Event()
        self._aligner = FrameAligner()

    def run(self) -> None:
        try:
            while not self._halt.is_set():
                readable, _, _ = select.select([self.sock], [], [], 0.5)
                if not readable:
                    continue
                data = self.sock.recv(16384)
                if not data:
                    break
                self.last_data = time.monotonic()
                frames = self._aligner.push(data)
                if frames:
                    self.on_audio(swap_channels(frames) if self.swap else frames)
        except OSError:
            pass
        except Exception as exc:  # e.g. disk full while recording
            self.error = exc
        finally:
            self.closed.set()

    def check(self) -> None:
        """Raises if the connection is gone; call this from the session loop."""
        if self.error is not None:
            raise self.error
        if self.closed.is_set():
            raise ConnectionLost("the hub closed the connection")
        if time.monotonic() - self.last_data > STALE_SECONDS:
            raise ConnectionLost(
                f"no audio from the hub for {STALE_SECONDS:.0f} s "
                "(is it in TCP server mode? are all 4 client slots in use?)")

    def halt(self) -> None:
        self._halt.set()
        self.join(timeout=2.0)


def send_all(sock: socket.socket, data: bytes) -> None:
    try:
        sock.sendall(data)
    except (socket.timeout, OSError) as exc:
        raise ConnectionLost(f"sending failed ({describe(exc)})") from exc


class Pacer:
    """Keeps file and tone sending close to real time, so stopping is prompt."""

    def __init__(self) -> None:
        self.start = None
        self.frames = 0

    def wait(self, frames: int) -> None:
        if self.start is None:
            self.start = time.perf_counter()
        ahead = self.frames / SAMPLE_RATE - (time.perf_counter() - self.start)
        if ahead > SEND_LEAD_SECONDS:
            time.sleep(ahead - SEND_LEAD_SECONDS)
        self.frames += frames


# ---------------------------------------------------------------------------
# Optional audio devices (soundcard + numpy)
# ---------------------------------------------------------------------------

def require_soundcard():
    try:
        import numpy as np
        import soundcard as sc
    except ImportError as exc:
        raise SystemExit(
            "This command needs the 'soundcard' and 'numpy' packages:\n"
            "    pip install soundcard numpy\n"
            f"({exc})")
    warnings.filterwarnings("ignore", message=".*discontinuity.*")
    return np, sc


def pick_speaker(sc, name):
    if not name:
        return sc.default_speaker()
    matches = [s for s in sc.all_speakers() if name.lower() in s.name.lower()]
    if not matches:
        names = "\n  ".join(s.name for s in sc.all_speakers())
        raise SystemExit(f"No output device matches '{name}'. Available:\n  {names}")
    return matches[0]


def pick_loopback(sc, name):
    speaker = pick_speaker(sc, name)
    try:
        return sc.get_microphone(id=str(speaker.name), include_loopback=True)
    except Exception as exc:
        raise SystemExit(
            f"Can't capture what '{speaker.name}' is playing ({exc}).\n"
            "send-pc needs loopback capture, which soundcard supports on Windows and Linux.\n"
            "Use send-file instead, or a loopback driver with --device.")


# ---------------------------------------------------------------------------
# Sessions: each one survives reconnects and keeps its state between them
# ---------------------------------------------------------------------------

class RecordSession:
    def __init__(self, args) -> None:
        self.path = args.file
        self.limit = int(args.seconds * SAMPLE_RATE) if args.seconds else None
        self.swap = args.swap_lr
        self.frames = 0
        self.meter = LevelMeter()
        self.done = threading.Event()
        self._lock = threading.Lock()
        self.wav = wave.open(self.path, "wb")
        self.wav.setnchannels(CHANNELS)
        self.wav.setsampwidth(SAMPLE_WIDTH)
        self.wav.setframerate(SAMPLE_RATE)

    def on_audio(self, data: bytes) -> None:
        with self._lock:
            if self.done.is_set():
                return
            if self.limit is not None:
                data = data[:(self.limit - self.frames) * FRAME_BYTES]
            self.wav.writeframesraw(data)
            self.frames += len(data) // FRAME_BYTES
            if self.limit is not None and self.frames >= self.limit:
                self.done.set()
        self.meter.feed(data)

    def run(self, sock: socket.socket) -> None:
        rx = Receiver(sock, self.on_audio, self.swap)
        rx.start()
        try:
            with Status(lambda: f"REC {clock(self.frames / SAMPLE_RATE)}  {meter_text(self.meter.read())}"):
                while not self.done.wait(0.2):
                    rx.check()
        finally:
            rx.halt()

    def close(self) -> None:
        with self._lock:
            self.done.set()
            self.wav.close()
        print(f"Saved {self.path} ({clock(self.frames / SAMPLE_RATE)}).")


class ListenSession:
    def __init__(self, args) -> None:
        self.np, self.sc = require_soundcard()
        self.speaker = pick_speaker(self.sc, args.device)
        self.swap = args.swap_lr
        self.target = int(args.buffer_ms * SAMPLE_RATE / 1000) * FRAME_BYTES
        self.limit = self.target * 4 + int(0.2 * SAMPLE_RATE) * FRAME_BYTES
        self.buffer = bytearray()
        self.prebuffering = True
        self.underruns = 0
        self.meter = LevelMeter()
        self._lock = threading.Lock()
        print(f"Playing on: {self.speaker.name}")

    def on_audio(self, data: bytes) -> None:
        self.meter.feed(data)
        with self._lock:
            self.buffer += data
            if len(self.buffer) > self.limit:
                # Playback fell behind: drop the oldest audio to keep latency low
                drop = len(self.buffer) - self.target
                del self.buffer[:drop - drop % FRAME_BYTES]

    def next_chunk(self) -> bytes:
        need = CHUNK_FRAMES * FRAME_BYTES
        with self._lock:
            if self.prebuffering and len(self.buffer) >= self.target:
                self.prebuffering = False
            if not self.prebuffering:
                if len(self.buffer) >= need:
                    chunk = bytes(self.buffer[:need])
                    del self.buffer[:need]
                    return chunk
                self.underruns += 1
                self.prebuffering = True
        return bytes(need)  # silence while the buffer fills

    def buffered_ms(self) -> int:
        with self._lock:
            return int(len(self.buffer) / FRAME_BYTES / SAMPLE_RATE * 1000)

    def run(self, sock: socket.socket) -> None:
        np = self.np
        with self._lock:
            self.buffer.clear()
            self.prebuffering = True
        rx = Receiver(sock, self.on_audio, self.swap)
        rx.start()
        try:
            with self.speaker.player(samplerate=SAMPLE_RATE, channels=CHANNELS, blocksize=CHUNK_FRAMES) as player:
                with Status(lambda: f"IN  {meter_text(self.meter.read())}  buf {self.buffered_ms():3d} ms  drop {self.underruns}"):
                    while True:
                        rx.check()
                        chunk = self.next_chunk()
                        pcm = np.frombuffer(chunk, dtype="<i2").reshape(-1, CHANNELS)
                        player.play(pcm.astype(np.float32) / 32768.0)
        finally:
            rx.halt()

    def close(self) -> None:
        pass


class SendSession:
    """Shared loop for tone, send-file and send-pc."""

    label = "OUT"
    paced = True
    prime_frames = 0   # silence sent first, as a cushion in the hub's TCP buffer

    def __init__(self, args) -> None:
        self.swap = args.swap_lr
        self.meter_out = LevelMeter()
        self.sent_frames = 0

    def next_chunk(self):
        raise NotImplementedError

    def run(self, sock: socket.socket) -> None:
        rx = Receiver(sock, lambda data: None, swap=False)  # drain the line-in stream
        rx.start()
        pacer = Pacer()
        try:
            if self.prime_frames:
                send_all(sock, bytes(self.prime_frames * FRAME_BYTES))
            with Status(lambda: f"{self.label} {clock(self.sent_frames / SAMPLE_RATE)}  {meter_text(self.meter_out.read())}"):
                while True:
                    rx.check()
                    chunk = self.next_chunk()
                    if chunk is None:
                        break
                    if self.swap:
                        chunk = swap_channels(chunk)
                    frames = len(chunk) // FRAME_BYTES
                    if self.paced:
                        pacer.wait(frames)
                    send_all(sock, chunk)
                    self.meter_out.feed(chunk)
                    self.sent_frames += frames
                # Let the hub play what is still buffered before hanging up
                time.sleep(SEND_LEAD_SECONDS + 0.2)
        finally:
            rx.halt()

    def close(self) -> None:
        pass


class ToneSession(SendSession):
    label = "TONE"

    def __init__(self, args) -> None:
        super().__init__(args)
        if not 20 <= args.freq <= 20000:
            raise SystemExit("--freq must be between 20 and 20000 Hz.")
        if args.level > 0:
            raise SystemExit("--level is in dBFS and must be 0 or lower (for example -12).")
        self.amplitude = 32767 * 10 ** (args.level / 20)
        self.step = 2 * math.pi * args.freq / SAMPLE_RATE
        self.phase = 0.0
        self.limit = int(args.seconds * SAMPLE_RATE) if args.seconds else None
        self.generated = 0
        print(f"Sending a {args.freq:g} Hz tone at {args.level:g} dBFS.")

    def next_chunk(self):
        frames = CHUNK_FRAMES
        if self.limit is not None:
            frames = min(frames, self.limit - self.generated)
            if frames <= 0:
                return None
        samples = array("h", bytes(frames * FRAME_BYTES))
        phase, step, amplitude = self.phase, self.step, self.amplitude
        for i in range(frames):
            value = int(amplitude * math.sin(phase))
            samples[2 * i] = value
            samples[2 * i + 1] = value
            phase += step
        self.phase = phase % (2 * math.pi)
        self.generated += frames
        return to_bytes(samples)


class FileSession(SendSession):
    label = "FILE"

    def __init__(self, args) -> None:
        super().__init__(args)
        self.loop = args.loop
        try:
            self.wav = wave.open(args.file, "rb")
        except (OSError, wave.Error, EOFError) as exc:
            raise SystemExit(f"Can't read {args.file}: {exc}")
        channels, width, rate = self.wav.getnchannels(), self.wav.getsampwidth(), self.wav.getframerate()
        if width != SAMPLE_WIDTH or rate != SAMPLE_RATE or channels not in (1, 2):
            raise SystemExit(
                f"{args.file} is {rate} Hz, {8 * width}-bit, {channels} channel(s). "
                "The hub needs 44,100 Hz, 16-bit, mono or stereo.\n"
                f'Convert it first, for example:  ffmpeg -i "{args.file}" -ar 44100 -ac 2 -sample_fmt s16 out.wav')
        self.mono = channels == 1
        print(f"Sending {args.file} ({clock(self.wav.getnframes() / SAMPLE_RATE)}"
              f"{', looping' if self.loop else ''}).")

    def next_chunk(self):
        data = self.wav.readframes(CHUNK_FRAMES)
        if not data and self.loop:
            self.wav.rewind()
            data = self.wav.readframes(CHUNK_FRAMES)
        if not data:
            return None
        return mono_to_stereo(data) if self.mono else data

    def close(self) -> None:
        self.wav.close()


class SendPcSession(SendSession):
    label = "PC"
    paced = False  # the audio device already runs in real time

    def __init__(self, args) -> None:
        super().__init__(args)
        # Loopback capture arrives in uneven bursts. A head start of silence, queued in the
        # TCP buffers ahead of the hub, absorbs them so its output doesn't run dry.
        self.prime_frames = int(args.buffer_ms * SAMPLE_RATE / 1000)
        self.np, self.sc = require_soundcard()
        self.source = pick_loopback(self.sc, args.device)
        self.recorder = None
        print(f"Capturing what this computer plays on: {self.source.name}")
        print("Tip: don't play the hub's line input on this computer at the same time, or it will echo.")

    def run(self, sock: socket.socket) -> None:
        with self.source.recorder(samplerate=SAMPLE_RATE, channels=CHANNELS, blocksize=CHUNK_FRAMES) as recorder:
            self.recorder = recorder
            super().run(sock)

    def next_chunk(self):
        np = self.np
        data = self.recorder.record(numframes=CHUNK_FRAMES)
        if data.ndim == 1:
            data = data[:, None]
        if data.shape[1] == 1:
            data = np.repeat(data, 2, axis=1)
        data = data[:, :2]
        return (np.clip(data, -1.0, 1.0) * 32767.0).astype("<i2").tobytes()


SESSIONS = {
    "listen": ListenSession,
    "record": RecordSession,
    "send-pc": SendPcSession,
    "send-file": FileSession,
    "tone": ToneSession,
}


# ---------------------------------------------------------------------------
# Command line
# ---------------------------------------------------------------------------

def parse_args(argv=None):
    parser = argparse.ArgumentParser(
        prog="wyra_audio_client.py",
        description="Stream audio to and from a Wyra Audio Hub in TCP server mode (switch position A).",
        epilog=EXAMPLES,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("host", help="the Audio Hub's IP address, as shown on its OLED")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT, help="TCP port (default: 7000)")
    parser.add_argument("--swap-lr", action="store_true", help="swap the left and right channels")
    commands = parser.add_subparsers(dest="command", required=True, metavar="command")

    p = commands.add_parser("listen", help="play the hub's line input on this computer")
    p.add_argument("--buffer-ms", type=int, default=80,
                   help="jitter buffer, 20-1000 ms (default: 80); raise it if the sound breaks up")
    p.add_argument("--device", help="part of the output device's name (default: system default)")

    p = commands.add_parser("record", help="save the hub's line input to a WAV file")
    p.add_argument("file", help="WAV file to write")
    p.add_argument("--seconds", type=float, help="stop after this many seconds (default: until Ctrl+C)")

    p = commands.add_parser("send-pc", help="send what this computer is playing to the hub's line output")
    p.add_argument("--device", help="part of the output device's name to capture (default: system default)")
    p.add_argument("--buffer-ms", type=int, default=100,
                   help="head start, 0-1000 ms (default: 100); raise it if the hub's output breaks up")

    p = commands.add_parser("send-file", help="send a 44.1 kHz / 16-bit WAV file to the hub's line output")
    p.add_argument("file", help="WAV file to send")
    p.add_argument("--loop", action="store_true", help="repeat the file until Ctrl+C")

    p = commands.add_parser("tone", help="send a test tone to the hub's line output")
    p.add_argument("--freq", type=float, default=440.0, help="frequency in Hz (default: 440)")
    p.add_argument("--level", type=float, default=-12.0, help="level in dBFS (default: -12)")
    p.add_argument("--seconds", type=float, help="stop after this many seconds (default: until Ctrl+C)")

    args = parser.parse_args(argv)
    if args.command == "listen" and not 20 <= args.buffer_ms <= 1000:
        parser.error("--buffer-ms must be between 20 and 1000")
    if args.command == "send-pc" and not 0 <= args.buffer_ms <= 1000:
        parser.error("--buffer-ms must be between 0 and 1000")
    return args


def main(argv=None) -> int:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(line_buffering=True)  # show messages at once, even through a pipe
    args = parse_args(argv)
    session = SESSIONS[args.command](args)
    try:
        while True:
            try:
                sock = open_connection(args.host, args.port)
            except OSError as exc:
                print(f"Can't reach {args.host}:{args.port} ({describe(exc)}). "
                      f"Retrying in {RETRY_SECONDS:.0f} s... (Ctrl+C to quit)")
                time.sleep(RETRY_SECONDS)
                continue
            print(f"Connected to {args.host}:{args.port}. Press Ctrl+C to stop.")
            try:
                session.run(sock)
                return 0
            except ConnectionLost as exc:
                print(f"Connection lost: {exc}. Retrying in {RETRY_SECONDS:.0f} s... (Ctrl+C to quit)")
                time.sleep(RETRY_SECONDS)
            finally:
                sock.close()
    except KeyboardInterrupt:
        print("\nStopped.")
        return 0
    finally:
        session.close()


if __name__ == "__main__":
    sys.exit(main())
