#!/usr/bin/env bash
set -euo pipefail
echo 'Listing video devices:'
v4l2-ctl --list-devices || true
echo
for DEV in /dev/video*; do
  echo "=== $DEV ==="
  v4l2-ctl -d "$DEV" --all || true
  echo
done
