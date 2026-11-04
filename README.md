# Wyrá Audio Hub — ESP32 Audio I/O

<a href="https://buymeacoffee.com/hapyjatech" target="_blank"><img src="https://cdn.buymeacoffee.com/buttons/v2/default-yellow.png" alt="Buy Me A Coffee" style="height: 60px !important;width: 217px !important;" ></a>

Turn a classic **ESP32** into a two-way digital audio hub for mixers, PCs and phones.
Stereo line in and line out at **44.1 kHz / 16-bit**, with three modes you pick on a
physical switch:

| Switch position | Mode | What it does |
| :--- | :--- | :--- |
| **A** | **Wi-Fi audio server (TCP)** | Streams the line input to apps on your network and plays audio they send back. |
| **Center** | **Bluetooth speaker (A2DP)** | Pair a phone or laptop and its audio comes out of the line output. Wi-Fi is switched off. |
| **B** | **Aux pass-through** | Sends the line input straight to the line output (ADC → DAC), no network needed. |

A 0.96" OLED shows the current mode, IP address and input/output level meters, and a web
dashboard shows the same information from any browser on the network.

---

## Features

- Full-duplex stereo audio over two I2S buses: **PCM1808** ADC in, **PCM5102A** DAC out.
- TCP audio server on port **7000** for up to **4 clients** at the same time.
- Bluetooth A2DP sink, visible as **`Audio_Server_ESP32`**.
- Mode changes on the fly: move the switch and the device restarts the audio pipeline in the new mode.
- Bluetooth and Aux work without any network setup; only the TCP server needs Wi-Fi.
- Wi-Fi and time-zone setup from your phone's browser, stored in flash (NVS). If the router is
  slow to come back after a power cut, the device keeps the saved network and retries on its own.
- OLED status screen and a web dashboard with level meters, connected-client count and network time.

## Status

| Function | Status |
| :--- | :--- |
| Line out over TCP (PC → device) | ✅ Tested on hardware |
| Bluetooth A2DP playback | ✅ Tested on hardware |
| Line in over TCP (device → PC) | 🧪 Implemented, hardware test in progress |
| Aux pass-through | 🧪 Implemented, hardware test in progress |

---

## Hardware

| Part | Notes |
| :--- | :--- |
| **ESP32 dev board with built-in 0.96" OLED** | Classic ESP32 (WROOM/WROVER). Bluetooth A2DP needs the original ESP32 — S2, S3, C3 and C6 chips won't work. OLED: SSD1306, I2C address `0x3C`. |
| **PCM1808 ADC module** | Line input from the mixer. 24-bit converter, used here at 16 bits. |
| **PCM5102A DAC module** | Line output to the mixer, headphones or amplifier. |
| **3-position slide switch (SP3T)** | Mode selector. |
| **2 × 10 kΩ resistors** | Pull-ups for the switch — required, see the wiring notes. |
| **5 V / 2 A USB power supply** | A weak supply can cause resets and audible noise. |
| Jacks and cable | 1/4" (6.35 mm) or 3.5 mm panel jacks; shielded audio cable to keep Wi-Fi noise out of the analog path. |
| *Recommended* | 220 µF electrolytic capacitors between 3.3 V and GND near the audio modules. |

### Wiring

All modules share a common **GND**.

**OLED (built into the recommended board)**

| OLED | ESP32 |
| :--- | :--- |
| SDA | GPIO 21 |
| SCL | GPIO 22 |

If your board wires its OLED to other pins, change `OLED_SDA` and `OLED_SCL` in the sketch.

**PCM1808 — line in (I2S1, receive only)**

| PCM1808 | ESP32 | Signal |
| :--- | :--- | :--- |
| BCK | GPIO 14 | Bit clock |
| LRCK (LRC) | GPIO 12 | Word select |
| DOUT (OUT) | GPIO 13 | Audio data into the ESP32 |
| SCK (SCKI) | **GPIO 3 (RX0)** | Master clock — the PCM1808 needs it to produce samples |
| FMT, MD0, MD1 | GND | I2S format, slave mode |

