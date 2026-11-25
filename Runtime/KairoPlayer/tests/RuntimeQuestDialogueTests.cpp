#include "../RuntimeQuestDialogue.hpp"
#include <iostream>
#include <random>
using namespace kairo::player::detail;

// Explicit checks survive Release builds, unlike assert under NDEBUG.
static void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main()
{
    // A complete two-objective dialogue loop, including locked choices,
    // duplicate requests and transactionally rejected saved progress.
    RuntimeQuestBook book;
    book.Add({"meet",{}});book.Add({"return",{"meet"}});
    RuntimeDialogue talk({{"start","Hello",{{"Return","","meet","return"},{"Meet","end",{},"meet"}}},
        {"end","Good luck",{{"Back","start",{},{}}}}},"start",book);
    Check(!talk.Choose(0,book),"Locked branch accepted");
    Check(talk.Choose(1,book)&&book.IsComplete("meet"),"Meet failed");
    Check(talk.Choose(0,book)&&talk.Choose(0,book)&&book.Won(),"Quest failed");
    Check(!talk.Current()&&!book.Complete("return"),"Duplicate completion");
    const auto saved=book.Snapshot();
    bool rejected=false;
    try{book.Restore({"return","meet"});}catch(const std::invalid_argument&){rejected=true;}
    Check(rejected&&book.Snapshot()==saved,"Invalid restore changed state");
    book.Restore({"meet"});Check(!book.Won()&&book.CanComplete("return"),"Restore failed");
    // Random DAG chains of varying lengths: blocked completion never changes
    // state, sequential completion always wins, and replay restores the same IDs.
    std::mt19937 random(805);
    for(unsigned trial=0;trial<500;++trial)
    {
        RuntimeQuestBook q;const unsigned n=2+random()%64;
        for(unsigned i=0;i<n;++i)
            q.Add({std::to_string(i),i?std::vector<std::string>{std::to_string(i-1)}:std::vector<std::string>{}});
        Check(!q.Complete(std::to_string(n-1)),"Chain bypass");
        for(unsigned i=0;i<n;++i)Check(q.Complete(std::to_string(i)),"Chain failure");
        Check(q.Won(),"Win missing");const auto state=q.Snapshot();
        q.Restore({});Check(!q.Won(),"Reset failed");
        q.Restore(state);Check(q.Won()&&q.Snapshot()==state,"Replay failed");
    }
    std::cout<<"Quest/dialogue scenarios and 500 seeded DAG replays passed\n";
}
