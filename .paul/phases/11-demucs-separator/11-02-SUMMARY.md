---
phase: 11-demucs-separator
plan: 02
type: summary
completed: 2026-05-15
commit: pending
---

# 11-02 Summary: demucs install + tests + v2.3.0 release

## What was built

- `demucs>=4.0.0` installed into venv; `DemucsSeparator().is_available()` → True
- `python/tests/test_separate.py` — `test_separate_demucs` added:
  - `pytest.importorskip("demucs")` — skips cleanly in CI without torch
  - Passes when demucs installed (27 total tests)
- `python/requirements.txt` — demucs uncommented (production dep)
- `CMakeLists.txt` — version 2.2.0 → 2.3.0
- `.paul/ROADMAP.md` — Phase 11 ✅ COMPLETE

## Bug fixed during apply

`torchaudio 2.11.0` defaults to `torchcodec` backend for both load and save —
`torchcodec` not installed. Fixed by replacing `torchaudio.load` / `torchaudio.save`
with `soundfile.read` / `soundfile.write` (already in requirements). `torchaudio`
still used for `resample` only.

## Acceptance criteria results

| AC | Result |
|----|--------|
| AC-1: demucs installs cleanly | ✓ torch 2.12.0 + torchaudio 2.11.0 |
| AC-2: is_available()=True after install | ✓ |
| AC-3: test skips when demucs absent | ✓ pytest.importorskip |
| AC-4: test passes when demucs installed | ✓ 4 stems, all > 0 bytes |
| AC-5: full suite green | ✓ 27 passed |

## Next

- Phase 12 options: Windows CI, LV2 packaging, macOS AU, or real-time processing
