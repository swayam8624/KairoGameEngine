#include "../RuntimeProgressCheckpoint.hpp"
#include <iostream>
using namespace kairo::player::detail;
void Check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(int argc,char** argv)
{
    if(argc!=3)return 2;
    const std::string mode=argv[1];const std::filesystem::path path=argv[2];
    RuntimeCollectibleRun goals;goals.Add(1,{0,0,0});goals.Add(2,{3,0,0});
    std::optional<RuntimeProximityTriggers> triggers(std::in_place,
        "kairo-triggers 1\ntrigger 1 0 0 0 1 once 8 show\ntrigger 2 0 0 0 1 exit 8 hide\n");
    TriggerVisuals visuals{{8,false}};
    constexpr std::uint64_t identity=123456789;
    if(mode=="save")
    {
        Check(goals.Step({0,0,0},.25).size()==1,"goal missing");
        for(const auto& e:triggers->Step({0,0,0}))visuals.at(e.Target)=e.Enable;
        Check(visuals.at(8),"once missing");
        SaveProgress(path,identity,goals,triggers,visuals);
    }
    else if(mode=="restore")
    {
        LoadProgress(path,identity,goals,triggers,visuals);
        Check(goals.Score()==1&&goals.Elapsed()==.25&&visuals.at(8),"restore differs");
        Check(goals.Step({0,0,0},.25).empty()&&triggers->Step({0,0,0}).empty(),"repeated progress");
        const auto exit=triggers->Step({3,0,0});
        Check(exit.size()==1&&exit[0].ID==2&&!exit[0].Enable,"missing restored exit");
        for(const auto& e:exit)visuals.at(e.Target)=e.Enable;
        Check(triggers->Step({0,0,0}).empty(),"once repeated after reload");
        Check(goals.Step({3,0,0},.5).size()==1&&goals.Won()&&goals.Elapsed()==1,"completion differs");
        SaveProgress(path,identity,goals,triggers,visuals);
        const auto before=ReadCraftSaveFile(path,MaximumProgressBytes);
        auto lock=path;lock+=".write-lock";std::filesystem::create_directory(lock);
        bool blocked=false;try{SaveProgress(path,identity,goals,triggers,visuals);}catch(const std::runtime_error&){blocked=true;}
        Check(blocked&&ReadCraftSaveFile(path,MaximumProgressBytes)==before,"writer lock changed checkpoint");
        std::filesystem::remove(lock);
    }
    else if(mode=="reject")
    {
        auto copyGoals=goals;auto copyTriggers=triggers;auto copyVisuals=visuals;
        LoadProgress(path,identity,copyGoals,copyTriggers,copyVisuals);
        Check(copyGoals.Won()&&!copyVisuals.at(8),"second cold restore lost final state");
        const auto fingerprint=ProgressIdentity({path});
        bool rejected=false;try{LoadProgress(path,identity+1,goals,triggers,visuals);}catch(const std::invalid_argument&){rejected=true;}
        Check(rejected&&!goals.Score()&&!visuals.at(8)&&!triggers->Progress()[0].Fired,"mismatch mutated progress");
        // Valid header/goal state but unknown target; all live state must remain unchanged.
        std::ofstream out(path);out<<"kairo-progress 1\n"<<identity<<" 2\n1\n1\n2\n1 1 1\n2 1 0\n1\n999 1\n";out.close();
        Check(fingerprint!=ProgressIdentity({path}),"source edit fingerprint unchanged");
        rejected=false;try{LoadProgress(path,identity,goals,triggers,visuals);}catch(const std::invalid_argument&){rejected=true;}
        Check(rejected&&!goals.Score()&&!visuals.at(8)&&!triggers->Progress()[0].Fired,"invalid visual mutated progress");
    }
    else return 2;
    std::cout<<mode<<" passed\n";
}
