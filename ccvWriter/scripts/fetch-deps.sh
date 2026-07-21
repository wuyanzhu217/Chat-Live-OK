#!/usr/bin/env bash
# Clone libdatachannel (+ recursive deps) into third_party/
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="$ROOT/third_party/libdatachannel"
REPO="${LIBDATACHANNEL_REPO:-https://github.com/paullouisageneau/libdatachannel.git}"

mkdir -p "$ROOT/third_party"
if [[ -d "$DEST/.git" ]]; then
  echo "Updating existing checkout: $DEST"
  git -C "$DEST" pull --ff-only || true
  git -C "$DEST" submodule update --init --recursive
else
  echo "Cloning $REPO -> $DEST"
  git clone --recursive "$REPO" "$DEST"
fi
echo "OK: $DEST"
