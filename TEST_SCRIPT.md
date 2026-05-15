# AutoRemix v2.3.0 — Human Test Script

Covers all 11 phases of v2 development. Run top-to-bottom; each section builds on
the previous. Mark each item `[x]` as you go.

---

## Prerequisites

- [ ] A stereo WAV or AIFF file available for testing (called `INPUT.wav` below —
  replace with your actual path). Any song, ≥ 30 s, 44100 Hz stereo recommended.
- [ ] Working directory: repo root (`autoremix/`)
- [ ] Python venv activated:
  - Linux/macOS: `cd python && source .venv/bin/activate`
  - Windows: `cd python && .venv\Scripts\activate`
- [ ] cmake + ninja (Linux/macOS) or Visual Studio 2022 (Windows) available (Section C)
- [ ] `demucs` installed for Section B-3b:
  `pip install demucs` (~2 GB — skip B-3b if not installed)

---

## Section A — Automated Baseline

> **Must pass before anything else. If A fails, do not proceed.**

### A-1: pytest suite

```bash
cd python
python -m pytest tests/ -v
```

**Expected:** `27 passed` (demucs test skips if demucs absent — that is OK).
No failures. Deprecation warnings are acceptable.

- [ ] Pass — `27 passed` (or `26 passed, 1 skipped` without demucs)
- [ ] Note any failures: _______________

### A-2: Sidecar imports cleanly

```bash
python -c "from server.main import app; print('import ok')"
```

**Expected:** `import ok`

- [ ] Pass

### A-3: OP_REGISTRY has all 6 effect ops

```bash
python -c "from server.remix.ops import OP_REGISTRY; print(sorted(OP_REGISTRY))"
```

**Expected:**
```
['bass_boost', 'chop', 'eq_highpass', 'pitch_shift', 'reverb', 'time_stretch']
```

- [ ] Pass

### A-4: DemucsSeparator availability check

```bash
python -c "from server.separators.demucs_sep import DemucsSeparator; print('demucs available:', DemucsSeparator().is_available())"
```

- [ ] If demucs installed: prints `demucs available: True`
- [ ] If demucs absent: prints `demucs available: False` (no crash, no ImportError)

---

## Section B — Python Sidecar (HTTP)

Start the sidecar in a separate terminal and leave it running for all of Section B:

```bash
cd python
python server/main.py
```

**Expected startup output:**
```
INFO:     Uvicorn running on http://127.0.0.1:17432
```

- [ ] Sidecar started without error

---

### B-1: Health check

```bash
curl -s http://127.0.0.1:17432/api/v1/health | python -m json.tool
```

**Expected:** `"status": "ok"` with non-empty lists.

- [ ] `status` = `"ok"`
- [ ] `available_engines` contains `chopped_screwed`, `slowed_reverb`, `drum_and_bass`
- [ ] `available_separators` contains `algorithmic`
- [ ] `available_separators` also contains `demucs` *(only if demucs installed)*

---

### B-2: Preset list

```bash
curl -s http://127.0.0.1:17432/api/v1/presets | python -m json.tool
```

**Expected:** JSON array with exactly 3 built-in presets.

- [ ] Exactly 3 presets returned
- [ ] IDs: `chopped_screwed`, `slowed_reverb`, `drum_and_bass`
- [ ] Each has `params`, `stem_mix`

---

### B-3: Stem separation — algorithmic

Replace `/absolute/path/to/INPUT.wav` with your actual path.

```bash
curl -s -X POST http://127.0.0.1:17432/api/v1/separate \
  -H "Content-Type: application/json" \
  -d '{"input_path": "/absolute/path/to/INPUT.wav", "output_dir": "/tmp/autoremix_test", "separator_id": "algorithmic"}' \
  | python -m json.tool
```

- [ ] `success` = `true`
- [ ] 4 stems returned: `vocals`, `drums`, `bass`, `other`
- [ ] All 4 files exist and are > 0 bytes:

