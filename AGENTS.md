# AGENTS.md — multi-agent coordination for this repo

Two AI agents work on this repository **in parallel**. Read this file at session
start and follow it. It exists to prevent merge conflicts and duplicated work.

## Agents and working trees

| Agent | Working tree (do not touch the other's tree) | Branch prefix |
|---|---|---|
| **Codex** (OpenAI) | `C:\Users\Tanner\Documents\Codex\work\vita2hos` | `fix/*`, commits directly |
| **ZCode** (Claude/GLM) | `C:\Users\Tanner\Documents\ZCode\work\vita2hos` | `zcode/*` |

- Remotes: `origin` = https://github.com/tanner02/vita2hos.git (shared),
  `codex-local` / `zcode-local` = the other agent's tree (fetch only, never push).
- **Never edit files inside the other agent's tree.** Patches move via git
  (push to `origin`, or `git format-patch` bundles).
- **Never edit the other agent's dirty files.** Before starting work, check the
  other tree's `git status --porcelain` and recent commits; treat any file it
  touched in its last ~10 commits or holds dirty as **owned** by it.

## Working agreement (updated 2026-11-27, per Tanner)

- **ZCode is offline-only**: static analysis, HLE implementation, host tests,
  audits and documentation. ZCode does NOT drive the Switch, install, launch,
  flash or pull logs. All hardware verification/testing is done by Codex (or
  Tanner), then logged in AGENT-PROGRESS.md so ZCode can decode results.
- **Codex owns**: hardware debugging loop (Switch via MTP/Atmosphere/
  sys-autopilot), `SceKernelThreadMgr.c`, `SceDisplay.c`, `SceCtrl.c`,
  `SceGxm.c` + `gxm_*.c`, shader recompiler glue, `load*.c` (ZCode additive
  edits allowed, flagged in AGENT-PROGRESS.md), `main.c`,
  `missing_import*.c`, thread/display/trophy/NET work, `tests/host/*`,
  savedata/AppUtil core.
- **ZCode owns**: NEW module files (`source/modules/Sce*.c` not previously
  existing), `source/load_unbound_stubs.c`, NID/import DB work,
  `AGENTS.md` itself, `ZCode/scripts/` + `ZCode/outputs/`.
- **ZCode additive edits in Codex files are allowed** when strictly additive
  (new EXPORT lines, one-line call sites, test-suite source lines); each must
  be flagged in AGENT-PROGRESS.md and rebased before pushing.
- **Shared, coordinate before touching**: `CMakeLists.txt` (additive source
  lines only), `source/modules/SceLibKernel.c`, `SceSysmem.c`.
- **Verification flow**: ZCode commits offline work + artifacts + expected
  log markers into `Documents/ZCode/outputs/ZCODE-VERIFICATION.md` and
  AGENT-PROGRESS.md → Codex verifies on hardware → Codex logs results →
  ZCode decodes and iterates offline.

## Serial resources

- **The Switch console** is a serial resource: only codex drives hardware.
  ZCode prepares builds/probes; codex flashes and tests.
- **The devkitPro rootfs** (`WSL Ubuntu-26.04`, chroot
  `/work/vita2hos-build/rootfs`) is shared read-only infrastructure:
  - Codex builds in `rootfs/work/build-debug`, ZCode in `rootfs/work/zcode-build`
    (`/work/zcode-build` inside the chroot). Never rebuild into the other's dir.

## Build recipe (ZCode)

```sh
MSYS_NO_PATHCONV=1 wsl -d Ubuntu-26.04 -- sh \
  /mnt/c/Users/Tanner/Documents/ZCode/scripts/build-vita2hos.sh
# -> rootfs/work/zcode-build/vita2hos.nsp  (chroot /work/zcode-build)
```

## Merge cadence

1. Small commits on your lane branch; push to `origin` often.
2. Rebase on the other agent's latest merged work before starting a new task.
3. Keep lane branches alive < 1 day; land or hand back.
4. On conflict in shared files, the *other* file owner wins; re-apply yours.
