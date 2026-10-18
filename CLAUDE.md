# CLAUDE.md: PsiConnect, the Atom modem firmware

Read this before changing anything. The user-facing documentation is
`README.md`; `docs/UPGRADE.md` is the engineering record and full reference,
`docs/HARDWARE.md` the wiring and safety, `docs/VALIDATION.md` what has been
verified. This firmware is one program of the Psion Series 5mx suite
(PsiTerm, PsiMail, PsiWeb, PsiCast, PsiLinux, PsiProxy); the shared code
lives in `core/`, the psion-core submodule.

## What this is

An ESP32 (M5Stack AtomS3R, AtomS3 Lite, or the plain Atom, with the Atomic
RS232 Base: only TX, RX and GND) running Hayes modem firmware that is
compatible with what the apps' `core/ssh/psiglue.cpp` expects: `ATDT
host:port`, `CONNECT`, raw TCP, `+++` with guard times, `ATH`, `NO CARRIER`
in band. Without RTS/CTS it **paces** output to the Psion so the line never
overruns. It is also PsiWeb's web proxy (dial `psiproxy`: TLS, HTML
simplifying, pictures to 16 greys, gzip on the line), a PPP peer for the
Psion's own Internet (with a DNS relay on 192.168.7.1), a WireGuard client
and TLS termination for PsiMail.

The pins (G19 TX, G22 RX on the plain Atom) are unconfirmed on hardware;
`AT$SWAP=1` swaps them. On the S3 boards the Base's transceiver is on G5/G6.

Layout: `src/*.cpp` is plain C++ (the modem, settings and schema, the
proxy, the HTML simplifier, the picture converter, PPP, the uplink
manager, WireGuard config, the DNS relay); `src/board/` and `src/main.cpp`
are the board side; `src/usbnet/` the USB network framing. `lib/wireguard`
is vendored. `hosttest/` runs the same code against a fake hardware layer
(`fakehal.h`); `hosttest/hostmodem` is the modem as a PC program on a unix
socket, for the Psion emulator's serial bridge (psion-core's
`tools/emu/net.py`).

Extension modules (`src/ext.h`, built in by `tools/ext.py`): optional parts
kept in other repositories. A module is a directory with a `module.json`; a
link to it in `ext/` (git-ignored) or a path in `$AM_EXT_DIRS` builds it
in, and `AM_EXT=0 pio run` leaves every module out. PsiCast's module is the
`atom/` folder of a psicast checkout beside this one
(`ln -s ../../psicast/atom ext/cast`). The firmware is complete without
any module, and this repository must not name private modules.

## Building and testing

```
git clone --recurse-submodules https://github.com/danieledge/psiconnect.git
pio run -e atoms3r                 # or atoms3-lite, m5stack-atom
pio run -e atoms3r -t upload       # flash over USB-C
make -C hosttest test              # the unit tests on a PC (needs zlib, python3 with Pillow)
```

The picture decoders and zlib are compiled in place from `core/` (see
`platformio.ini` and `hosttest/sources.mk`); `git submodule update --init`
in a checkout made without `--recurse-submodules`. An extension module's own
host tests include `hosttest/sources.mk` and set `FW` (this folder) and, if
their core is elsewhere, `FW_IMG`.

Host tests to keep green after any change: `test_modem` (AT parser, guard
times, pacing, settings, schema, TLS termination, psiexec, the uplink
manager, PPP with no uplink), `test_v22` (WireGuard config, DNS relay, saved
networks, secrets never logged; with ASan/UBSan), `test_atremote`,
`test_usbnet`, `test_proxy` and `test_proxy_tinfl`, `test_imapxcode`,
`test_fwtrial`. Treat everything from the line, the network and the web
pages as untrusted; bounded copies everywhere.

## Rules

- **Never commit:** Wi-Fi SSIDs or passwords, keys, WireGuard configs
  (`*.conf`, `*.wg`), `*_local.h` (`src/webpass_local.h` is the git-ignored
  local default web password), LAN addresses, home paths, or anything from a
  private repository. Test fixtures use made-up tokens.
- Keep the AT command set compatible with WiRSa/WiFi232/Zimodem and with
  what `core/ssh/psiglue.cpp` sends; a change on either side needs the other
  checked (`docs/VALIDATION.md`).
- Settings records (`Settings2`) have a fixed layout with spare bytes; add
  settings in the spare bytes and the schema, never by changing the size.
- British English, plain short sentences, no em dashes. Commit messages in
  plain British English.