```bash
ls -lh /tmp/autoremix_test/stems/*/
```

**Set shell variables for later sections:**

```bash
STEMS_DIR=$(ls -d /tmp/autoremix_test/stems/*/); echo $STEMS_DIR
VOCALS=$(ls $STEMS_DIR/vocals.wav 2>/dev/null || ls $STEMS_DIR/*vocals*.wav)
DRUMS=$(ls  $STEMS_DIR/drums.wav  2>/dev/null || ls $STEMS_DIR/*drums*.wav)
BASS=$(ls   $STEMS_DIR/bass.wav   2>/dev/null || ls $STEMS_DIR/*bass*.wav)
OTHER=$(ls  $STEMS_DIR/other.wav  2>/dev/null || ls $STEMS_DIR/*other*.wav)
echo "vocals=$VOCALS drums=$DRUMS bass=$BASS other=$OTHER"
```

- [ ] All 4 variables set to existing file paths

---

### B-3b: Stem separation — Demucs ML *(skip if demucs not installed)*

```bash
curl -s -X POST http://127.0.0.1:17432/api/v1/separate \
  -H "Content-Type: application/json" \
  -d '{"input_path": "/absolute/path/to/INPUT.wav", "output_dir": "/tmp/autoremix_test_demucs", "separator_id": "demucs"}' \
  | python -m json.tool
```

**Note:** Demucs CPU inference takes 1–5 minutes for a typical song. The sidecar
timeout is 300 s. Be patient — no output until inference completes.

- [ ] `success` = `true`
- [ ] 4 stems returned: `vocals`, `drums`, `bass`, `other`
- [ ] All 4 files exist and are > 0 bytes
- [ ] **Listen:** stems are noticeably cleaner than the algorithmic separator —
  minimal bleed between vocals and instruments

---

### B-4: Built-in engine remix (×3)

#### B-4a: Chopped & Screwed

```bash
curl -s -X POST http://127.0.0.1:17432/api/v1/remix \
  -H "Content-Type: application/json" \
  -d "{\"vocals_path\":\"$VOCALS\",\"drums_path\":\"$DRUMS\",\"bass_path\":\"$BASS\",\"other_path\":\"$OTHER\",\"output_path\":\"/tmp/autoremix_test/out_cs.wav\",\"engine_id\":\"chopped_screwed\"}" \
  | python -m json.tool
```

- [ ] `success` = `true`; `/tmp/autoremix_test/out_cs.wav` > 0 bytes
- [ ] **Listen:** slower tempo, lower pitch, periodic chops audible

#### B-4b: Slowed Reverb

```bash
curl -s -X POST http://127.0.0.1:17432/api/v1/remix \
  -H "Content-Type: application/json" \
  -d "{\"vocals_path\":\"$VOCALS\",\"drums_path\":\"$DRUMS\",\"bass_path\":\"$BASS\",\"other_path\":\"$OTHER\",\"output_path\":\"/tmp/autoremix_test/out_sr.wav\",\"engine_id\":\"slowed_reverb\"}" \
  | python -m json.tool
```

- [ ] `success` = `true`; `/tmp/autoremix_test/out_sr.wav` > 0 bytes
- [ ] **Listen:** slower tempo, heavy reverb tail

#### B-4c: Drum and Bass

```bash
curl -s -X POST http://127.0.0.1:17432/api/v1/remix \
  -H "Content-Type: application/json" \
  -d "{\"vocals_path\":\"$VOCALS\",\"drums_path\":\"$DRUMS\",\"bass_path\":\"$BASS\",\"other_path\":\"$OTHER\",\"output_path\":\"/tmp/autoremix_test/out_dnb.wav\",\"engine_id\":\"drum_and_bass\"}" \
  | python -m json.tool
```

- [ ] `success` = `true`; `/tmp/autoremix_test/out_dnb.wav` > 0 bytes
- [ ] **Listen:** drums faster than other stems, bass more present

---

