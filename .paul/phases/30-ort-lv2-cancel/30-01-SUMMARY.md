---
phase: 30-ort-lv2-cancel
plan: 01
type: summary
status: complete
commit: ecd0d6d
date: 2026-05-29
---

# Summary: 30-01 — ORT 1.17→1.21 + SHA256 pinning

## What was built

- `ORT_VERSION` bumped `1.17.0` → `1.21.0` in `CMakeLists.txt`
- All 5 ORT package SHA256 values pinned (previously empty `""`):
  - Linux CPU: `7485c7e...`
  - macOS universal2: `3c3cfc7...`
  - Windows CPU: `5c07bb2...`
  - Linux GPU: `ef37a33...`
  - DirectML NuGet 1.21.0: `12bed6e...`
- DirectML NuGet URL fixed: v2 API returned 404 for this package.
  Changed to v3 flatcontainer format:
  `https://api.nuget.org/v3-flatcontainer/microsoft.ml.onnxruntime.directml/{ver}/microsoft.ml.onnxruntime.directml.{ver}.nupkg`

## Decisions
- NuGet v3 flatcontainer URL used for DirectML (v2 API 404s for this package)
- Linux GPU SHA pinned after background download of ~1GB tarball
- No ORT API changes required — 1.17→1.21 C++ API backward compatible

## Acceptance criteria results
| AC | Result |
|----|--------|
| AC-1: ORT 1.21.0 builds clean | PASS — 299 targets, 0 errors, Linux CPU verified |
| AC-2: SHA256 pinned for all packages | PASS — no empty ORT_SHA fields remain |
| AC-3: GPU builds unaffected | PASS — configure with -DAUTOREMIX_GPU=ON exits 0 |
