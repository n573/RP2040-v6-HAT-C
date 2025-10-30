# Help

# **How to push updates downstream**
Step 1 — Finalize your change in n573/io6Library
```
cd /path/to/io6Library
git status
git add -A
git commit -m "Your change description"
git push origin main-patched
# Optional: tag and push
# git tag -a vYYYY.MM.DD-io6 -m "Describe the change"
# git push origin vYYYY.MM.DD-io6
```
Step 2 — Bump the pointer in n573/RP2040-v6-HAT-C -- Note: this method tracks the branch tip
```
cd /path/to/RP2040-v6-HAT-C
git switch main-patched
# Ensure submodule config is synced
git submodule sync -- libraries/io6Library
# Advance to your fork’s latest main-patched and stage the new gitlink
git submodule update --init --remote -- libraries/io6Library
# Verify it moved
git -C libraries/io6Library rev-parse --short HEAD
git add libraries/io6Library
git commit -m "Bump libraries/io6Library to latest n573/main-patched"
git push origin main-patched
```
Step 3 — Bump RP2040-v6-HAT-C in n573/B-Series_Template
```
cd /path/to/B-Series_Template
git switch -c chore/bump-rp2040-hat-c || git switch chore/bump-rp2040-hat-c
# Ensure config is synced and then update to latest main-patched
git submodule sync -- external/RP2040-v6-HAT-C
git -C external/RP2040-v6-HAT-C fetch origin --prune
git -C external/RP2040-v6-HAT-C checkout -B main-patched origin/main-patched

# Pull nested submodules (io6Library, etc.)
git -C external/RP2040-v6-HAT-C submodule sync --recursive
git -C external/RP2040-v6-HAT-C submodule update --init --recursive

# Commit and push the new gitlink
git add external/RP2040-v6-HAT-C
git commit -m "Update RP2040-v6-HAT-C to latest main-patched (includes io6Library changes)"
git push -u origin chore/bump-rp2040-hat-c
# Open a PR and merge after CI passes
```
Safety checklist

*Never force-push the branches referenced by superprojects (main-patched).
*Prefer PRs for both bumps (RP2040-v6-HAT-C and B-Series_Template) so CI verifies everything.
*If you need strict reproducibility, cut a tag in n573/io6Library and pin RP2040-v6-HAT-C to that tag; then bump B-Series_Template.
*If you see “not our ref” errors again, it means a submodule pointer references a commit that isn’t present remotely; update the parent to a valid commit/tag and commit the new pointer.



**If io6Library submodule update fails/breaks**
```
SUB=libraries/io6Library
NEW_URL=https://github.com/n573/io6Library.git

# Update .gitmodules and track branch
git submodule set-url "$SUB" "$NEW_URL"
git submodule set-branch --branch main-patched "$SUB"
git add .gitmodules
git commit -m "io6Library: switch to n573 repo and track main-patched"
git submodule sync --recursive

# Update the submodule worktree
git -C "$SUB" remote set-url origin "$NEW_URL"
git -C "$SUB" fetch --prune --tags origin
git -C "$SUB" fetch --unshallow || true
git -C "$SUB" checkout -B main-patched origin/main-patched
git -C "$SUB" branch --set-upstream-to=origin/main-patched main-patched

# Verify tip (expect 71de07e8ea0b95bbba1381274014bc8cfbd2701a)
git -C "$SUB" rev-parse HEAD
git -C "$SUB" ls-remote origin main-patched | sed 's/\s.*//'

# Record the updated submodule commit in the superproject
git add "$SUB"
git commit -m "Update io6Library submodule to latest main-patched"
git push
```