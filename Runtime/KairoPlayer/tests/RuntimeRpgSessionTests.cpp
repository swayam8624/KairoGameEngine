#include "RuntimeRpgSession.hpp"
#include <iostream>
#include <random>
using namespace kairo::player::detail;
static void Check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main()
{
    QuestLab q;
    Check(!q.Choose(1)&&q.Bag().Count("crystal")==0,"locked return");
    Check(q.Choose(0)&&q.Choose(1)&&q.Choose(0)&&q.Choose(0),"branch route");
    Check(q.Bag().Count("crystal")==1&&q.Choose(0),"revisit cave");
    const auto before=q.Encode();Check(!q.Choose(0)&&q.Encode()==before,"duplicate crystal");
    auto corrupt=before;corrupt[4]=0;
    bool rejected=false;try{q.Decode(corrupt);}catch(const std::invalid_argument&){rejected=true;}
    Check(rejected&&q.Encode()==before,"inconsistent item save");
    for(std::size_t i=0;i<before.size();++i)
    {
        rejected=false;try{q.Decode(std::span(before).first(i));}catch(const std::invalid_argument&){rejected=true;}
        Check(rejected&&q.Encode()==before,"truncated save changed session");
    }
    QuestLab restored;restored.Decode(before);Check(restored.Encode()==before,"roundtrip");
    QuestLab legacy;legacy.Decode(EncodeQuestSave(q.Book(),q.Dialogue()));Check(legacy.Encode()==before,"legacy migration");
    Check(restored.Choose(1)&&restored.Choose(1)&&restored.Book().Won()&&restored.Bag().Count("crystal")==0&&restored.Bag().Count("gold")==10,"turn-in reward");
    const auto won=restored.Encode();Check(!restored.Choose(1)&&restored.Encode()==won,"duplicate reward");
    std::mt19937 rng(82);
    for(int i=0;i<1000;++i)
    {
        QuestLab a;
        for(int step=0;step<20;++step)
        {
            const auto old=a.Encode();const auto index=rng()%3;const auto allowed=a.Available(index);
            Check(a.Choose(index)==allowed,"choice availability mismatch");
            if(!allowed)Check(a.Encode()==old,"rejected choice mutation");
            QuestLab b;b.Decode(a.Encode());Check(a.Encode()==b.Encode(),"random roundtrip");
        }
    }
    // Cooperating writers reuse the actual save helper in a unique CTest work dir.
    const auto path=std::filesystem::current_path()/"rpg-session-regression.ksave";
    if(std::filesystem::exists(path))throw std::runtime_error("Regression save path occupied");
    try{restored.Save(path);QuestLab disk;disk.Load(path);Check(disk.Encode()==won,"disk restart");std::filesystem::remove(path);}
    catch(...){std::filesystem::remove(path);throw;}
    std::cout<<"20000 RPG transitions and save roundtrips, legacy migration and reward conservation passed\n";
}
