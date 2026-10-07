# md4c

- Version: **0.5.2**
- Upstream: <https://github.com/mity/md4c> (tag `release-0.5.2`)
- License: **MIT** (see [LICENSE](LICENSE))

| File | Taken from upstream path |
| --- | --- |
| `md4c.c` | `src/md4c.c` |
| `md4c.h` | `src/md4c.h` |

Only the parser is vendored: it turns Markdown into the callbacks in `md4c.h`, and
dazzle's own writers (`src/markdown/`) turn those into the markup the update windows
show — today the HTML that AppKit renders, with the Windows rich edit control's RTF to
follow in the same directory. md4c's own renderers (`md4c-html.c`, `md4c-html.h`) are not
used, so the windows cannot render anything but the subset in
`../../specs/updater-ui.md`.

`sha256`:

```
52e39843b96b64d0281bf795128bcc88c9c4dc52d7436737e9f4306648dc4590  md4c.c
4efd19bf7ec270691d5b4189f496886e421768a814b5e817eb945aa85e859f18  md4c.h
```
