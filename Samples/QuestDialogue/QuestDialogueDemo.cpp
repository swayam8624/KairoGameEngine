#include "../../Runtime/KairoPlayer/RuntimeQuestDialogue.hpp"
#include <iostream>
#include <string>

using namespace kairo::player::detail;

// A small terminal RPG slice. The same state machine can later be driven by
// native Player input, NPC interactions and authored menu controls.
int main()
{
    RuntimeQuestBook quests;
    quests.Add({"accept",{}});quests.Add({"find",{"accept"}});quests.Add({"return",{"find"}});
    RuntimeDialogue dialogue({
        {"village","The lantern keeper needs a crystal.",{
            {"Accept the quest","cave",{},"accept"},
            {"Return the crystal","","find","return"}}},
        {"cave","A crystal glows in the cavern.",{
            {"Pick up the crystal","village","accept","find"},
            {"Go back","village",{},{}}}}
    },"village",quests);
    while(const auto* node=dialogue.Current())
    {
        std::cout<<"\n"<<node->Text<<"\n";
        for(std::size_t i=0;i<node->Choices.size();++i)
            std::cout<<i+1<<". "<<node->Choices[i].Label
                <<(dialogue.Available(i,quests)?"":" (locked)")<<"\n";
        std::cout<<"Choose a number, or q to quit: ";
        std::string line;if(!std::getline(std::cin,line)||line=="q")return 0;
        if(line.size()!=1||line[0]<'1'||line[0]>'9'||
            !dialogue.Choose(static_cast<std::size_t>(line[0]-'1'),quests))
            std::cout<<"That choice is unavailable.\n";
    }
    std::cout<<(quests.Won()?"Quest complete!\n":"Conversation ended.\n");
    return quests.Won()?0:1;
}
