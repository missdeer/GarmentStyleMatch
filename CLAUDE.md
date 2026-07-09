# CLAUDE.md — 6-rule

These rules apply to every task in this project unless explicitly overridden.
Bias: caution over speed on non-trivial work. Use judgment on trivial tasks.
Each rule ends with a ❌/✅ pair — match the pattern, not the slogan.

## Rule 0 — Modern CLI only (enforced by PreToolUse hook)
Mappings: `find`→`fd`, `grep`→`rg`, `cat`→`bat` (or Read), `ls`→`eza`, `diff`→`delta`, JSON parsing → `jq` (never `python -c "import json"`).
The hook `.claude/hooks/legacy-cli-pretool.sh` will **deny** any Bash call whose first segment-token is a legacy tool and tell you the replacement — reissue with the modern equivalent, don't retry the same command. `git grep` / `git diff` are fine.
- ❌ `find . -name "*.go" | xargs grep TODO`
- ✅ `fd -e go -x rg TODO`

## Rule 1 — Think, ask, surface conflicts
State assumptions before coding. If two interpretations are both plausible, present them and ask — don't pick silently. If two patterns in the codebase contradict, pick one (more recent / more tested), say why, flag the other for cleanup; never blend them. Use the model only for judgment work (classification, drafting, summarization, extraction); for routing / retries / deterministic transforms, write code — don't ask the model.
- ❌ Picking interpretation A and producing 200 lines of code for it; or writing a retry loop by prompting the model.
- ✅ "I see two readings: A or B. Going with A because X — confirm if you meant B." / Retries live in a `for` loop with explicit backoff.

## Rule 2 — Minimal, surgical, conformant changes
Smallest diff that solves the stated problem. No speculative features, no abstractions for single-use code, no "improvements" to adjacent code / comments / formatting. Match the codebase's existing style even if you disagree — if you genuinely think a convention is harmful, surface it; don't fork silently. Senior-engineer test: would they call this overcomplicated or out-of-scope? If yes, simplify.
- ❌ Bug fix that also renames variables in nearby functions "while we're here", or introduces a `Strategy` interface for one caller.
- ✅ Smallest diff that fixes the bug; new abstraction only when ≥2 real call sites exist.

## Rule 3 — Read before you write
Before adding code: read the relevant exports, immediate callers, shared utilities in `libs/`. "Looks orthogonal" is dangerous — structure usually exists for a reason. Confirm a new helper has a real call site before committing it; `unusedfunc` / `unusedparams` are blocking findings, not advisories.
- ❌ Writing `parseDate()` helper and trusting nothing similar exists.
- ✅ `rg -i 'parseDate|ParseDate' libs/ tools/` first, then either reuse or add.

## Rule 4 — Goal-driven loop
Define success criteria up front, then iterate until verified. Don't follow a fixed step list — strong criteria let you self-correct. For feature work, "compiles" and "`go vet` clean" are not "feature works" — exercise the actual behavior and cite the evidence (command run, output observed).
- ❌ "Done — `go build ./...` passes."
- ✅ "Criteria: import R41 into `dewu-burgeon-sales-daily` for week N. Ran `./bin/...`; row count matches source xlsx (1,234); spot-checked 3 rows against `usage.md` query."

## Rule 5 — Report honestly: checkpoint, fail loud, tests verify intent
**Checkpoint** after each significant step — what's done, what's verified, what's left; if you lose track, stop and restate. **Fail loud** — "completed" is wrong if anything was skipped silently; "tests pass" is wrong if any were skipped or marked `t.Skip`; surface uncertainty, don't hide it. **Tests encode intent**, not just behavior — a test that can't fail when the business rule changes is broken; assert *why* the value matters (the rule), not just *what* it is right now.
- ❌ "All 3 subtasks done!" when subtask 2 silently fell through to a default, or a test that just re-encodes the current return value with no link to the business rule.
- ✅ "2 of 3 done. Subtask 2 hit Y — need your call on Z before continuing." / `assert sale_price == cost * (1 + REQUIRED_MARGIN)` instead of `assert sale_price == 13.75`.

# Project Overview

Garment style matching tool.

## Build

Qt 6.10+ (Core, Gui, Qml, Quick, QuickControls2, Widgets) is required. 

```bash
# Configure (root CMakeLists.txt drives the entire build via qt_add_executable)
cmake -S . -B cmake-msvc-build -DCMAKE_PREFIX_PATH="<path-to-Qt>/lib/cmake"

# Build
cmake --build cmake-msvc-build --config Release

# Run
./cmake-msvc-build/bin/GarmentStyleMatch.exe

# Install (bundles Qt QML/plugins via qt_generate_deploy_qml_app_script)
cmake --install cmake-msvc-build --prefix install
```

`cmake-msvc-build/` is the working build directory on this machine — reuse it, do not create a new `build/`.

## qt.conf trick (development runtime)

