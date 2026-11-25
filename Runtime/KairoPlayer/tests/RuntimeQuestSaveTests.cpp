#include "../RuntimeQuestSave.hpp"
#include <iostream>
#include <chrono>
using namespace kairo::player::detail;
static void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main()
{
    RuntimeQuestBook book;book.Add({"accept",{}});book.Add({"finish",{"accept"}});
    RuntimeDialogue talk({{"start","Hello",{{"Accept","end",{},"accept"}}},
        {"end","Finish",{{"Finish","","accept","finish"}}}},"start",book);
    Check(talk.Choose(0,book),"Accept failed");
    const auto saved=EncodeQuestSave(book,talk);
    // Reject every truncated prefix, without changing either live state.
    for(std::size_t n=0;n<saved.size();++n)
    {
        bool rejected=false;try{DecodeQuestSave(std::span(saved).first(n),book,talk);}catch(const std::exception&){rejected=true;}
        Check(rejected&&EncodeQuestSave(book,talk)==saved,"Truncated save changed state");
    }
    auto trailing=saved;trailing.push_back(0);bool rejected=false;
    try{DecodeQuestSave(trailing,book,talk);}catch(const std::exception&){rejected=true;}
    Check(rejected&&EncodeQuestSave(book,talk)==saved,"Trailing save accepted");
    auto unknown=saved;unknown.back()='z';rejected=false;
    try{DecodeQuestSave(unknown,book,talk);}catch(const std::exception&){rejected=true;}
    Check(rejected&&EncodeQuestSave(book,talk)==saved,"Unknown node accepted");
    const auto path=std::filesystem::temp_directory_path()/("kairo-quest-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    SaveQuest(path,book,talk);
    Check(talk.Choose(0,book)&&book.Won()&&!talk.Current(),"Finish failed");
    LoadQuest(path,book,talk);Check(EncodeQuestSave(book,talk)==saved&&!book.Won(),"Disk restore failed");
    Check(talk.Choose(0,book)&&book.Won(),"Restored branch failed");
    SaveQuest(path,book,talk);LoadQuest(path,book,talk);
    Check(book.Won()&&!talk.Current(),"Completed save failed");
    std::filesystem::remove(path);
    std::cout<<"Quest save roundtrip, replacement, malformed prefixes and rollback passed\n";
}
