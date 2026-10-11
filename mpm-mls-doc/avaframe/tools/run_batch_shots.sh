#!/bin/bash
# Runs the app in AvaFrame batch mode and captures its window whenever the batch asks for a
# screenshot ("screenshots": true in the jobs file). macOS only: screencapture plus a small Swift
# helper (winid.swift, compiled on first use) that finds the app's window by process id, so only
# the app's own window is captured, even when other windows cover it.
#
# usage (repository root): mpm-mls-doc/avaframe/tools/run_batch_shots.sh <jobs.json> <log>
set -e
TOOLS=$(cd "$(dirname "$0")" && pwd)
[ -x "$TOOLS/winid" ] || swiftc -O "$TOOLS/winid.swift" -o "$TOOLS/winid"
APP=./build/webgpu_app_macos_release/apps/webgpu_app/webgpu_app.app/Contents/MacOS/webgpu_app
cp imgui.ini "$2.imgui.ini" 2>/dev/null || true
: > "$2"
WEBIGEO_SANDBOX_BATCH="$1" "$APP" > "$2" 2>&1 &
PID=$!
SEEN=0
while kill -0 $PID 2>/dev/null; do
  N=$(grep -c "AvaFrame batch: screenshot " "$2" || true)
  if [ "$N" -gt "$SEEN" ]; then
    grep "AvaFrame batch: screenshot " "$2" | sed -n "$((SEEN+1)),${N}p" | sed 's/.*screenshot //' | while read -r P; do
      P="${P%\"}"; P="${P#\"}"
      screencapture -x -o -l "$("$TOOLS/winid" $PID)" "$P.tmp.png" && mv "$P.tmp.png" "$P"
    done
    SEEN=$N
  fi
  sleep 0.5
done
# The app writes its window layout on exit; keep the user's.
cp "$2.imgui.ini" imgui.ini 2>/dev/null || true
