---
name: update-docs-and-versions
description: |
  After completing a feature or fix, update project documentation (memory bank,
  progress.md) and bump package versions. Scans git first to only touch files
  affected by recent changes.
---

# Update Documentation & Package Versions

After a coding session, update memory bank docs and bump package versions following
the project's version rules. Always scan git first — never read unchanged files.

## Workflow

### Step 1: Scan Git Changes

```bash
git status --short          # staged + unstaged + untracked
git diff --stat             # unstaged changes
git diff --cached --stat    # staged changes
git log --oneline -5        # recent commits for context
```

If working tree is clean and no recent commits → output "Nothing to update" and stop.

### Step 2: Categorize Changed Files

Map each changed file to a package and determine impact:

| Changed Files | Package | Impact |
|--------------|---------|--------|
| `packages/core/src/` | @namorix/core | Check if new module, utility, or bug fix |
| `packages/shared/src/` | @namorix/shared | Check if new type, constant, or error code |
| `packages/ui/src/` | @namorix/ui | Check if new component, or fix |
| `packages/styles/src/` | @namorix/styles | Check if new token, variable, or fix |
| `frontend/src/` | frontend | Check if new page, route, or bug fix |
| `frontend/src/addons/*/` | addon (internal) | Check if addon changed → bump addon version trong `packages/core/src/version.ts` (`NmxAddonVersions`) |
| `backend/` | backend | Check if new endpoint, service, or bug fix (C#) |
| Root config files | root (namorix) | Check if workspace scripts, tooling changed |

### Step 3: Determine Version Bumps

Use the bump triggers from CLAUDE.md Meta Rules and `progress.md`:

| Package | PATCH bump | MINOR bump | MAJOR bump |
|---------|-----------|------------|------------|
| root (namorix) | Bug fixes, config tweaks, dependency updates | New feature, new package, milestone completion, workspace structure change | Breaking workspace change, dropped package |
| frontend | Bug fixes, CSS tweaks | New pages, routing changes, auth flow, i18n | Breaking UX overhaul |
| @namorix/core | Bug fixes | New utility, new type, new module (i18n, validation) | Removed/renamed public API |
| @namorix/styles | Token fixes | New token, new variable, new export | Renamed/removed existing tokens |
| @namorix/ui | Bug fixes | New component | Removed component, breaking prop change |
| @namorix/shared | Bug fixes | New type, new constant, new error code, new helper | Removed/renamed exported type or constant |
| backend | Bug fixes | New API endpoint, auth feature, refactor to decorators | Breaking API contract change |

**Addon internal — bump trong `packages/core/src/version.ts` (`NmxAddonVersions`):**

| Addon change | Bump |
|--------------|------|
| Bug fix / tweak nhỏ | PATCH (vd `1.1.0` → `1.1.1`) |
| Feature mới / phase mới | MINOR (vd `1.1.0` → `1.2.0`) |
| Scaffold → hoàn thiện / breaking addon | MAJOR (vd `0.1.0` → `1.0.0`) |

**Rule:** Only bump packages whose files actually changed. Don't bump unrelated packages.

**Monorepo dependency rule:** If `@namorix/core` (or any package) is bumped, check all
other packages and `frontend/package.json` that depend on it — update their version
reference to match the new version.

### Step 4: Read Only What's Needed

Read ONLY these files (skip if unchanged):
- `.claude/memory/progress.md` — always read (current versions + history)
- `.claude/memory/activeContext.md` — always read (current focus)
- `.claude/memory/systemPatterns.md` — only if architecture changed
- `.claude/memory/techContext.md` — only if tech/deps changed
- `.claude/memory/productContext.md` — only if UX changed
- `.claude/memory/projectbrief.md` — rarely changes
- `.claude/FLOW.md` — always read (comprehensive flow docs, update if architecture/flows changed)
- Package `.json` files — only for packages being bumped + their dependents
- `packages/core/src/version.ts` — nếu có addon internal thay đổi (bump version addon)
- **Cả 3 README.md — LUÔN đọc hết, đừng bỏ sót:**
  - `README.md` (root) — milestones, features, backend/frontend structure
  - `frontend/README.md` — frontend structure, addon list, milestones
  - `backend/README.md` — backend structure, API endpoints, models, SignalR events
- `docs/` markdown files — only if related to changed code

**Never read:**
- `LICENSE`
- Unchanged packages
- `node_modules/`
- `*.g.cs`, `*.Designer.cs`, EF migration auto-generated files

### Step 5: Update Documentation

#### progress.md — always update if versions changed
1. Update the "Current Version" table with new versions
2. Add a new entry in "Version History" under today's date

#### activeContext.md — update if needed
1. Update "Recent Changes" with summary of what was done
2. Update "Next Steps" if milestone progress changed

#### Other memory files — only if relevant
- `systemPatterns.md`: new patterns, architecture decisions
- `techContext.md`: new deps, new config, new key files
- `productContext.md`: UX changes

#### FLOW.md — update if related code changed
1. Check git diff for changes to:
   - Auth flow (backend auth, frontend controllers, guards)
   - SignalR events (new event, new notifier, new subscriber)
   - Appearance settings (new key, new validation, new UI)
   - API endpoints (new/modified controller endpoints)
   - Addon system (new contract, new mode)
   - New backend interfaces/services
   - New frontend hooks/components
2. Read relevant sections of FLOW.md that match changed code
3. Update sections accordingly — add new events to SignalR table, new keys to appearance table, new files to responsibility map

### Step 6: Update All Version References

For each bumped package, update the `"version"` field in its `package.json`.

Then **search the entire project** for other references to the old version string:

```bash
# Without quotes — catches all formats
grep -rn '0\.5\.0' --include='*.{md,json,ts,tsx,cs,csproj,yml,yaml}' . --exclude-dir=node_modules
```

Update all occurrences found in:
- `README.md` — version badges, tech stack table, quick start commands, package versions table
- `docs/` markdown files — version numbers in example code, config samples, compatibility notes
- `.claude/memory/techContext.md` — dependency version notes
- `frontend/` — API URL or version constants in source code
- Other `package.json` files that reference the bumped package as a dependency
- Any `.env.example` or config files mentioning version-specific values

**Addon internal:** nếu có file `frontend/src/addons/<name>/` thay đổi, bump version của addon đó trong
`frontend/packages/core/src/version.ts` (`NmxAddonVersions`) — đừng bỏ sót (không nằm trong package.json).

**Rule:** A version bump is not complete until all references across the project are
updated. Incomplete updates cause confusion and integration bugs.

### Step 7: Present Changes for Approval

Show a table of all planned changes:

```
| File | Change |
|------|--------|
| .claude/memory/progress.md | Update versions table + history |
| .claude/memory/activeContext.md | Add recent changes entry |
| README.md | Update version badge, package versions table |
| packages/core/package.json | 0.5.0 → 0.5.1 |
| frontend/package.json | @namorix/core: 0.5.0 → 0.5.1 |
| ... | ... |
```

Ask user to confirm the full plan before writing any file.
If user rejects specific files, skip those and apply the rest.

## Important Rules

1. **Scan git first** — never guess what changed
2. **Only read changed files + required doc files** — skip everything else
3. **Don't bump unrelated packages** — only packages whose code changed
4. **Version notation:** `MAJOR.MINOR.PATCH`, no leading `v`
5. **Ask before writing** — present the full plan, wait for approval; respect partial rejection
6. **Follow existing format** in progress.md — match the table style exactly
7. **Always update cả 3 README.md** — khi version bump, kiểm tra và cập nhật:
   - `README.md` (root) + `frontend/README.md` + `backend/README.md` — **không chỉ root**
   - Tech stack table (nếu backend/frontend stack thay đổi)
   - Package versions table (nếu có)
   - Quick start commands (nếu dev workflow thay đổi)
   - Directory structure (nếu workspace thay đổi)
8. **Search the whole project** — dùng `grep` không có quotes để tìm tất cả references
   đến version cũ. Update tất cả để tránh inconsistency.
9. **Update monorepo dependents** — khi bump một package, tìm tất cả package.json
   khác trong workspace reference đến nó và update version đó luôn.
10. **Early exit** — nếu không có changes, dừng ngay và thông báo.

