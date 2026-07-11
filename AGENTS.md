# AGENTS.md — 6-rule

These rules apply to every task in this project unless explicitly overridden.
Bias: caution over speed on non-trivial work. Use judgment on trivial tasks.
Each rule ends with a ❌/✅ pair — match the pattern, not the slogan.

## Rule 0 — Modern CLI only
Mappings: `find`→`fd`, `grep`→`rg`, `cat`→`bat` (or Read), `ls`→`eza`, `diff`→`delta`, JSON parsing → `jq` (never `python -c "import json"`).
`git grep` / `git diff` are fine.
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
Before adding code: read the relevant declarations, immediate callers, controller/model implementation in `src/core/`, and reusable QML components in `src/qml/components/`. "Looks orthogonal" is dangerous — structure usually exists for a reason. Confirm a new helper has a real call site before committing it; unused functions and parameters are blocking findings, not advisories.
- ❌ Adding another clearable text field without checking the existing QML component.
- ✅ `rg -n "ClearableTextField" src/qml` first, then reuse or extend the existing component.

## Rule 4 — Goal-driven loop
Define success criteria up front, then iterate until verified. Don't follow a fixed step list — strong criteria let you self-correct. For feature work, "compiles" and "CTest passes" are not automatically "feature works" — exercise the relevant UI or data path and cite the evidence.
- ❌ "Done — the QML cache compiled."
- ✅ "Built the app, opened the affected tab, exercised the new control, and observed the expected controller/status update."

## Rule 5 — Report honestly: checkpoint, fail loud, tests verify intent
**Checkpoint** after each significant step — what's done, what's verified, what's left; if you lose track, stop and restate. **Fail loud** — "completed" is wrong if anything was skipped silently; "tests pass" is wrong if any were skipped or marked `t.Skip`; surface uncertainty, don't hide it. **Tests encode intent**, not just behavior — a test that can't fail when the business rule changes is broken; assert *why* the value matters (the rule), not just *what* it is right now.
- ❌ "All 3 subtasks done!" when subtask 2 silently fell through to a default, or a test that just re-encodes the current return value with no link to the business rule.
- ✅ "2 of 3 done. Subtask 2 hit Y — need your call on Z before continuing." / Test that a persisted PPT page selection is restored, not merely that a setter returns its input.

# Project Overview

GarmentStyleMatch is a Qt Quick desktop tool for browsing photographed garments, scanning classified output folders, extracting style images from PowerPoint files, and confirming garment style numbers.

## Build

The project uses C++20 and CMake 3.21+. Required dependencies are:

- Qt 6.10+: Concurrent, Core, Gui, Qml, Quick, QuickControls2, and Widgets
- Qt AxContainer on Windows, used for PowerPoint preview export
- LibArchive, supplied by the root `vcpkg.json` manifest

On the current Windows workstation, always reuse `cmake-msvc-build/` and use the login-shell wrappers:

```bash
# Reconfigure after changing CMakeLists.txt or build dependencies
bash -lc "cmake-reconfigure cmake-msvc-build"

# Build
bash -lc "cmake-build cmake-msvc-build"

# Run all five CTest executables
ctest --test-dir cmake-msvc-build --output-on-failure

# Run
./cmake-msvc-build/bin/GarmentStyleMatch.exe

# Install (bundles Qt QML/plugins via qt_generate_deploy_qml_app_script)
cmake --install cmake-msvc-build --prefix install
```

Do not create a second `build/` tree. A running `GarmentStyleMatch.exe` locks the output binary on Windows and causes linker error `LNK1168`; close it before rebuilding.

## qt.conf trick (development runtime)

`src/CMakeLists.txt` writes `qt.conf` next to the exe with `Prefix = <detected Qt install>`. This is intentional and works around a `windeployqt 6.11` bug where `qt_add_qml_module` projects miss the top-level `QtQuick.Controls` `qmldir`. When editing that CMake block, keep the qt.conf output — do not replace with `windeployqt`.

# C++ Coding Standards

