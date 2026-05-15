# AutoRemix v2.2.0 — Human Test Script

Covers all 10 phases of v2 development. Run top-to-bottom; each section builds on
the previous. Mark each item `[x]` as you go.

---

## Prerequisites

- [ ] A stereo WAV or AIFF file available for testing (called `INPUT.wav` below —
  replace with your actual path). Any song, ≥ 30s, 44100 Hz stereo recommended.
- [ ] Working directory: repo root (`autoremix/`)
- [ ] Python venv activated: `cd python && source .venv/bin/activate`
- [ ] cmake + ninja available in PATH (for Section C)

---

## Section A — Automated Baseline

> **Must pass before anything else. If A fails, do not proceed.**

### A-1: pytest suite

```bash
cd python
source .venv/bin/activate
python -m pytest tests/ -v
```

**Expected:** `26 passed` (no failures, only deprecation warnings acceptable)

- [ ] Pass — `26 passed`
- [ ] Note any failures: _______________

### A-2: Sidecar imports cleanly

```bash
python -c "from server.main import app; print('import ok')"
```

**Expected:** `import ok` (no ImportError, no traceback)

- [ ] Pass

### A-3: OP_REGISTRY has all 6 ops

```bash
python -c "from server.remix.ops import OP_REGISTRY; print(sorted(OP_REGISTRY))"
```

**Expected:**
```
['bass_boost', 'chop', 'eq_highpass', 'pitch_shift', 'reverb', 'time_stretch']
```

- [ ] Pass

---

## Section B — Python Sidecar (HTTP)

Start the sidecar in a separate terminal and leave it running for all of Section B:

```bash
cd python
source .venv/bin/activate
python server/main.py
```

**Expected startup output:**
```
INFO:     Started server process [...]
INFO:     Uvicorn running on http://127.0.0.1:17432
```

- [ ] Sidecar started without error

---

### B-1: Health check

```bash
curl -s http://127.0.0.1:17432/api/v1/health | python -m json.tool
```

**Expected:** JSON with `"status": "ok"` and non-empty `available_engines` and
`available_separators` lists.

- [ ] `status` = `"ok"`
- [ ] `available_engines` contains `chopped_screwed`, `slowed_reverb`, `drum_and_bass`
- [ ] `available_separators` contains `algorithmic`

---

### B-2: Preset list

```bash
curl -s http://127.0.0.1:17432/api/v1/presets | python -m json.tool
```

**Expected:** JSON array with exactly 3 objects (built-in presets). Each has `id`,
`name`, `params`, `stem_mix`.

- [ ] Exactly 3 presets returned
- [ ] IDs present: `chopped_screwed`, `slowed_reverb`, `drum_and_bass`
- [ ] Each preset has `params.tempo_factor`, `params.pitch_shift_semi`, etc.
- [ ] Each preset has `stem_mix` with `vocals`, `drums`, `bass`, `other`

---

### B-3: Stem separation

Replace `INPUT.wav` with your actual file path.

```bash
curl -s -X POST http://127.0.0.1:17432/api/v1/separate \
  -H "Content-Type: application/json" \
  -d '{"input_path": "/absolute/path/to/INPUT.wav", "output_dir": "/tmp/autoremix_test", "separator_id": "algorithmic"}' \
  | python -m json.tool
```

**Expected:** `"success": true` with paths to 4 stem WAV files.

- [ ] `success` = `true`
- [ ] 4 stems returned: `vocals`, `drums`, `bass`, `other`
- [ ] All 4 files exist on disk:

```bash
ls -lh /tmp/autoremix_test/stems/*/
```

- [ ] Each stem file is > 0 bytes
- [ ] Each stem file is a valid WAV (optional: `file /tmp/autoremix_test/stems/*/*.wav`)

**Note the stem paths** — needed for B-4 and B-5. Set shell variables:

```bash
STEMS_DIR=$(ls -d /tmp/autoremix_test/stems/*/); echo $STEMS_DIR
VOCALS=$(ls $STEMS_DIR/vocals.wav 2>/dev/null || ls $STEMS_DIR/*vocals*.wav)
DRUMS=$(ls  $STEMS_DIR/drums.wav  2>/dev/null || ls $STEMS_DIR/*drums*.wav)
BASS=$(ls   $STEMS_DIR/bass.wav   2>/dev/null || ls $STEMS_DIR/*bass*.wav)
OTHER=$(ls  $STEMS_DIR/other.wav  2>/dev/null || ls $STEMS_DIR/*other*.wav)
echo "vocals=$VOCALS drums=$DRUMS bass=$BASS other=$OTHER"
```

---

### B-4: Built-in engine remix (×3)