Power the module according to its silkscreen: many PCM1808 boards have separate 5 V
(analog) and 3.3 V (digital) pins.

**PCM5102A — line out (I2S0, transmit only)**

| PCM5102A | ESP32 | Signal |
| :--- | :--- | :--- |
| BCK | GPIO 26 | Bit clock |
| LCK (LRCK) | GPIO 25 | Word select |
| DIN | GPIO 18 | Audio data out of the ESP32 |
| SCK | GND | Lets the DAC generate its own clock |
| VIN | 3.3 V | |

**Mode switch**

| Switch | ESP32 |
| :--- | :--- |
| Common (middle pin) | GND |
| Side A | GPIO 34, plus a 10 kΩ pull-up to 3.3 V |
| Side B | GPIO 35, plus a 10 kΩ pull-up to 3.3 V |

> **Wiring notes**
> - GPIO 34 and 35 are input-only and have **no internal pull-up**. Without the external
>   resistors the pins float and the device jumps between modes on its own.
> - GPIO 12 is a boot strapping pin. Make sure nothing pulls it high while the ESP32 boots.
> - GPIO 3 is also the USB-serial RX line. While the ADC clock is running, the Serial
>   Monitor can't send data to the board.

---

## Building the firmware

1. Install the **Arduino IDE** and the **esp32 by Espressif** board package. The sketch uses the
   legacy `driver/i2s.h` API and is verified to compile with esp32 **3.3.10** and ESP32-A2DP **1.8.11**.
2. Install these libraries from the Library Manager (`Ctrl+Shift+I`):
   - `Adafruit GFX Library`
   - `Adafruit SSD1306`
   - `ESP32-A2DP` by Phil Schatzmann
3. Open `ESP32_audio_project/ESP32_audio_project.ino`.
4. Select **Tools → Board → ESP32 Dev Module**.
5. Select **Tools → Partition Scheme → Huge APP (3MB No OTA/1MB SPIFFS)**. This is required:
   Wi-Fi, the web server and Bluetooth together don't fit in the default partition.
6. Upload.

## First-time setup

Only the TCP server mode needs Wi-Fi. Bluetooth and Aux work right away.

1. Set the switch to **A** (TCP server) and power the device. With no saved network, the OLED
   shows the setup screen.
2. On your phone, join the open Wi-Fi network **`AudioServer_Setup`**.
3. Open **http://192.168.4.1** in the browser.
4. Pick your network, enter the password, choose your time zone and save. The device restarts
   and joins your network.
5. The OLED shows the selected mode and the device's IP address.

If the saved network doesn't answer within 10 seconds (for example, the router is still
booting after a power cut), the OLED shows **CAN'T JOIN WI-FI**. The device keeps the saved
settings, opens `AudioServer_Setup` in case you want to pick another network, and retries the
saved one every 30 seconds. As soon as it connects, it restarts in normal mode.

To change networks later, open the dashboard and use **Reset Wi-Fi**, which erases the saved
network and restarts the device.

## Dashboard

Browse to `http://<device-ip>/` from any device on the same network. The page refreshes every
3 seconds and shows:

- current mode and IP address;
- connected TCP clients (out of 4);
- network time in your time zone;
- line-in and line-out levels.

The dashboard works in TCP server mode, and in Aux mode when a network is saved. It isn't
available in Bluetooth mode, because Wi-Fi is off.

---

## Using each mode

### A — Wi-Fi audio server (TCP)

| | |
| :--- | :--- |
| Protocol | Raw TCP socket |
| Port | `7000` |
| Clients | Up to 4 at once |
| Audio format | PCM, 44,100 Hz, 16-bit signed little-endian, 2 channels interleaved |

- **Device → client:** as soon as a client connects, the device sends the line input continuously,
  in 512-byte blocks (128 stereo frames, about 2.9 ms). Every connected client gets the same stream.
- **Client → device:** write raw PCM in the same format to the socket and it plays on the line
  output. The device plays one 512-byte block at a time from the first client that has a full
  block waiting, so send audio from one client at a time.