Use C++20 and follow [C++ Core Guidelines](CppCoreGuideline.md). The repository also contains `.clang-format` and `.clang-tidy` configuration files.

## Automated checks

`.claude/settings.json` enables a modern-CLI `PreToolUse` hook for Claude Code. It also registers clang-format, clang-tidy, and clazy `PostToolUse` hooks, but those scripts currently filter for the obsolete, absent `tools/crawler-webengine/` subtree and use its `cmake-build` directory. They do **not** check files under `src/`; do not report that static analysis passed unless it was run explicitly against `cmake-msvc-build/compile_commands.json`.

The authoritative automated regression suite is CTest. It currently contains `PptStyleExtractorTest`, `GalleryListModelTest`, `PptPageListModelTest`, `ImageMetadataTest`, and `MatchControllerTest`.

# Architecture

Qt Quick app: **C++ controllers + list models exposed to QML via `QQmlContext` context properties.** No QML-side business logic.

```
src/main.cpp
  ├─ constructs 4 QAbstractListModel subclasses, ImageMetadata, and MatchController on the stack
  ├─ wires all four models into MatchController
  ├─ refreshes ImageMetadata when currentPhotoPath changes
  ├─ exposes 6 context properties to QML
  └─ engine.loadFromModule("GarmentStyleMatch", "Main")   ← qt_add_qml_module URI
```

## Data flow

- `PhotoListModel` — input photo list scanned from `photoDir`.
- `CandidateListModel` — classified output/style folders scanned from `outputDir`.
- `PptPageListModel` — PowerPoint page previews and persisted page selections.
- `GalleryListModel` — style-number image gallery extracted from selected PowerPoint pages.
- `ImageMetadata` — file, image, IPTC, and EXIF properties for the current input photo.
- `PptStyleExtractor` — reads the `.pptx` archive through LibArchive and extracts selected page/style assets; controller extraction runs through Qt Concurrent.
- `MatchController` — central state and action hub for selection indices, preview source, filters, paths, cached PPT data, extraction, navigation, confirmation, UI style, busy state, and `QSettings` persistence. It emits `logMessage(QString)` for the status bar.

New C++ features almost always belong on `MatchController` as a `Q_PROPERTY` + slot pair, then bound from QML. New model rows/roles go on the corresponding `*ListModel`.

## QML layout (src/qml/)

`Main.qml` is a `HeaderBar`, lazily loaded 3-column workspace, and bottom status bar:

- Left: `CandidatePanel` with input/output tabs, directory pickers, filters, the photo list, and the classified output list.
- Middle: `MainImageView`, an output-image thumbnail strip when applicable, and `ImagePropertiesPanel`. Its tabs are 操作 / 文件信息 / 图像信息 / IPTC / EXIF. The 操作 tab contains a vertical `ConfirmBar`: 自动匹配款号, a clearable 款号 field, and 确认款号.
- Right: `GalleryPanel` with PPT页面预览 and 款号小图库 tabs. Selected PPT pages are extracted in the background, then the gallery tab is selected.

Application state binds through `controller.*` and the context-property models. QML signals dispatch actions back through named controller slots; small presentation-only behavior remains local to QML. Adding a new panel requires a QML file under `src/qml/components/` and an entry in `GSM_QML_FILES` in `src/CMakeLists.txt`.

# Adding C++ source files

Every new `.cpp`/`.h` must be added to `GSM_SOURCES` / `GSM_HEADERS` in `src/CMakeLists.txt` — there is no glob. Every new QML file must be added to `GSM_QML_FILES` (the `qt_add_qml_module` call), otherwise it will not be part of the QML module and `loadFromModule` cannot see it.

# Generated and placeholder directories

- `3rdparty/` is currently a placeholder. The root CMake file automatically adds it only when `3rdparty/CMakeLists.txt` exists.
- `cmake-msvc-build/`, root `compile_commands.json`, and populated `install/` contents are generated artifacts; do not hand-edit them.
- `install/` currently contains only a local `.gitkeep` placeholder and is the default local packaging destination.
