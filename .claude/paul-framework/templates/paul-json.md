# paul.json Template

Template for `.paul/paul.json` - machine-readable satellite manifest for external system discovery.

**Purpose:** Provides a standardized signal file that workspace management systems (BASE) can scan for, detect, and register PAUL-managed projects.

---

## File Template

```json
{
  "name": "{project_name}",
  "version": "{milestone_version}",
  "milestone": {
    "name": "{milestone_name}",
    "version": "{milestone_version}",
    "status": "not_started | in_progress | complete"
  },
  "phase": {
    "number": 0,
    "name": "{phase_name}",
    "status": "not_started | planning | in_progress | complete"
  },
  "loop": {
    "plan": null,
    "position": "IDLE | PLAN | APPLY | UNIFY"
  },
  "timestamps": {
    "created_at": "{ISO 8601}",
    "updated_at": "{ISO 8601}"
  },
  "satellite": {
    "groom": true
  }
}
```

---

## Field Reference

| Field | Type | Description |
|-------|------|-------------|
| `name` | string | Project name (from init or directory name) |
| `version` | string | Current milestone version (e.g., "0.1.0") |
| `milestone.name` | string | Current milestone name (e.g., "v0.1 Initial Release") |
| `milestone.version` | string | Milestone version tag |
| `milestone.status` | enum | `not_started`, `in_progress`, `complete` |
| `phase.number` | number | Current phase number (0 = no phase yet) |
| `phase.name` | string | Current phase name ("None" if no phase) |
| `phase.status` | enum | `not_started`, `planning`, `in_progress`, `complete` |
| `loop.plan` | string\|null | Current plan ID (e.g., "12-01") or null if idle |
| `loop.position` | enum | `IDLE`, `PLAN`, `APPLY`, `UNIFY` |
| `timestamps.created_at` | string | ISO 8601 timestamp of paul.json creation |
| `timestamps.updated_at` | string | ISO 8601 timestamp of last update |
| `satellite.groom` | boolean | Whether BASE groom should health-check this project (default: true) |

---

## Field Update Triggers

| Workflow | Fields Updated |
|----------|---------------|
| `init-project` | All fields (initial creation) |
| `plan-phase` | `loop.plan`, `loop.position` → "PLAN", `timestamps.updated_at` |
| `apply-phase` | `loop.position` → "APPLY", `timestamps.updated_at` |
| `unify-phase` | `phase.*`, `loop.position` → "IDLE", `loop.plan` → null, `timestamps.updated_at` |
| `create-milestone` | `milestone.*`, `version`, `phase.*`, `loop.*` → IDLE, `timestamps.updated_at` |
| `complete-milestone` | `milestone.status` → "complete", `timestamps.updated_at` |

**Note:** Not all workflows currently implement sync. v1.1 implements sync in: init-project, unify-phase, create-milestone, complete-milestone. Plan-phase and apply-phase sync are deferred (unify catches up).

---

## Example: Newly Initialized Project

```json
{
  "name": "my-app",
  "version": "0.0.0",
  "milestone": {
    "name": "None",
    "version": "0.0.0",
    "status": "not_started"
  },
  "phase": {
    "number": 0,
    "name": "None",
    "status": "not_started"
  },
  "loop": {
    "plan": null,
    "position": "IDLE"
  },
  "timestamps": {
    "created_at": "2026-03-17T14:00:00-05:00",
    "updated_at": "2026-03-17T14:00:00-05:00"
  },
  "satellite": {
    "groom": true
  }
}
```

## Example: Mid-Development Project

```json
{
  "name": "casegate",
  "version": "0.2.0",
  "milestone": {
    "name": "v0.2 API Layer",
    "version": "0.2.0",
    "status": "in_progress"
  },
  "phase": {
    "number": 5,
    "name": "Authentication",
    "status": "in_progress"
  },
  "loop": {
    "plan": null,
    "position": "IDLE"
  },
  "timestamps": {
    "created_at": "2026-02-01T09:00:00-06:00",
    "updated_at": "2026-03-15T16:30:00-05:00"
  },
  "satellite": {
    "groom": true
  }
}
```

---

## Design Notes

- **Machine-readable only.** paul.json is for external systems, not humans. Humans read STATE.md.
- **Always tooling-generated.** Created by init, synced by workflows. Never hand-edited.
- **Backward compatible.** Workflows check for paul.json existence before syncing. Pre-v1.1 projects without paul.json are silently skipped.
- **Satellite config.** The `satellite` object is for consumer-side settings. BASE reads `satellite.groom` to decide whether to include this project in health checks.

---

*Template added: v1.1 Satellite Manifest*
