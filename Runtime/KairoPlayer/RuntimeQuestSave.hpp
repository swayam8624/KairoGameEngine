#pragma once
#include "CraftSaveFile.hpp"
#include "RuntimeQuestDialogue.hpp"

namespace kairo::player::detail
{
    // Quest save v1: magic KQS1, little-endian u16 completed count,
    // then length-prefixed IDs (1..128 bytes), followed by a u8 position
    // flag and optional dialogue ID. No pointer, platform ABI or native
    // size_t appears on disk. The caller supplies the matching definitions.
    inline constexpr std::size_t MaximumQuestSaveBytes=600000;
    inline std::vector<std::uint8_t> EncodeQuestSave(const RuntimeQuestBook& book,const RuntimeDialogue& dialogue)
    {
        std::vector<std::uint8_t> bytes{'K','Q','S','1'};
        const auto count=book.Snapshot().size();
        bytes.push_back(static_cast<std::uint8_t>(count&255));
        bytes.push_back(static_cast<std::uint8_t>(count>>8));
        const auto append=[&](const std::string& id)
        {
            if(id.empty()||id.size()>128)throw std::invalid_argument("Invalid saved ID.");
            bytes.push_back(static_cast<std::uint8_t>(id.size()));
            bytes.insert(bytes.end(),id.begin(),id.end());
        };
        for(const auto& id:book.Snapshot())append(id);
        const auto position=dialogue.Position();
        bytes.push_back(position?1:0);
        if(position)append(*position);
        return bytes;
    }
    // Decode into copies, validate the quest DAG and dialogue node against
    // authored definitions, then publish both. Unknown IDs, impossible
    // completion order, truncation and trailing bytes reject the whole save.
    // The envelope validates structure, not authenticity or quest causality
    // for arbitrary hand-edited dialogue positions.
    inline void DecodeQuestSave(std::span<const std::uint8_t> bytes,RuntimeQuestBook& book,RuntimeDialogue& dialogue)
    {
        if(bytes.size()<7||bytes.size()>MaximumQuestSaveBytes||bytes[0]!='K'||bytes[1]!='Q'||bytes[2]!='S'||bytes[3]!='1')
            throw std::invalid_argument("Invalid quest save header.");
        const auto count=unsigned(bytes[4])|(unsigned(bytes[5])<<8);
        if(count>4096)throw std::invalid_argument("Too many saved objectives.");
        std::size_t cursor=6;
        const auto read=[&]() -> std::uint8_t
        {
            if(cursor>=bytes.size())throw std::invalid_argument("Truncated quest save.");
            return bytes[cursor++];
        };
        const auto readID=[&]()
        {
            const auto length=read();
            if(!length||length>128||length>bytes.size()-cursor)throw std::invalid_argument("Invalid saved ID length.");
            std::string id(reinterpret_cast<const char*>(bytes.data()+cursor),length);
            cursor+=length;return id;
        };
        std::vector<std::string> completed;completed.reserve(count);
        for(unsigned i=0;i<count;++i)completed.push_back(readID());
        const auto flag=read();
        if(flag>1)throw std::invalid_argument("Invalid dialogue position flag.");
        std::optional<std::string> position;
        if(flag)position=readID();
        if(cursor!=bytes.size())throw std::invalid_argument("Trailing quest save data.");
        auto candidateBook=book;auto candidateDialogue=dialogue;
        candidateBook.Restore(completed);candidateDialogue.RestorePosition(std::move(position));
        book=std::move(candidateBook);dialogue=std::move(candidateDialogue);
    }
    // Reuse the existing Craft save I/O rather than implement another
    // file publication protocol. Caller owns the save path and parent directory.
    inline void SaveQuest(const std::filesystem::path& path,const RuntimeQuestBook& book,const RuntimeDialogue& dialogue)
    {
        const auto bytes=EncodeQuestSave(book,dialogue);
        WriteCraftSaveFile(path,bytes,MaximumQuestSaveBytes);
    }
    inline void LoadQuest(const std::filesystem::path& path,RuntimeQuestBook& book,RuntimeDialogue& dialogue)
    {
        const auto bytes=ReadCraftSaveFile(path,MaximumQuestSaveBytes);
        DecodeQuestSave(bytes,book,dialogue);
    }
}
