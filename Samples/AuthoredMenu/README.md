# Authored menu sample

This project copies the existing playable vertical slice and adds an optional
Config/Menu.kmenu. Each button has a stable ID, literal label, allowlisted
host command (resume/quit), and x/y/width/height in viewport thousandths.
The Player's shipping bridge loads the menu into the existing RuntimeUIScene
and LocalizationCatalog, including accessibility labels. Run
KairoPlayer Samples/AuthoredMenu/Project.kproject --validate to check the
document and project together. Native input and draw routing are a separate
follow-up; this branch alone does not show a menu on screen.