`src/CMakeLists.txt` writes `qt.conf` next to the exe with `Prefix = <detected Qt install>`. This is intentional and works around a `windeployqt 6.11` bug where `qt_add_qml_module` projects miss the top-level `QtQuick.Controls` `qmldir`. When editing that CMake block, keep the qt.conf output — do not replace with `windeployqt`.

# C++ Coding Standards (tools/crawler-webengine only)

See [C++ Core Guidelines](CppCoreGuideline.md)

## Automated checks

After save / Edit, a `PostToolUse` hook runs two static analyzers against `.cpp/.cc/.cxx` files under `tools/crawler-webengine/`:

1. `clang-tidy --quiet -p tools/crawler-webengine/cmake-build` — general C++ Core Guidelines / modernize / readability / performance checks; ruleset is defined in the repo-root `.clang-tidy`.
2. `clazy-standalone --only-qt -p tools/crawler-webengine/cmake-build` — Qt-framework-specific checks (default level1), covering Qt-specific anti-patterns that `clang-tidy` cannot detect: implicit-shared / container detach, `Q_PROPERTY`, `emit`, signal-slot connections, etc.

Diagnostics from both tools are injected back into the conversation context for Claude to fix. At session end, a `Stop` hook re-runs `clang-tidy` and `clazy-standalone` over every uncommitted C++ file; if any warnings or errors remain, it blocks termination and requires further fixes.

Common `clazy` warnings and their typical Qt fixes:

- `range-loop-detach`: `for (auto &x : container.method())` directly over a return value triggers an implicit-shared detach. Store the return value in `const auto local = ...;` first, then iterate over `local`.
- `qstring-arg`: chained `.arg()` calls when concatenating strings should use the single `arg(a, b, c)` overload to avoid a detach per call.
- `connect-by-name` / `old-style-connect`: use the function-pointer form of `connect`, not the `SIGNAL` / `SLOT` macros.

If a rule genuinely should not apply, adjust `.clang-tidy` (for `clang-tidy`) or pass `--checks=level1,no-XXX` via `clazy-postedit.sh` / `clazy-stop.sh` (for `clazy`). Do **not** add per-file `// NOLINT` or `// clazy:exclude=...` annotations.

# Architecture

Qt Quick app: **C++ controllers + list models exposed to QML via `QQmlContext` context properties.** No QML-side business logic.

```
src/main.cpp
  ├─ constructs 3 QAbstractListModel subclasses + 1 controller on the stack
  ├─ wires them via MatchController::set*Model(...)
  ├─ exposes all four as context properties: controller, candidateModel, galleryModel, photoModel
  └─ engine.loadFromModule("GarmentStyleMatch", "Main")   ← qt_add_qml_module URI
```

## Data flow

- `PhotoListModel`   — 输入实拍图列表 (scanned from `photoDir`)
- `CandidateListModel` — 候选款号 (produced by matching / scanned from `outputDir`)
- `GalleryListModel`  — 右侧小图库 (loaded from a PPT via `pptPath`)
- `MatchController`   — the single hub: owns current selection indices (`currentIndex`, `currentPhotoIndex`, `currentImagePage`), preview source (`PreviewPhoto` vs `PreviewOutput`), filter/search state, and every user-invokable slot (`scanPhotoDir`, `scanOutputDir`, `reloadPpt`, `confirmSelectedThumb`, `confirmStyleId`, `generateFineTuneModel`, image nav, etc.). Emits `logMessage(QString)` for UI-level status output.

New C++ features almost always belong on `MatchController` as a `Q_PROPERTY` + slot pair, then bound from QML. New model rows/roles go on the corresponding `*ListModel`.

## QML layout (src/qml/)

`Main.qml` is a `HeaderBar` + 3-column `RowLayout`:

- Left: `CandidatePanel` (candidate style numbers + photo list + `PathPickerRow`s for photo/output dirs)
- Middle: `MainImageView` (current preview + paging + open-original) over `ConfirmBar` (confirm-thumb / confirm-styleId / prev-item / generate-fine-tune-model)
- Right: `GalleryPanel` (category dropdown + search + 2-col grid + PPT path picker)

All state binds through `controller.*` properties; QML signals dispatch back through named controller slots. Adding a new panel = new QML file under `src/qml/components/` + append to `GSM_QML_FILES` in `src/CMakeLists.txt`.

# Adding C++ source files

Every new `.cpp`/`.h` must be added to `GSM_SOURCES` / `GSM_HEADERS` in `src/CMakeLists.txt` — there is no glob. Every new QML file must be added to `GSM_QML_FILES` (the `qt_add_qml_module` call), otherwise it will not be part of the QML module and `loadFromModule` cannot see it.

# 3rdparty / install / tool / doc

Currently empty placeholders (`.gitkeep`). If you add third-party CMake here, the root `CMakeLists.txt` will `add_subdirectory(3rdparty)` automatically when it detects `3rdparty/CMakeLists.txt` — no root edits needed to opt in.
