# Working agreement

Tanner transferred all source, HLE and hardware work to Codex on 2026-11-28.
ZCode is no longer part of the workflow. Older lane restrictions are superseded.

- Work in this checkout; do not modify the former ZCode checkout.
- Read and update `C:/Users/Tanner/Documents/Codex/work/AGENT-PROGRESS.md`.
  Claim source edits and console ownership before starting them. Preserve history.
- Hardware operations are exclusive. Preserve payloads and reports, verify
  uploads by readback, observe title closure before replacing active files,
  and detach injected controllers after input.
- Keep normal hbmenu and unrelated titles available. Use the dedicated vita2hos
  title for testing; do not introduce commercial-title overrides.
- Use bounded local delegation for focused summaries, patch drafts and tests;
  Codex owns architecture, debugging, integration and final review.
- Use the existing devkitPro rootfs as infrastructure, building in Codex-owned
  directories. Do not rebuild in the former ZCode build directory.
- Run focused host/sanitizer checks and native builds before hardware changes.
  Record measured results separately from inference and unverified behavior.
- Push reviewed changes to the origin fork. Do not push to upstream.
