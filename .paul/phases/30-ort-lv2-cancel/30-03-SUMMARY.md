---
phase: 30-ort-lv2-cancel
plan: 03
type: summary
status: complete
commits: 1c9a4ff, 2f5cdd2, 770f447, bcd92d5, 2c4b1c3
date: 2026-05-29
---

# Summary: 30-03 — Demucs separation cancellation

## What was built

Cancel button was already present in ScreenSeparating UI. What was missing:
actual inference interruption + thread safety for multiple cancels.

### NativeDemucsSeparator.h
- `DemucsResult`: added `bool cancelled = false`
- `separate_demucs`: added `const std::atomic<bool>* cancel = nullptr` parameter
- Chunk loop checks cancel at top of each iteration — aborts between ORT chunks.
  (ORT `session.Run()` for one chunk is not interruptible; gap between checks = ~5-10s on CPU)

### ScreenContext.h
- Added `std::shared_ptr<std::atomic<bool>> separation_cancel_token`
- Includes `<atomic>` and `<memory>`

### ScreenSeparating.h
- `cancel_requested_` (stack member) replaced with `cancel_token_` (heap shared_ptr)
- `onEnter()`: creates fresh token, shares via `ctx_.separation_cancel_token`
- `onExit()`: clears `ctx_.separation_cancel_token` (screen's copy keeps atomic alive)
- `startSeparation()`: captures `cancel` (shared_ptr copy) BEFORE thread spawn —
  thread never accesses `this` after `separate_fn_` returns (was use-after-free)
- All callAsync lambdas guard with `cancel->load()` before using `this`
- `requestCancel()`: sets `ctx_.set_status("Ready")` to clear stale status bar text

### AutoRemixEditor.cpp (separateNative)
- `cancel_snap` snapshotted at top of demucs branch (keeps atomic alive through inference)
- All `health_dot_.setState()`, `callAsync` status updates, and `sep_progress` callback
  guard with `cancel_snap->load()` — silences callbacks from stale threads immediately

### ModelStatusDot.h
- `setState()` now posts state write via `callAsync` (message thread only).
  Was: direct `state_ = s` from background threads → data race → crash.

## Bugs found and fixed during verify

1. **Use-after-free #1**: Non-owning shared_ptr to stack `cancel_requested_` →
   screen destroyed → background thread accesses dangling atomic. Fix: heap-allocate.

2. **Use-after-free #2**: Thread lambda captured `this` raw → used `this->cancel_token_`
   after screen destroyed. Fix: capture `cancel_token_` by value before thread spawn.

3. **Data race / crash**: Multiple background threads (from rapid cancel→restart)
   called `health_dot_.setState()` simultaneously — `state_` not atomic → UB crash.
   Fix: `setState` posts to message thread via `callAsync`.

4. **% jumping**: Old threads' `sep_progress` callbacks fired after cancel.
   Fix: `cancel_snap` check at top of callback + inside `callAsync`.

5. **Stale status bar**: "Demucs: checking model cache" remained after cancel.
   Fix: `requestCancel()` calls `ctx_.set_status("Ready")`.

## Acceptance criteria results
| AC | Result |
|----|--------|
| AC-1: Cancel interrupts between chunks | PASS — verified on Mac |
| AC-2: Cancel button visible during separation | PASS — was already there |
| AC-3: Algorithmic path unaffected | PASS |
| Mac verify: cancel multiple times, let last complete | PASS — no crash |
