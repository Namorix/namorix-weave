---
name: git-commit-suggester
description: |
  Scan git working tree changes and suggest structured commits following the project's
  commit conventions. Groups files into logical commits, proposes commit messages,
  and outputs ready-to-run bash commands.
---

# Git Commit Suggester

Analyze uncommitted changes and propose structured commits.

## Workflow

### 1. Scan the current state

Run these commands in parallel:
```bash
git status
git diff --stat
git ls-files --others --exclude-standard
git log --oneline -5
```

If no changed files found (no modified, no untracked) → output "Nothing to commit" and stop.

### 2. Analyze each changed file

For each modified/new/deleted/renamed file, categorize by scope:

| Scope | Matches |
|-------|---------|
| `core` | `packages/core/` |
| `ui` | `packages/ui/` |
| `shared` | `packages/shared/`, `packages/shared/src/` |
| `styles` | `packages/styles/` |
| `frontend` | `frontend/` |
| `backend` | `backend/` |
| `docs` | `docs/`, `*.md` |
| `chore` | `package.json`, config files, `.gitignore`, license files |

Memory bank files (`.claude/memory/`) — scope: `docs`.

Skip always:
- `.idea/`
- `node_modules/`
- `dist/`, `*.lock`, `*.log`
- Symlinks inside `.claude/skills/`

### 3. Group into logical commits

Group files that change for the same reason:
- Feature work: `feat(scope): description`
- Bug fixes: `fix(scope): description`
- Documentation: `docs: description`
- Config/chores: `chore: description`
- Breaking changes: `feat(scope)!: description` or `fix(scope)!: description`

**Rule:** Files that are changed for the same purpose go together. Split unrelated changes into separate commits.

**Cross-package rule:** If a feature spans multiple packages (e.g. `@namorix/core` + `frontend`), split into separate commits per package — one commit per scope.

### 4. Verify file existence before generating git add

Before outputting any `git add` command, verify every file path exists on disk:

```bash
# For each path in the proposed git add list:
test -f "path/to/file" || test -d "path/to/file"
```

Skip files that:
- Are staged as **deleted** (D in `git status --short`) — they no longer exist
- Are staged as **renamed** from (the source path) — only add the destination path
- Are empty directories — `git add` ignores them

**Rule:** Every path in `git add` must pass `test -f` or `test -d`. No exceptions.
If unsure, run `ls` on the parent directory to confirm.

### 5. Draft commit messages

Format: `{type}({scope}): {description}`

Types: `feat`, `fix`, `refactor`, `docs`, `chore`

- Use `!` suffix for breaking changes: `feat(core)!: rename auth hook`
- The description should explain WHY, not just WHAT
- Check `git log --oneline -5` to avoid duplicate messages

### 6. Output format

Present as numbered commits with file lists and ready-to-run bash:

```bash
# ============================================================
# Commit 1 — type(scope): short description
# ============================================================

git add \
  path/to/file1 \
  path/to/file2

git commit -m "$(cat <<'EOF'
type(scope): description

- Bullet point of what changed
- Another bullet point
EOF
)"

# ============================================================
# Commit 2 — type(scope): short description
# ============================================================
...
```

## Important Rules

1. **Never stage `.idea/`** — IDE config stays in `.gitignore`
2. **Never stage symlinks** inside `.claude/skills/` — they point to global config
3. **Use HEREDOC syntax** for multi-line commit messages
4. **Check recent commits** (`git log --oneline -5`) to avoid duplicate messages
5. **If files are already staged**, include them and explain they're staged from previous work
6. **Separate untracked new files** from modified files — make it clear what's new vs changed
7. **Do NOT actually commit** — only output the commands. The user will run them.
8. **Never add `Co-Authored-By`** trailer — project convention không dùng.
9. **No changes = stop early** — if `git status` shows clean working tree, say so and exit.
10. **Breaking changes** — if a commit removes/renames a public API, use `!` suffix and explain impact in bullet points.
11. **Never skip modified files** — every file in `git diff --stat` and `git ls-files --others --exclude-standard` output must appear in exactly one commit. No exceptions.
12. **Verify file existence before git add** — before outputting any `git add` command, check `test -f <path>` or `test -d <path>` for every path. Skip files đã staged delete (D) hoặc là source path của staged rename (R) — những file này không còn trên disk.
13. **Check OS before outputting commands** — xác định `process.platform` (win32/darwin/linux).
    - **win32**: user dùng Windows Terminal với PowerShell. Dùng backtick `` ` `` cho line continuation, here-string `@'...'@` cho commit message.
    - **darwin/linux**: dùng `\` line continuation, `cat <<'EOF'` heredoc.
    - Output lệnh đúng với terminal của user.
14. **NEVER execute git commands** — chỉ output ready-to-run commands. Không tự động chạy `git add` hay `git commit`.
