# Third-party components

| Component | Where | Licence |
|---|---|---|
| [esp_wireguard](https://github.com/droscy/esp_wireguard) 0.4.8 (WireGuard for lwIP on the ESP32, via esphome-libs/wireguard), vendored with two small marked changes | `lib/wireguard/` | BSD-3-Clause - see `lib/wireguard/LICENSE` and `library.json` |
| [miniz](https://github.com/richgel999/miniz) 1.15, its inflate (tinfl) only - the host tests | `hosttest/tinfl/` (the same inflate the ESP32 ROM carries, so the tests run the Atom's code path) | public domain |
| The suite's picture decoders ([picojpeg](https://github.com/richgel999/picojpeg), public domain, with PsiMail's PNG, GIF and progressive JPEG decoders, MIT) and [zlib](https://zlib.net/) (zlib licence) | `core/mail/engine/img/`, `core/ssh/zlib/` in the [psion-core](https://github.com/danieledge/psion-core) submodule - see its `THIRD-PARTY.md` | as listed there |
| The Arduino-ESP32 core, ESP-IDF, M5Unified (the AtomS3R's screen) | fetched by PlatformIO at the versions pinned in `platformio.ini` (not in this repository) | their own licences (Apache 2.0, MIT) |

Everything else here (the modem, the web proxy, the picture converter, the
USB network framing, the host tests and the documentation) is this
project's own code, MIT (`LICENSE`).