### B-5: Effect Chain DSL — custom chain preset

#### B-5a: Create and install preset

```bash
# Linux/macOS
mkdir -p ~/.config/autoremix/modes/
cat > ~/.config/autoremix/modes/my_chain.json << 'EOF'
{
  "id": "my_chain",
  "version": "2.0",
  "name": "My Chain",
  "params": {
    "tempo_factor": 0.75, "pitch_shift_semi": -2.0, "reverb_mix": 0.1,
    "chop_interval_ms": 0.0, "bass_boost_db": 0.0, "drums_tempo_factor": 1.0
  },
  "stem_mix": {"vocals": 1.0, "drums": 1.0, "bass": 1.2, "other": 0.8},
  "effects": [
    {"op": "time_stretch", "stems": "all",      "params": {"factor": 0.75}},
    {"op": "reverb",       "stems": "all",      "params": {"mix": 0.20, "room_size": 0.7}},
    {"op": "bass_boost",   "stems": ["bass"],   "params": {"db": 6.0}},
    {"op": "eq_highpass",  "stems": ["other"],  "params": {"cutoff_hz": 300}}
  ]
}
EOF
```

```bat
rem Windows
mkdir %APPDATA%\autoremix\modes
rem Create the file above at %APPDATA%\autoremix\modes\my_chain.json
```

Stop the sidecar (Ctrl-C) and restart it to pick up the new preset.

#### B-5b: Verify preset appears

```bash
curl -s http://127.0.0.1:17432/api/v1/presets | python -m json.tool
```

- [ ] Now **4** presets (3 built-in + `my_chain`)
- [ ] `my_chain` has `"name": "My Chain"`

#### B-5c: Trigger chain interpreter

```bash
curl -s -X POST http://127.0.0.1:17432/api/v1/remix \
  -H "Content-Type: application/json" \
  -d "{\"vocals_path\":\"$VOCALS\",\"drums_path\":\"$DRUMS\",\"bass_path\":\"$BASS\",\"other_path\":\"$OTHER\",\"output_path\":\"/tmp/autoremix_test/out_chain.wav\",\"engine_id\":\"my_chain\"}" \
  | python -m json.tool
```

- [ ] `success` = `true`; `/tmp/autoremix_test/out_chain.wav` > 0 bytes
- [ ] Sidecar log: `Remixing via effect chain my_chain` (not `with engine`)
- [ ] **Listen:** slower (~0.75×), noticeable reverb, bass boost

#### B-5d: Duration check

```bash
python -c "
import soundfile as sf
orig, sr = sf.read('$VOCALS')
out, _   = sf.read('/tmp/autoremix_test/out_chain.wav')
expected = (len(orig) / sr) / 0.75
actual   = len(out) / sr
print(f'Ratio: {actual/expected:.3f}  (should be 0.90–1.10)')
"
```

- [ ] Ratio between 0.90 and 1.10

---

### B-6: Error handling

#### B-6a: Unknown engine ID → HTTP 400

```bash
curl -s -o /dev/null -w "%{http_code}" -X POST http://127.0.0.1:17432/api/v1/remix \
  -H "Content-Type: application/json" \
  -d "{\"vocals_path\":\"$VOCALS\",\"drums_path\":\"$DRUMS\",\"bass_path\":\"$BASS\",\"other_path\":\"$OTHER\",\"output_path\":\"/tmp/err.wav\",\"engine_id\":\"does_not_exist\"}"
```

- [ ] Returns `400`
- [ ] Sidecar does NOT crash — still responds to next request

#### B-6b: Unknown effect op → success=false

