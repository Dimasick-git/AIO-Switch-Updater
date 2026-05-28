#!/usr/bin/env bash
# Pack the built Ryazhenka Updater .nro into a release zip. The zip mirrors the
# original AIO layout so the in-app self-updater (NRO_PATH constant unchanged
# at /switch/aio-switch-updater/aio-switch-updater.nro) keeps working: the
# zip contains switch/aio-switch-updater/aio-switch-updater.nro renamed from
# the build artifact ryazhenka-updater.nro.

set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_root"

SRC_NRO="ryazhenka-updater.nro"
RELEASE_ZIP="Ryazhenka_AIO.zip"
STAGING="switch/aio-switch-updater"

if [[ ! -f "$SRC_NRO" ]]; then
    echo "error: $SRC_NRO not found — run \`make\` first" >&2
    exit 1
fi

rm -rf switch "$RELEASE_ZIP"
mkdir -p "$STAGING"

# Keep the original on-card filename so existing installs are overwritten
# in place by the in-app updater.
cp "$SRC_NRO" "$STAGING/aio-switch-updater.nro"

zip -FSr "$RELEASE_ZIP" switch/
echo "packed $RELEASE_ZIP ($(du -h "$RELEASE_ZIP" | cut -f1))"
