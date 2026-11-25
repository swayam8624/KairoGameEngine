#include "../RuntimeNetworkPrediction.hpp"
#include <iostream>
#include <random>
using namespace kairo::player::detail;
static void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main()
{
    // Server receives reliable ordered inputs. Simulate delayed, dropped and
    // reordered snapshots independently of TCP; final acknowledgment drains
    // the client journal. This is not a UDP reliability implementation.
    std::mt19937 rng(1926);
    for(unsigned trial=0;trial<500;++trial)
    {
        NetworkPrediction client;NetworkAuthority server;
        std::vector<NetworkFrame> delayed;
        for(unsigned tick=1;tick<=128;++tick)
        {
            const auto input=client.Predict(int(rng()%3)-1,int(rng()%3)-1);
            const auto snapshot=server.Apply(input);
            if(rng()%4!=0)delayed.push_back(snapshot); // drop 25% of snapshots
            if(!delayed.empty()&&rng()%3==0)
            {
                const auto index=rng()%delayed.size();
                (void)client.Reconcile(delayed[index]);delayed.erase(delayed.begin()+static_cast<std::ptrdiff_t>(index));
            }
            Check(client.Predicted()==server.Snapshot(),"Prediction diverged on equal simulation");
        }
        (void)client.Reconcile(server.Snapshot());
        Check(client.Pending().empty()&&client.Acknowledged()==server.Snapshot(),"Final acknowledgment failed");
    }
    NetworkPrediction client;
    (void)client.Predict(1,0);(void)client.Predict(1,0);(void)client.Predict(0,1);
    Check(client.Reconcile({NetworkKind::Snapshot,1,10,20}),"Correction rejected");
    Check(client.Predicted()==NetworkFrame{NetworkKind::Snapshot,3,11,21}&&client.Pending().size()==2,"Replay correction failed");
    Check(!client.Reconcile({NetworkKind::Snapshot,1,0,0}),"Repeated acknowledgment accepted");
    bool rejected=false;try{(void)client.Reconcile({NetworkKind::Snapshot,4,0,0});}catch(const std::invalid_argument&){rejected=true;}
    Check(rejected&&client.Predicted()==NetworkFrame{NetworkKind::Snapshot,3,11,21},"Future ack mutated state");
    NetworkPrediction saturated;
    for(unsigned i=0;i<NetworkPrediction::MaximumPending;++i)(void)saturated.Predict(1,0);
    rejected=false;try{(void)saturated.Predict(1,0);}catch(const std::length_error&){rejected=true;}
    Check(rejected&&saturated.Pending().size()==NetworkPrediction::MaximumPending,"Journal capacity failed");
    Check(saturated.Reconcile({NetworkKind::Snapshot,256,256,0}),"Drain failed");
    Check(saturated.Predict(0,1).Tick==257,"Prediction did not resume");
    std::cout<<"64000 delayed/lost/reordered snapshot ticks, correction and journal bounds passed\n";
}