Run all 3. Each should produce a valid output WAV.

#### B-4a: Chopped & Screwed

```bash
curl -s -X POST http://127.0.0.1:17432/api/v1/remix \
  -H "Content-Type: application/json" \
  -d "{\"vocals_path\":\"$VOCALS\",\"drums_path\":\"$DRUMS\",\"bass_path\":\"$BASS\",\"other_path\":\"$OTHER\",\"output_path\":\"/tmp/autoremix_test/out_cs.wav\",\"engine_id\":\"chopped_screwed\"}" \
  | python -m json.tool
```

- [ ] `success` = `true`
- [ ] `/tmp/autoremix_test/out_cs.wav` exists and is > 0 bytes
- [ ] **Listen:** tempo is slower, pitch is lower, periodic chops audible

#### B-4b: Slowed Reverb

```bash
curl -s -X POST http://127.0.0.1:17432/api/v1/remix \
  -H "Content-Type: application/json" \
  -d "{\"vocals_path\":\"$VOCALS\",\"drums_path\":\"$DRUMS\",\"bass_path\":\"$BASS\",\"other_path\":\"$OTHER\",\"output_path\":\"/tmp/autoremix_test/out_sr.wav\",\"engine_id\":\"slowed_reverb\"}" \
  | python -m json.tool
```

- [ ] `success` = `true`
- [ ] `/tmp/autoremix_test/out_sr.wav` exists and is > 0 bytes
- [ ] **Listen:** tempo is slower, heavy reverb tail audible

#### B-4c: Drum and Bass

```bash
curl -s -X POST http://127.0.0.1:17432/api/v1/remix \
  -H "Content-Type: application/json" \
  -d "{\"vocals_path\":\"$VOCALS\",\"drums_path\":\"$DRUMS\",\"bass_path\":\"$BASS\",\"other_path\":\"$OTHER\",\"output_path\":\"/tmp/autoremix_test/out_dnb.wav\",\"engine_id\":\"drum_and_bass\"}" \
  | python -m json.tool
```

- [ ] `success` = `true`
- [ ] `/tmp/autoremix_test/out_dnb.wav` exists and is > 0 bytes
- [ ] **Listen:** drums are noticeably faster than other stems, bass has more presence

---

### B-5: Effect Chain DSL — custom chain preset

#### B-5a: Create chain preset file

```bash
cat > /tmp/my_chain.json << 'EOF'
{
  "id": "my_chain",
  "version": "2.0",
  "name": "My Chain",
  "params": {
    "tempo_factor": 0.75,
    "pitch_shift_semi": -2.0,
    "reverb_mix": 0.1,
    "chop_interval_ms": 0.0,
    "bass_boost_db": 0.0,
    "drums_tempo_factor": 1.0
  },
  "stem_mix": {"vocals": 1.0, "drums": 1.0, "bass": 1.2, "other": 0.8},
  "effects": [
    {"op": "time_stretch", "stems": "all",       "params": {"factor": 0.75}},
    {"op": "reverb",       "stems": "all",       "params": {"mix": 0.20, "room_size": 0.7}},
    {"op": "bass_boost",   "stems": ["bass"],    "params": {"db": 6.0}},
    {"op": "eq_highpass",  "stems": ["other"],   "params": {"cutoff_hz": 300}}
  ]
}
EOF
```

#### B-5b: Install preset and restart sidecar

```bash
mkdir -p ~/.config/autoremix/modes/
cp /tmp/my_chain.json ~/.config/autoremix/modes/
```

Stop the running sidecar (Ctrl-C), restart it:

```bash
python server/main.py
```

#### B-5c: Verify preset appears in list

```bash
curl -s http://127.0.0.1:17432/api/v1/presets | python -m json.tool
```

- [ ] Now shows **4** presets (3 built-in + `my_chain`)
- [ ] `my_chain` has `"id": "my_chain"` and `"name": "My Chain"`

#### B-5d: Trigger chain interpreter via remix

```bash
curl -s -X POST http://127.0.0.1:17432/api/v1/remix \
  -H "Content-Type: application/json" \
  -d "{\"vocals_path\":\"$VOCALS\",\"drums_path\":\"$DRUMS\",\"bass_path\":\"$BASS\",\"other_path\":\"$OTHER\",\"output_path\":\"/tmp/autoremix_test/out_chain.wav\",\"engine_id\":\"my_chain\"}" \
  | python -m json.tool
```

- [ ] `success` = `true`
- [ ] `/tmp/autoremix_test/out_chain.wav` exists and is > 0 bytes
- [ ] **Listen:** tempo is slower (~0.75×), noticeable reverb, bass boost present
- [ ] Sidecar log says `Remixing via effect chain my_chain` (not `with engine`)