```bash
python -c "
import asyncio, httpx
from server.main import app
from server.models import RemixPreset, PresetParams, StemMix
import server.main as m

preset = RemixPreset(id='bad_op', version='2.0', name='Bad',
    engine='chopped_screwed', params=PresetParams(), stem_mix=StemMix(),
    effects=[{'op': 'nonexistent', 'stems': 'all', 'params': {}}])
m._presets['bad_op'] = preset

async def run():
    async with httpx.AsyncClient(transport=httpx.ASGITransport(app=app), base_url='http://test') as c:
        r = await c.post('/api/v1/remix', json={
            'vocals_path': '$VOCALS', 'drums_path': '$DRUMS',
            'bass_path': '$BASS', 'other_path': '$OTHER',
            'output_path': '/tmp/err2.wav', 'engine_id': 'bad_op'})
        print(r.status_code, r.json())
asyncio.run(run())
"
```

- [ ] `success` = `false`; error mentions `nonexistent`
- [ ] No traceback to stdout

---

### B-7: User preset override

```bash
cat > ~/.config/autoremix/modes/chopped_screwed.json << 'EOF'
{
  "id": "chopped_screwed",
  "version": "1.0",
  "name": "Chop & Screw (My Override)",
  "params": {
    "tempo_factor": 0.60, "pitch_shift_semi": -6.0, "reverb_mix": 0.10,
    "chop_interval_ms": 1000.0, "bass_boost_db": 3.0, "drums_tempo_factor": 1.0
  },
  "stem_mix": {"vocals": 1.0, "drums": 1.0, "bass": 1.0, "other": 1.0},
  "effects": []
}
EOF
```

Restart sidecar, then:

```bash
curl -s http://127.0.0.1:17432/api/v1/presets \
  | python -c "import sys,json; p=[x for x in json.load(sys.stdin) if x['id']=='chopped_screwed'][0]; print(p['name'], p['params']['tempo_factor'])"
```

- [ ] Output: `Chop & Screw (My Override) 0.6`

Clean up:

```bash
rm ~/.config/autoremix/modes/chopped_screwed.json
```

---

## Section C — Plugin UI

> Sidecar must be running. Restart with `my_chain` preset installed (B-5a) for full tab tests.

### C-1: Build

**Linux/macOS:**
```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
```

**Windows:**
```bat
cmake -B build -G "Visual Studio 17 2022" -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug --parallel
```

- [ ] Build completes with 0 errors
- [ ] Standalone binary present: `build/AutoRemix_artefacts/Debug/Standalone/AutoRemix` (`.exe` on Windows)
- [ ] VST3 bundle present: `build/AutoRemix_artefacts/Debug/VST3/AutoRemix.vst3/`

---

### C-2: Standalone app — launch

```bash
AUTOREMIX_SERVER_PATH=$(pwd)/python/server/main.py \
  build/AutoRemix_artefacts/Debug/Standalone/AutoRemix
```

- [ ] Window opens without crash (600×400, dark theme)
- [ ] No assertion failures or JUCE leak detector alerts in terminal

---

### C-3: Sidecar health dot

- [ ] Dot in top-right corner is **green** when sidecar running
- [ ] Dot turns **red** within a few seconds after stopping sidecar
- [ ] Dot returns to **green** after restarting sidecar

---

### C-4: Preset tab bar — dynamic population

With sidecar running and `my_chain` preset installed:

- [ ] Tab bar shows **4 tabs**: `Chop & Screw`, `Slowed Reverb`, `Drum and Bass`, `My Chain`
- [ ] Tab labels match `name` field from `/api/v1/presets`

With sidecar **not** running:

- [ ] Tab bar shows fallback labels — does not crash

---

### C-5: Tab switching updates sliders

- [ ] Click **Chop & Screw** → Tempo ≈ 0.70, Pitch ≈ −4.0
- [ ] Click **Slowed Reverb** → Tempo matches preset
- [ ] Click **Drum and Bass** → Tempo matches preset
- [ ] Click **My Chain** → Tempo ≈ 0.75
- [ ] No crash switching tabs rapidly

---

### C-6: Load audio file

- [ ] Click **Load** / Open button
- [ ] **Linux/WSL2:** JUCE file browser opens (not native OS dialog)
- [ ] **Windows/macOS:** native OS file dialog opens
- [ ] Select `INPUT.wav` → waveform renders in display
- [ ] Status label updates
- [ ] No crash

