#pragma once
#include "RuntimeItemInventory.hpp"
#include "RuntimeQuestSave.hpp"
#include <array>

namespace kairo::player::detail
{
    // Small authored RPG vertical slice. The quest graph owns objectives,
    // dialogue owns position, inventory owns items. Only this session may
    // publish their combined state, so the three systems cannot drift apart.
    class QuestLab
    {
    public:
        QuestLab():m_Book(Definitions()),m_Dialogue({
            {"village","THE KEEPER NEEDS A CRYSTAL",{{"VISIT CAVE","cave",{},"accept"},{"RETURN CRYSTAL","","find","return"}}},
            {"cave","A CRYSTAL GLOWS",{{"PICK UP CRYSTAL","village","accept","find"},{"GO BACK","village",{},{}}}}
        },"village",m_Book),m_Bag({{"crystal",1},{"gold",10}}){}
        [[nodiscard]] const RuntimeQuestBook& Book() const noexcept{return m_Book;}
        [[nodiscard]] const RuntimeDialogue& Dialogue() const noexcept{return m_Dialogue;}
        [[nodiscard]] const RuntimeItemInventory& Bag() const noexcept{return m_Bag;}
        [[nodiscard]] bool Available(std::size_t index) const
        {
            if(!m_Dialogue.Available(index,m_Book))return false;
            const auto& node=*m_Dialogue.Current();
            if(node.ID=="cave"&&index==0)return !m_Book.IsComplete("find")&&m_Bag.Count("crystal")==0;
            if(node.ID=="village"&&index==1)return m_Bag.Count("crystal")==1;
            return true;
        }
        // Stage all mutations on one candidate. A locked choice or failed
        // trade leaves dialogue, objective order and inventory unchanged.
        bool Choose(std::size_t index)
        {
            if(!Available(index))return false;
            auto next=*this;
            const auto& node=*m_Dialogue.Current();
            using Change=RuntimeItemInventory::Change;
            if(node.ID=="cave"&&index==0&&!next.m_Bag.Apply(std::array{Change{"crystal",1}}))return false;
            if(node.ID=="village"&&index==1&&!next.m_Bag.Apply(std::array{Change{"crystal",-1},Change{"gold",10}}))return false;
            if(!next.m_Dialogue.Choose(index,next.m_Book))return false;
            next.Validate();*this=std::move(next);return true;
        }
        // KRL1 stores explicit item counts, then the existing KQS1 payload.
        // Its narrow fixture inventory is fully determined by quest progress;
        // reject mismatched counts rather than silently grant a reward twice.
        [[nodiscard]] std::vector<std::uint8_t> Encode() const
        {
            Validate();const auto payload=EncodeQuestSave(m_Book,m_Dialogue);
            std::vector<std::uint8_t> bytes{'K','R','L','1',static_cast<std::uint8_t>(m_Bag.Count("crystal")),static_cast<std::uint8_t>(m_Bag.Count("gold"))};
            bytes.insert(bytes.end(),payload.begin(),payload.end());return bytes;
        }
        void Decode(std::span<const std::uint8_t> bytes)
        {
            auto next=*this;
            if(bytes.size()>=4&&bytes[0]=='K'&&bytes[1]=='Q'&&bytes[2]=='S'&&bytes[3]=='1')
            {
                // Legacy lab saves had no inventory. The fixed quest graph
                // uniquely determines the missing counts; preserve progress.
                DecodeQuestSave(bytes,next.m_Book,next.m_Dialogue);
                next.m_Bag.Restore(next.ExpectedCounts());
            }
            else
            {
                if(bytes.size()<13||bytes.size()>MaximumBytes||bytes[0]!='K'||bytes[1]!='R'||bytes[2]!='L'||bytes[3]!='1')
                    throw std::invalid_argument("Invalid RPG save header.");
                DecodeQuestSave(bytes.subspan(6),next.m_Book,next.m_Dialogue);
                next.m_Bag.Restore({{"crystal",bytes[4]},{"gold",bytes[5]}});
            }
            next.Validate();*this=std::move(next);
        }
        void Save(const std::filesystem::path& path) const{const auto bytes=Encode();WriteCraftSaveFile(path,bytes,MaximumBytes);}
        void Load(const std::filesystem::path& path){Decode(ReadCraftSaveFile(path,MaximumBytes));}
    private:
        static constexpr std::size_t MaximumBytes=MaximumQuestSaveBytes+6;
        RuntimeQuestBook m_Book;
        RuntimeDialogue m_Dialogue;
        RuntimeItemInventory m_Bag;
        static RuntimeQuestBook Definitions()
        {
            RuntimeQuestBook b;b.Add({"accept",{}});b.Add({"find",{"accept"}});b.Add({"return",{"find"}});return b;
        }
        [[nodiscard]] RuntimeItemInventory::Counts ExpectedCounts() const
        {return {{"crystal",m_Book.IsComplete("find")&&!m_Book.Won()?1u:0u},{"gold",m_Book.Won()?10u:0u}};}
        // Reject impossible dialogue positions even if the generic KQS1
        // reader accepts their IDs. Village is valid before/after visits.
        void Validate() const
        {
            const auto position=m_Dialogue.Position();
            if(m_Bag.Snapshot()!=ExpectedCounts()||m_Book.Won()!=!position||
                (position&&*position=="cave"&&!m_Book.IsComplete("accept")))
                throw std::invalid_argument("Inconsistent RPG progress.");
        }
    };
}
