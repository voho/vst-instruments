# Taikor screenshots

`taikor-standalone.png` is rendered by Taikor's own plug-in regression suite
during the Nightly workflow's macOS build, and committed back to `main` only
when the bytes actually change — so the image in the README tracks the real
editor rather than a hand-captured screenshot that quietly goes stale.

Rendering it requires a macOS build, so the file appears here after the first
Nightly run that follows Taikor being added. To produce it locally:

```bash
cd taikor
TAIKOR_EDITOR_SNAPSHOT="$PWD/Docs/screenshots/taikor-standalone.png" \
  ./scripts/build-macos.sh
```

This directory is tracked before that file exists so the Nightly's
`git add` step has somewhere to put it.
