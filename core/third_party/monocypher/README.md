# Monocypher

- Version: **4.0.2**
- Upstream: <https://github.com/LoupVaillant/Monocypher> (tag `4.0.2`)
- License: dual-licensed, **BSD-2-Clause OR CC0-1.0** (see [LICENCE](LICENCE))

| File | Taken from upstream path |
| --- | --- |
| `monocypher.c` | `src/monocypher.c` |
| `monocypher.h` | `src/monocypher.h` |
| `monocypher-ed25519.c` | `src/optional/monocypher-ed25519.c` |
| `monocypher-ed25519.h` | `src/optional/monocypher-ed25519.h` |

Only the Ed25519 API (`monocypher-ed25519`) and the monocypher core it builds on are
used, for Ed25519 signature verification.
