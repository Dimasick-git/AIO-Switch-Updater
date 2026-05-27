#!/usr/bin/env bash
# Daily upstream sync. Run from CI (.github/workflows/sync-upstream.yml). Pulls
# HamletDuFromage/aio-switch-updater@master into our main, re-applies the
# Ryazhenka patches, and aborts cleanly when conflicts appear so a human can
# resolve them.
#
# Exit codes:
#   0   nothing to do or sync succeeded; safe to push
#   10  merge conflict from upstream — manual resolution required
#   11  patches failed to re-apply after a clean upstream merge
#   1   unexpected error

set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/../.." && pwd)"
cd "$repo_root"

UPSTREAM_REMOTE="${UPSTREAM_REMOTE:-upstream}"
UPSTREAM_BRANCH="${UPSTREAM_BRANCH:-master}"
UPSTREAM_URL="${UPSTREAM_URL:-https://github.com/HamletDuFromage/aio-switch-updater.git}"

if ! git remote get-url "$UPSTREAM_REMOTE" >/dev/null 2>&1; then
    git remote add "$UPSTREAM_REMOTE" "$UPSTREAM_URL"
fi

git fetch "$UPSTREAM_REMOTE" --tags --prune

upstream_head="$(git rev-parse "$UPSTREAM_REMOTE/$UPSTREAM_BRANCH")"
local_base="$(cat .upstream-base 2>/dev/null || echo "")"

if [[ "$upstream_head" == "$local_base" ]]; then
    echo "upstream unchanged at $upstream_head"
    exit 0
fi

echo "merging $UPSTREAM_REMOTE/$UPSTREAM_BRANCH ($upstream_head)"

if ! git merge --no-ff --no-edit -m "chore(sync): merge upstream@${upstream_head:0:7}" "$UPSTREAM_REMOTE/$UPSTREAM_BRANCH"; then
    git merge --abort || true
    echo "merge conflict" >&2
    exit 10
fi

if ! bash "$script_dir/apply-patches.sh"; then
    echo "patches failed to re-apply" >&2
    exit 11
fi

echo "$upstream_head" > .upstream-base
git add .upstream-base

if git diff --cached --quiet; then
    echo "no patch drift — base already current"
else
    git commit -m "chore(sync): record upstream base ${upstream_head:0:7}"
fi

echo "sync OK at $upstream_head"