---

### C-7: Play (separate + remix)

With file loaded and sidecar running:

- [ ] Select **Chop & Screw** → click **Play**
- [ ] Status label shows separation progress, then remix progress
- [ ] Output WAV written (check sidecar log for output path)
- [ ] **Listen:** slower, pitch-shifted, chopped

- [ ] Repeat with **Slowed Reverb** → reverb character audible
- [ ] Repeat with **My Chain** → sidecar log says `Remixing via effect chain my_chain`

---

### C-8: Play with sidecar down

Stop sidecar. With file loaded:

- [ ] Click **Play**
- [ ] Status label shows error message — no crash
- [ ] UI remains responsive

---

### C-9: VST3 in REAPER *(if available)*

```bash
# Linux: copy VST3 to scan path
cp -r build/AutoRemix_artefacts/Debug/VST3/AutoRemix.vst3 ~/.vst3/

# Windows: copy to C:\Program Files\Common Files\VST3\
```

- [ ] REAPER → Options → VST → Re-scan → `AutoRemix` appears
- [ ] Insert on track → plugin window opens, 600×400, dark theme
- [ ] Tab bar, sliders, Load/Play/Save all visible
- [ ] Play button functional (same as C-7)

---

## Section D — Regression Check

### D-1: Full pytest suite still green

```bash
cd python
python -m pytest tests/ -v
```

- [ ] `27 passed` (or `26 passed, 1 skipped` without demucs) — same as A-1

### D-2: Built-in engines still work

- [ ] `/tmp/autoremix_test/out_cs.wav` — playable
- [ ] `/tmp/autoremix_test/out_sr.wav` — playable
- [ ] `/tmp/autoremix_test/out_dnb.wav` — playable

### D-3: Chain preset still works

- [ ] `/tmp/autoremix_test/out_chain.wav` — playable, correct character

### D-4: Demucs stems quality check *(if B-3b was run)*

- [ ] `/tmp/autoremix_test_demucs/` — 4 stems exist
- [ ] **Listen:** demucs stems noticeably cleaner than algorithmic stems from B-3

---

## Results Summary

| Section | Items | Passed | Failed | Notes |
|---------|-------|--------|--------|-------|
| A-1 pytest | 1 | | | |
| A-2 import | 1 | | | |
| A-3 OP_REGISTRY | 1 | | | |
| A-4 demucs avail | 2 | | | |
| B-1 Health | 4 | | | |
| B-2 Presets | 3 | | | |
| B-3 Algorithmic sep | 4 | | | |
| B-3b Demucs sep | 4 | | | *(skip if demucs absent)* |
| B-4 Engine remix ×3 | 9 | | | |
| B-5 Chain DSL | 7 | | | |
| B-6 Error handling | 4 | | | |
| B-7 User override | 1 | | | |
| C-1 Build | 3 | | | |
| C-2 Standalone launch | 2 | | | |
| C-3 Health dot | 3 | | | |
| C-4 Tab bar | 4 | | | |
| C-5 Tab switching | 5 | | | |
| C-6 File load | 4 | | | |
| C-7 Play | 5 | | | |
| C-8 Play sidecar down | 3 | | | |
| C-9 VST3 REAPER | 4 | | | |
| D Regression | 7 | | | |
| **Total** | **86** *(+4 demucs)* | | | |

---

## Known Limitations (not bugs)

- Demucs CPU inference: 1–5 minutes for a 3-minute song. Normal — sidecar timeout is 300 s.
- Algorithmic separator has poor stem isolation (band-split only). Use `demucs` for quality.
- Sidecar health dot polling: brief sidecar restarts may not be detected immediately.
- Duration ratio check (B-5d) allows 10% tolerance — librosa `time_stretch` is approximate.
- WSL2: native OS file dialog not available — JUCE file browser is used instead.
- No real-time processing — offline batch only.
