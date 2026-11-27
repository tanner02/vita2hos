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

## Lane ownership (updated 2026-11-27)

- **Codex owns**: hardware debugging loop (Switch via MTP/Atmosphere),
  `SceKernelThreadMgr.c`, `SceDisplay.c`, `SceCtrl.c`, `SceGxm.c` +
  `gxm_*.c`, shader recompiler glue, `load*.c`, `main.c`, `missing_import*.c`,
  thread/display/trophy/NET work, `tests/host/*`.
- **ZCode owns**: `SceSysmodule.c`, NEW module files (new `Sce*.c` in
  `source/modules/`), NID/import DB work under `external/vita-headers`
  lookups, `AGENTS.md` itself, `scripts/` in the ZCode tree.
- **Shared, coordinate before touching**: `CMakeLists.txt` (new source files
  must be added — keep your diff minimal and rebase before pushing),
  `source/modules/SceLibKernel.c`, `SceSysmem.c`.

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