- **Keep reading.** A client that only sends audio must still read and discard the incoming
  stream; otherwise the device's TCP buffer fills up and audio stalls.
- Add a jitter buffer of at least 50 ms in your client to ride out Wi-Fi hiccups.

**Example clients (Python):** the repository includes two Jupyter notebooks. Set `ESP32_IP` to
the address shown on the OLED before running them.

| Notebook | What it does | Needs |
| :--- | :--- | :--- |
| `test_tcp_LINE_IN.ipynb` | Plays the device's line input on the PC speakers | `pip install pyaudio` |
| `test_tcp_LINE_OUT.ipynb` | Captures what the PC is playing (loopback) and sends it to the line output | `pip install soundcard numpy` |

### Center — Bluetooth speaker (A2DP)

Pair your phone or computer with **`Audio_Server_ESP32`**. Audio plays on the line output, and the
OLED shows the output level and whether music is playing. Wi-Fi is off in this mode.

### B — Aux pass-through

The line input goes straight to the line output through the ESP32. Use it to monitor the
input locally without a network. If a Wi-Fi network is saved, the device joins it in the
background so the dashboard stays available; the audio doesn't depend on it.

---

## Specifications

| | |
| :--- | :--- |
| Sample rate / resolution | 44.1 kHz / 16-bit, stereo |
| Audio interface | 2 × I2S (full duplex), ESP32 as clock master |
| ADC / DAC | PCM1808 / PCM5102A |
| Network | Wi-Fi 2.4 GHz, TCP port 7000, web dashboard on port 80 |
| Bluetooth | Classic Bluetooth A2DP sink |
| Display | 128 × 64 SSD1306 OLED |
| Clock | NTP (`pool.ntp.org`), time zone chosen at setup (UTC offset, no daylight saving) |

## Troubleshooting

| Symptom | Fix |
| :--- | :--- |
| `text section exceeds available space` when compiling | Set **Partition Scheme** to **Huge APP**. |
| Mode changes by itself | Add the 10 kΩ pull-ups on GPIO 34 and 35. |
| No sound from the DAC | Check the PCM5102A solder jumpers: on most boards **XSMT** must be high (unmuted). |
| No signal from the line input | Check that the PCM1808 SCK pin is wired to GPIO 3 and that FMT, MD0 and MD1 are grounded. |
| OLED stays blank | Check GPIO 21/22 and the I2C address (`0x3C`). |
| No Bluetooth audio | Install `ESP32-A2DP` and pair with `Audio_Server_ESP32`. |
| OLED shows **CAN'T JOIN WI-FI** | The saved network didn't answer. Wait: the device retries every 30 s and connects on its own when the router is back. If the password changed, join `AudioServer_Setup` and enter it again. |

---

## Project files

| Path | Contents |
| :--- | :--- |
| `ESP32_audio_project/ESP32_audio_project.ino` | Main sketch: mode state machine, TCP server, Bluetooth, OLED |
| `ESP32_audio_project/AudioI2SForESP32.h` | I2S setup for the ADC and DAC, including the master clock on GPIO 3 |
| `ESP32_audio_project/html_pages.h` | Setup and dashboard web pages, served by the device |
| `ESP32_audio_project/logo_png.h` | Logo for the web pages, stored in flash so it shows even without internet |
| `ESP32_audio_project/setup.html`, `dashboard.html` | Static previews of the web pages with sample data |
| `ESP32_audio_project/logo_web.png` | Source image for `logo_png.h` |
| `test_tcp_LINE_IN.ipynb`, `test_tcp_LINE_OUT.ipynb` | Python test clients |

## License

© 2026 Sergio Levy · Wyrá. Proprietary license: you may build and modify this project for
personal, educational and non-commercial use. Selling it, or using it in a commercial product,
needs written permission. See [LICENSE](LICENSE).

Bluetooth support uses [ESP32-A2DP](https://github.com/pschatzmann/ESP32-A2DP) by Phil
Schatzmann; the display uses the Adafruit GFX and SSD1306 libraries.
