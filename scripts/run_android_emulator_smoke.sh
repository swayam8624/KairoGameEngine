#!/usr/bin/env bash
# Run only against the isolated test emulator. Retain actual device logs and image.
set -euo pipefail
apk="$1"
evidence="$2"
mkdir -p "$evidence"
adb install -r "$apk"
adb shell am force-stop org.kairo.platformer
adb logcat -c
adb shell am start -W -n org.kairo.platformer/.MainActivity --ez smoke true
for ((i=0;i<60;i++)); do
    adb logcat -d -s KairoMobile:I AndroidRuntime:E > "$evidence/logcat.txt"
    if grep -q SMOKE_FAILED "$evidence/logcat.txt"; then cat "$evidence/logcat.txt"; exit 1; fi
    if grep -q SMOKE_PASSED "$evidence/logcat.txt"; then
        adb exec-out screencap -p > "$evidence/emulator.png"
        test -s "$evidence/emulator.png"
        exit 0
    fi
    sleep 1
done
cat "$evidence/logcat.txt"
exit 1
