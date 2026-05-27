#!/usr/bin/env bash
# Apply (or verify) the small set of patches that adapt upstream files to the
# Ryazhenka fork. Designed to be idempotent and safe to invoke from CI on every
# sync iteration. Strategy: try `git apply --check`; if that fails, try the
# reverse direction — a successful reverse means the patch is already applied
# so we skip silently. Any other failure stops the script with non-zero exit.
#
# Usage:
#   apply-patches.sh           apply all patches (in lexical order)
#   apply-patches.sh --check   only verify; do not modify the working tree
#   apply-patches.sh --reset   reverse all currently applied patches (debug)

set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/../.." && pwd)"
patches_dir="$script_dir/patches"

mode="apply"
case "${1:-}" in
    --check) mode="check" ;;
    --reset) mode="reset" ;;
    "") ;;
    *) echo "unknown flag: $1" >&2; exit 2 ;;
esac

shopt -s nullglob
patches=("$patches_dir"/*.patch)
shopt -u nullglob

if [[ ${#patches[@]} -eq 0 ]]; then
    echo "no patches in $patches_dir"
    exit 0
fi

cd "$repo_root"

# Sort by filename so 01-, 02-, … always run in order.
IFS=$'\n' sorted=($(printf '%s\n' "${patches[@]}" | sort))
unset IFS

if [[ "$mode" == "reset" ]]; then
    # Reverse in opposite order so later patches are peeled first.
    for ((i = ${#sorted[@]} - 1; i >= 0; i--)); do
        p="${sorted[$i]}"
        name="$(basename "$p")"
        if git apply -R --check "$p" 2>/dev/null; then
            git apply -R "$p"
            echo "[$name] reversed"
        else
            echo "[$name] not applied — skipped"
        fi
    done
    exit 0
fi

for p in "${sorted[@]}"; do
    name="$(basename "$p")"
    if git apply --check "$p" 2>/dev/null; then
        if [[ "$mode" == "check" ]]; then
            echo "[$name] applicable"
        else
            git apply "$p"
            echo "[$name] applied"
        fi
    elif git apply -R --check "$p" 2>/dev/null; then
        echo "[$name] already applied"
    else
        echo "[$name] CONFLICT — manual resolution required" >&2
        echo "  hint: run \`git apply --3way $p\` to attempt a three-way merge" >&2
        exit 1
    fi
done