#### B-5e: Duration proportional to time_stretch factor

```bash
python -c "
import soundfile as sf
orig, sr = sf.read('$VOCALS')
out, _   = sf.read('/tmp/autoremix_test/out_chain.wav')
orig_dur = len(orig) / sr
out_dur  = len(out)  / sr
expected = orig_dur / 0.75
ratio    = out_dur / expected
print(f'Original stem: {orig_dur:.2f}s')
print(f'Output:        {out_dur:.2f}s')
print(f'Expected (~):  {expected:.2f}s')
print(f'Ratio:         {ratio:.3f}  (should be close to 1.0)')
"
```

- [ ] Ratio is between 0.90 and 1.10 (within 10% of expected duration)

---

### B-6: Error handling

#### B-6a: Unknown engine ID

```bash
curl -s -X POST http://127.0.0.1:17432/api/v1/remix \
  -H "Content-Type: application/json" \
  -d "{\"vocals_path\":\"$VOCALS\",\"drums_path\":\"$DRUMS\",\"bass_path\":\"$BASS\",\"other_path\":\"$OTHER\",\"output_path\":\"/tmp/autoremix_test/err.wav\",\"engine_id\":\"does_not_exist\"}" \
  | python -m json.tool
```

- [ ] HTTP 400 returned (not 500, not 200)
- [ ] Error message mentions `does_not_exist`
- [ ] Sidecar does NOT crash — still responds to next request

#### B-6b: Unknown op in chain preset (inject via direct call)

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
            'output_path': '/tmp/autoremix_test/err2.wav',
            'engine_id': 'bad_op'})
        print(r.status_code, r.json())

