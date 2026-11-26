# Authored menu sample

This project copies the existing playable vertical slice and adds an optional
Config/Menu.kmenu. Each button has a stable ID, literal label, allowlisted
host command (resume/quit), and x/y/width/height in viewport thousandths.
The Player's shipping bridge loads the menu into the existing RuntimeUIScene
and LocalizationCatalog, including accessibility labels. Run
KairoPlayer Samples/AuthoredMenu/Project.kproject --validate to check the
document and project together. Run without --validate using --renderer metal or --renderer vulkan. The
native menu starts open, pauses gameplay, and offers Tab/Shift-Tab, arrows,
Enter/Space or pointer activation, F1 reopen/close and F2 larger text. The
focused label is also reflected in the window title; this is not OS screen
reader integration. Bitmap labels may truncate for unusually crowded menus.
Native execution remains unverified at authoring time.

## Graphical authoring companion

Build KairoMenuDocumentTool with the workspace, or compile it standalone:

    c++ -std=c++23 -Wall -Wextra -Werror -IRuntime/KairoPlayer Samples/AuthoredMenu/MenuDocumentTool.cpp -o /tmp/kairo-menu-validator
    python3 scripts/kairo_menu_designer.py Samples/AuthoredMenu --validator /tmp/kairo-menu-validator

Python 3 with Tk is required. Drag a button, edit its ID/label/action and normalized dimensions, add/delete buttons, undo and save. The actual Player C++ parser validates every transaction and writes canonical Menu.kmenu data; the graphical preview uses Tk text, not the exact runtime bitmap glyphs. Existing disk edits are detected before atomic replacement. The tool assumes a locally trusted, non-concurrently-mutating project directory. Standalone companion, not yet embedded in KairoEditor. Native Player smoke remains independent.
