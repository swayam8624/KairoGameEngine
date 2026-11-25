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
