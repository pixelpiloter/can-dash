# Contributing to can-dash

> **License: AGPL-3.0 + 商业化限制** — 详见 [LICENSE](LICENSE)。
> 贡献的代码默认以 AGPL-3.0 发布;任何包含本项目的商业化使用需获书面授权。

Thanks for keeping the cluster safe and the build clean. This file covers
the rules — the architecture lives in [ARCHITECTURE.md](ARCHITECTURE.md).

## Ground rules (1 minute)

1. **One logical change per commit.** A commit that fixes three unrelated
   things will be asked to be split. This is what makes `git bisect`
   actually useful.
2. **No "fix:" band-aids.** If you find yourself writing a second
   `fix:` in a row, the right move is `git rebase -i HEAD~N` to squash
   or, better, a `refactor:` commit that addresses the root cause. The
   pre-commit hook will block three consecutive `fix:` commits.
3. **Tests for behavior changes.** Layer 2 (pure C++ logic) and
   Layer 1 (shm helpers) must have unit tests. UI changes need a
   manual-test note in the PR description.
4. **Don't touch generated files in `src/generated/`.** They are produced
   by `tools/yaml_to_c.py` and `tools/qml_generator.py`. Edit the YAML
   in `config/` and re-run codegen.
5. **Hardcoded paths are banned.** Use `std::filesystem::path` joined
   from `CMAKE_SOURCE_DIR` (C++) or `Path(__file__).parent` (Python).
   No more `/home/<user>/...` literals.

## Commit message format (enforced by hook)

```
<type>(<scope>): <subject>

<body, wrap at 72 cols>

<footer: Refs: REQ-XXX-NNN | Breaking-Change: ...>
```

| type       | when                                                        |
|------------|-------------------------------------------------------------|
| feat       | new user-visible behavior                                   |
| fix        | bug fix (use sparingly — see ground rule 2)                |
| refactor   | code change with no behavior delta                          |
| perf       | measurable speed/memory improvement                         |
| test       | adding or fixing tests only                                 |
| docs       | markdown / comments / diagrams                              |
| build      | CMake / codegen / dependency changes                        |
| ci         | GitHub Actions / hooks / lint config                        |
| chore      | housekeeping (gitignore, file rename, no logic change)      |
| style      | formatting, no logic change                                 |

Subject: imperative mood, lowercase, no period, ≤ 72 chars.

Examples:
```
feat(shm): add CRC32 checksum to DisplayDataShm
fix(backend): emit displayHealth on heartbeat timeout
refactor(runtime): extract alarm policy to pure function
test(shm): cover magic + version + CRC fault injection
ci: split ut-only job from full build job
```

## Pull request flow

1. Branch from `main`. Name: `feat/<short-topic>` or `fix/<short-topic>`.
2. Run the test suite locally:
   ```bash
   ./build.sh ut           # builds + runs unit tests (no Qt needed)
   ```
3. Push the branch. Open a PR against `main`. CI runs `validate-yaml`
   on every push; `build` runs on `main` only.
4. Self-review the diff before requesting review. Look for:
   - debug `printf` / `std::cerr` left in
   - commented-out code
   - `/home/<user>/` paths
   - features that should be config (YAML) not code

## Pre-commit hooks (recommended)

```bash
python3 tools/install-hooks.sh        # native git hooks
pre-commit install                    # optional, adds cppcheck + clang-format
```

The hook will refuse a commit that:
- lacks a conventional `type: subject` prefix
- is the 3rd consecutive `fix:`

## Where things live

```
ARCHITECTURE.md            4-layer model, data flow, extension points
docs/requirements/         REQ-XXX-NNN specs (start here for new features)
docs/MULTI_AGENT_*.md      how AI agents collaborate on this repo
schemas/                   JSON schemas for config/*.yaml
src/layer1/                platform shims (shm, can, log)
src/layer2/                pure C++ business logic (unit-testable)
src/layer3/                Qt backend (UI surface, no business logic)
src/ui/                    QML pages + components
tools/                     codegen, validate, lint scripts
tests/                     Layer 1 / Layer 2 unit tests
references/                deep-dive docs (shared memory, codegen, etc.)
```

## Filing a bug

Include: hardware (CPU/Qt version), `git rev-parse HEAD`, build command,
repro steps, and expected vs actual. A `git bisect` friendly minimal
repro is worth 10x the bug report.