asyncio.run(run())
"
```

- [ ] Response has `success` = `false` and error mentions `nonexistent`
- [ ] No Python traceback escapes to stdout

---

### B-7: User preset override

Built-in `chopped_screwed` can be overridden by a user preset with the same `id`.

```bash
cat > ~/.config/autoremix/modes/chopped_screwed.json << 'EOF'
{
  "id": "chopped_screwed",
  "version": "1.0",
  "name": "Chop & Screw (My Override)",
  "params": {
    "tempo_factor": 0.60,
    "pitch_shift_semi": -6.0,
    "reverb_mix": 0.10,
    "chop_interval_ms": 1000.0,
    "bass_boost_db": 3.0,
    "drums_tempo_factor": 1.0
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

- [ ] Output shows `Chop & Screw (My Override) 0.6` (user file took precedence)

Clean up after:

```bash
rm ~/.config/autoremix/modes/chopped_screwed.json
```

---

## Section C — Plugin UI

> Requires a successful cmake build. Sidecar must be running from Section B
> (or restart it before running the plugin).

### C-1: Build

```bash
cd /path/to/autoremix   # repo root
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
```

- [ ] Build completes with 0 errors
- [ ] Warning count is stable (same as previous builds; no new warnings introduced)
- [ ] Standalone binary present:

```bash
ls build/AutoRemix_artefacts/Debug/Standalone/AutoRemix
```

- [ ] VST3 bundle present:

```bash
ls build/AutoRemix_artefacts/Debug/VST3/AutoRemix.vst3
```

---

### C-2: Standalone app — launch

```bash
# Start sidecar first (separate terminal)
cd python && source .venv/bin/activate && python server/main.py

# Launch plugin (in another terminal)
AUTOREMIX_SERVER_PATH=$(pwd)/python/server/main.py \
  build/AutoRemix_artefacts/Debug/Standalone/AutoRemix
```

- [ ] App window opens without crash (600×400)
- [ ] No assertion failures or JUCE leak detector alerts in terminal output

---

### C-3: Sidecar health indicator

With sidecar running:

- [ ] Small dot in top-right corner of plugin window is **green**

Stop the sidecar (Ctrl-C in its terminal):

- [ ] Dot turns **red** within a few seconds (polling interval)

Restart sidecar:

- [ ] Dot returns to **green**

---

### C-4: Preset tab bar — dynamic population

With sidecar running and `my_chain` preset installed (from B-5b):

- [ ] Tab bar shows **4 tabs** (not hardcoded 3): `Chop & Screw`, `Slowed Reverb`,
  `Drum and Bass`, `My Chain`
- [ ] Tab labels match `name` field from `/api/v1/presets`

With sidecar **not** running:

- [ ] Tab bar shows default fallback labels (does not crash)
- [ ] No assertion failure

---

### C-5: Tab switching updates slider defaults

With sidecar running:

- [ ] Click **Chop & Screw** tab → Tempo slider ≈ 0.70, Pitch ≈ −4.0
- [ ] Click **Slowed Reverb** tab → Tempo slider matches that preset's `tempo_factor`
- [ ] Click **Drum and Bass** tab → Tempo slider matches that preset's `tempo_factor`
- [ ] Click **My Chain** tab → Tempo slider ≈ 0.75 (as defined in `my_chain.json`)
- [ ] No crash when switching tabs rapidly

---

### C-6: Load audio file

- [ ] Click **Open** / file chooser button
- [ ] Native file dialog does NOT open (WSL2 / headless: `useNativeFileDialog=false`)
  — JUCE file chooser opens instead
- [ ] Navigate to `INPUT.wav` and select it
- [ ] Waveform renders in the waveform display zone
- [ ] Status label shows something like `Loaded: INPUT.wav` (or similar)
- [ ] No crash

---

### C-7: Play (separate + remix)

With a file loaded and sidecar running:

- [ ] Select **Chop & Screw** tab
- [ ] Click **Play** button
- [ ] Status label updates to indicate separation in progress
- [ ] Status label updates to indicate remix in progress
- [ ] After completion: status label shows success or output path
- [ ] Output WAV written to `/tmp/autoremix/` (or wherever the plugin writes it)
- [ ] **Listen:** output matches Chopped & Screwed character (slower, pitch-shifted, chopped)

Repeat with **Slowed Reverb** tab:

- [ ] Output WAV exists and has reverb character

Repeat with **My Chain** tab:

- [ ] Output WAV exists — routed via chain interpreter (check sidecar log for
  `Remixing via effect chain my_chain`)

---

### C-8: Play with sidecar down

Stop sidecar. With plugin open and file loaded:

- [ ] Click **Play**
- [ ] Status label shows error message (e.g. "Sidecar not available" or similar)
- [ ] Plugin does **not** crash or freeze
- [ ] UI remains responsive after error

---

### C-9: VST3 in REAPER (if available)

```bash
# Copy VST3 to REAPER search path (Linux default):
cp -r build/AutoRemix_artefacts/Debug/VST3/AutoRemix.vst3 ~/.vst3/
```

- [ ] Open REAPER → Options → Preferences → VST → Re-scan
- [ ] `AutoRemix` appears in plugin list
- [ ] Insert on a track → plugin window opens without crash
- [ ] UI renders correctly (600×400, dark theme, tab bar, sliders visible)
- [ ] Play button functional (same checks as C-7 apply)

---

## Section D — Regression Check

After completing B and C, confirm nothing regressed.

### D-1: Automated suite still green

```bash
cd python
python -m pytest tests/ -v
```

- [ ] **26 passed** (same count as A-1)

### D-2: All 3 built-in engines still work end-to-end

(Confirm output files from B-4 still exist and are playable)

- [ ] `/tmp/autoremix_test/out_cs.wav` — playable
- [ ] `/tmp/autoremix_test/out_sr.wav` — playable
- [ ] `/tmp/autoremix_test/out_dnb.wav` — playable

### D-3: Chain preset output still works

- [ ] `/tmp/autoremix_test/out_chain.wav` — playable, correct character

---

## Results Summary

Fill in after completing the full script.

| Section | Items | Passed | Failed | Notes |
|---------|-------|--------|--------|-------|
| A — Automated baseline | 3 | | | |
| B-1 Health | 3 | | | |
| B-2 Presets | 4 | | | |
| B-3 Separation | 4 | | | |
| B-4 Engine remix | 9 | | | |
| B-5 Chain DSL | 8 | | | |
| B-6 Error handling | 4 | | | |
| B-7 User override | 1 | | | |
| C-1 Build | 4 | | | |
| C-2 Standalone launch | 2 | | | |
| C-3 Health dot | 3 | | | |
| C-4 Tab bar | 4 | | | |
| C-5 Tab switching | 5 | | | |
| C-6 File load | 5 | | | |
| C-7 Play | 6 | | | |
| C-8 Play sidecar down | 3 | | | |
| C-9 VST3 REAPER | 4 | | | |
| D Regression | 5 | | | |
| **Total** | **87** | | | |

---

## Known Limitations (not bugs)

- Algorithmic FFT separator produces rough stems — vocals leaks into other stems.
  This is expected; Demucs ML separator is deferred to a future phase.
- `useNativeFileDialog=false` means no OS file picker on WSL2 — JUCE file browser
  used instead.
- Sidecar health dot polling interval may be several seconds — brief sidecar restarts
  may not be detected immediately.
- Duration ratio check (B-5e) allows 10% tolerance because librosa `time_stretch`
  is approximate.
