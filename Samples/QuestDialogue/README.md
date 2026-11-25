# Quest Dialogue — first RPG rules slice

This is a terminal example of the engine's reusable quest/dialogue logic,
not yet a 3D RPG. It demonstrates prerequisite gating, a branching conversation,
one-time completion, a full objective chain and a final win condition.

Build independently with a C++23 compiler from the engine root:

    c++ -std=c++23 -Wall -Wextra -Werror Samples/QuestDialogue/QuestDialogueDemo.cpp -o /tmp/kairo-quest
    /tmp/kairo-quest

Choose 1 to accept, 1 to retrieve the crystal, 2 to return.
The runtime header documents all APIs. The tests exercise invalid restores
and 500 seeded DAG chains; UI rendering remains a future gate; disk saves are available.

Use `s` to save and `l` to load; supply a save path as the first CLI argument. For example, accept the quest, save and quit, start the demo again, load, collect and return. Saves use bounded KQS1 records and the existing Craft file-publication helper. They require matching authored definitions and are not authenticated.