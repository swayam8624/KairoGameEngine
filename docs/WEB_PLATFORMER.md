# Web platformer — shared C++ game rules

On Linux with Clang, wasm-ld and Python 3:

    python3 scripts/build_web_platformer.py /tmp/kairo-web
    python3 -m http.server 8000 --bind 127.0.0.1 --directory /tmp/kairo-web

Open http://127.0.0.1:8000 in your browser. The site uses Canvas 2D for
presentation and pointer/keyboard input; the game state and rules execute
inside the same freestanding C++23 core as the native platformer. No
Emscripten, external JS libraries, assets or network service are needed.
Do not open index.html directly as file:// because the module is fetched.

    PYTHONPATH=scripts python3 -m unittest discover -s scripts/tests -p test_web_wasm.py
    PYTHONPATH=scripts python3 -m unittest discover -s scripts/tests -p test_web_platformer.py

The first test uses Node's actual WebAssembly runtime and a separately built
native reference process. The second requires a permitted Chromium browser
and checks real events, Canvas pixels, death/reset and the complete objective.

Native tests passed under GCC and Clang (50,000 randomized ticks each),
and 20,279 native/WASM state/map comparisons passed. The local browser
launch timed out; independent navigation was administrator-blocked. Browser
rendering therefore remains unverified. CI keeps the parity and browser
gates distinct. This is a narrow game deployment path, not a port of the
native Metal/Vulkan renderer, general project exporter, or Android/iOS app.
