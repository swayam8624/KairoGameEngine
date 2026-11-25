#include "../CraftEquipmentSave.hpp"
#include <iostream>
#include <random>
using namespace kairo::player::detail;
void Check(bool ok){if(!ok)throw std::runtime_error("envelope invariant");}
int main(){
    std::mt19937 rng(782);
    const auto rejects=[](const std::vector<std::uint8_t>& bytes,std::size_t limit){try{(void)DecodeCraftEquipmentEnvelope(bytes,limit);return false;}catch(const std::invalid_argument&){return true;}};
    for(unsigned i=0;i<10000;++i){
        std::vector<std::uint8_t> payload(8+rng()%1024);
        std::copy_n(CraftEquipmentMagic.begin(),7,payload.begin());
        payload[7]=i%2?'3':'4';
        for(std::size_t j=8;j<payload.size();++j)payload[j]=static_cast<std::uint8_t>(rng());
        CraftEquipment e{static_cast<std::uint32_t>(rng()%65),false}; e.Equipped=e.Durability&&rng()%2;
        const auto encoded=EncodeCraftEquipmentEnvelope(payload,e,1032);
        const auto decoded=DecodeCraftEquipmentEnvelope(encoded,1032);
        Check(decoded.Equipment.Durability==e.Durability&&decoded.Equipment.Equipped==e.Equipped&&std::equal(decoded.Payload.begin(),decoded.Payload.end(),payload.begin(),payload.end()));
        Check(EncodeCraftEquipmentEnvelope(decoded.Payload,decoded.Equipment,1032)==encoded);
        auto broken=encoded;broken[rng()%broken.size()]^=1;Check(rejects(broken,1032));
        broken=encoded;broken.pop_back();Check(rejects(broken,1032));
        broken=encoded;broken[16]=0;broken[17]=0;broken[18]=1;broken.resize(broken.size()-8);CraftWrite(broken,CraftSaveChecksum(broken),8);Check(rejects(broken,1032));
        Check(rejects(encoded,payload.size()-1));
    }
    std::cout<<"10000 envelope roundtrips and 40000 corruption, state, truncation and limit rejections passed\n";
}
