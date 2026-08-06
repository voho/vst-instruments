# Working conventions

- Per-fix audio previews (the before/after/diff takes under
  `*/Docs/audio/realism-fixes/`, `fidelity-fixes/` and similar) are
  **one-time review evidence**: render them while the change is under
  review, then leave them frozen once the pull request merges. Do not
  re-render or refresh them when later engine changes make them stale,
  and do not treat their staleness as a defect.
