# Third-party code

Vendored third-party code lives here as byte-identical upstream files: these files are
never edited, reformatted, or patched locally — to change behavior, bump the pinned
version and re-vendor the upstream file. The current contents are
[nlohmann/json](nlohmann/README.md) v3.11.3 (`nlohmann/json.hpp`, MIT,
<https://github.com/nlohmann/json>) and [Monocypher](monocypher/README.md) 4.0.2
(`monocypher/monocypher.{c,h}` plus the optional Ed25519 module, BSD-2-Clause OR CC0-1.0,
<https://github.com/LoupVaillant/Monocypher>).
