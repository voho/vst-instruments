# Taikor screenshots

`taikor-standalone.png` is rendered by Taikor's own plug-in regression suite
during the Nightly workflow's macOS build, and committed back to `main` only
when the bytes actually change — so the image in the README tracks the real
editor rather than a hand-captured screenshot that quietly goes stale.

Rendering it requires a macOS build. To produce it locally:

```bash
cd taikor
TAIKOR_EDITOR_SNAPSHOT="$PWD/Docs/screenshots/taikor-standalone.png" \
  ./scripts/build-macos.sh
```

The Nightly deletes the committed image before it builds and fails if the suite
does not write it back, so the file here is always that run's own render.
