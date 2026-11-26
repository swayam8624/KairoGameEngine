# Native Android platformer

The Android app compiles the same freestanding C++ game core and scalar adapter as the desktop/WebAssembly sample. Java owns an Android Canvas view, a fixed-step Choreographer loop and pointer-ID-based multi-touch; JNI copies all state and tile cells in one bounded array. Tap the status after victory to reset. Pausing or losing focus clears pending input.

Build with JDK 17, Gradle 8.9, Android SDK platform 35, NDK 27.2.12479018 and CMake 3.22.1:

    gradle -p Samples/AndroidPlatformer :app:assembleDebug
    adb install -r Samples/AndroidPlatformer/app/build/outputs/apk/debug/app-debug.apk

Linux JVM/JNI acceptance (no Android SDK needed):

    python3 scripts/test_android_host.py --compiler g++
    python3 scripts/test_android_host.py --compiler clang++

The observed desktop host tests passed both compilers, with 20,000 replay snapshots each, complete death/respawn/win and pointer/array checks. The emulator workflow is a separate acceptance gate: builds arm64/x86_64 debug APK, starts the activity on x86_64 Android, routes synthetic multi-pointer MotionEvents through the real View, checks an Android Canvas before/after image and retains a device screenshot/log. APK build/emulator execution have not yet been observed locally. No iOS, app signing, general mobile editor/exporter, screen-reader control tree or native Vulkan renderer port. This is a single-session narrow 2D game.
