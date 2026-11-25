#pragma once
#include <algorithm>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace kairo::player::detail
{
    // A quest is a prerequisite DAG, not a frame-loop script. Definitions
    // are immutable after play starts; completed IDs are the save boundary.
    // The caller translates world events into Complete() requests.
    class RuntimeQuestBook
    {
    public:
        struct Objective
        {
            std::string ID; // Persistent, case-sensitive authored identity.
            std::vector<std::string> Requires; // Earlier objective IDs (DAG).
        };
        // Prerequisites must already exist, making cycles impossible without
        // recursive graph traversal. Build definitions before restoring state.
        void Add(Objective objective)
        {
            if(m_Sealed||objective.ID.empty()||objective.ID.size()>128||
                m_Definitions.size()>=4096||m_Definitions.contains(objective.ID)||
                objective.Requires.size()>64)
                throw std::invalid_argument("Invalid objective definition.");
            std::sort(objective.Requires.begin(),objective.Requires.end());
            if(std::adjacent_find(objective.Requires.begin(),objective.Requires.end())!=objective.Requires.end())
                throw std::invalid_argument("Duplicate prerequisite.");
            for(const auto& requirement:objective.Requires)
                if(!m_Definitions.contains(requirement))
                    throw std::invalid_argument("Prerequisite must be defined first.");
            const auto key=objective.ID;
            m_Definitions.emplace(key,std::move(objective.Requires));
        }

        // A completed objective remains complete for the session. Unknown IDs
        // are configuration errors, whereas unmet prerequisites are normal.
        [[nodiscard]] bool IsComplete(std::string_view id) const
        {
            (void)RequireKnown(id);
            return std::find(m_Completed.begin(),m_Completed.end(),id)!=m_Completed.end();
        }
        [[nodiscard]] bool CanComplete(std::string_view id) const
        {
            const auto& requirements=RequireKnown(id);
            return !IsComplete(id)&&std::all_of(requirements.begin(),requirements.end(),
                [&](const auto& requirement){return IsComplete(requirement);});
        }
        // Return true only for a new completion. Duplicate world events
        // cannot grant the objective twice. Persist IDs in completion order.
        bool Complete(std::string_view id)
        {
            if(!CanComplete(id))return false;
            m_Completed.emplace_back(id);
            m_Sealed=true;
            return true;
        }
        [[nodiscard]] bool Won() const noexcept
        {return !m_Definitions.empty()&&m_Completed.size()==m_Definitions.size();}
        // Borrow until next completion/restore; copy to a save envelope.
        [[nodiscard]] const std::vector<std::string>& Snapshot() const noexcept
        {return m_Completed;}
        // Replay a bounded saved sequence against the same authored DAG.
        // Invalid IDs, duplicates or impossible order leave live state intact.
        void Restore(const std::vector<std::string>& completed)
        {
            if(completed.size()>m_Definitions.size())
                throw std::invalid_argument("Too many saved objectives.");
            auto candidate=*this;
            candidate.m_Completed.clear();
            for(const auto& id:completed)
                if(!candidate.Complete(id))
                    throw std::invalid_argument("Invalid saved objective order.");
            m_Completed=std::move(candidate.m_Completed);
            m_Sealed=true;
        }
    private:
        std::map<std::string,std::vector<std::string>,std::less<>> m_Definitions;
        std::vector<std::string> m_Completed;
        bool m_Sealed=false;
        // Resolve a stable ID without exposing mutable definition storage.
        [[nodiscard]] const std::vector<std::string>& RequireKnown(std::string_view id) const
        {
            const auto found=m_Definitions.find(id);
            if(found==m_Definitions.end())throw std::out_of_range("Unknown objective.");
            return found->second;
        }
    };

    // Dialogue graph owns text and choices. A choice can require a completed
    // objective and/or award another; the graph may intentionally contain loops.
    struct RuntimeDialogueChoice
    {
        std::string Label;
        std::string Next; // Empty means conversation ends.
        std::optional<std::string> Requires;
        std::optional<std::string> Awards;
    };
    struct RuntimeDialogueNode
    {
        std::string ID,Text;
        std::vector<RuntimeDialogueChoice> Choices;
    };

    class RuntimeDialogue
    {
    public:
        // Validate all references in one pass after collecting node IDs. This
        // permits forward links and branching without allowing dangling edges.
        RuntimeDialogue(std::vector<RuntimeDialogueNode> nodes,std::string entry,
            const RuntimeQuestBook& quests)
        {
            if(nodes.empty()||nodes.size()>4096)throw std::invalid_argument("Invalid dialogue size.");
            for(auto& node:nodes)
            {
                if(node.ID.empty()||node.ID.size()>128||node.Text.size()>16384||
                    node.Choices.empty()||node.Choices.size()>64)
                    throw std::invalid_argument("Invalid dialogue node.");
                const auto id=node.ID;
                if(!m_Nodes.emplace(id,std::move(node)).second)
                    throw std::invalid_argument("Duplicate dialogue node.");
            }
            if(!m_Nodes.contains(entry))throw std::invalid_argument("Missing dialogue entry.");
            for(const auto& [id,node]:m_Nodes)
            {
                (void)id;
                for(const auto& choice:node.Choices)
                {
                    if(choice.Label.empty()||choice.Label.size()>1024||
                        (!choice.Next.empty()&&!m_Nodes.contains(choice.Next)))
                        throw std::invalid_argument("Invalid dialogue edge.");
                    if(choice.Requires)(void)quests.IsComplete(*choice.Requires);
                    if(choice.Awards)(void)quests.IsComplete(*choice.Awards);
                }
            }
            m_Current=std::move(entry);
        }
        // The UI borrows the current node; null means conversation ended.
        [[nodiscard]] const RuntimeDialogueNode* Current() const
        {return m_Current?&m_Nodes.at(*m_Current):nullptr;}
        // Save the current authored node ID, not a pointer or array index.
        [[nodiscard]] std::optional<std::string> Position() const{return m_Current;}
        // Check the node against the current graph before changing position.
        // A missing optional denotes a finished conversation.
        void RestorePosition(std::optional<std::string> id)
        {
            if(id&&!m_Nodes.contains(*id))throw std::invalid_argument("Unknown saved dialogue node.");
            m_Current=std::move(id);
        }
        // Choice indices stay stable even when a prerequisite locks an option.
        // A disabled choice may be displayed, but cannot change conversation.
        [[nodiscard]] bool Available(std::size_t index,const RuntimeQuestBook& quests) const
        {
            const auto* node=Current();
            if(!node||index>=node->Choices.size())return false;
            const auto& choice=node->Choices[index];
            return (!choice.Requires||quests.IsComplete(*choice.Requires))&&
                (!choice.Awards||quests.IsComplete(*choice.Awards)||quests.CanComplete(*choice.Awards));
        }
        // Prepare the next node before awarding. A rejected choice does not
        // mutate either system. Commit the conversation after the quest update.
        bool Choose(std::size_t index,RuntimeQuestBook& quests)
        {
            if(!Available(index,quests))return false;
            const auto& choice=Current()->Choices[index];
            std::optional<std::string> next;
            if(!choice.Next.empty())next=choice.Next;
            if(choice.Awards&&!quests.IsComplete(*choice.Awards))
                if(!quests.Complete(*choice.Awards))return false;
            m_Current=std::move(next);
            return true;
        }
    private:
        std::map<std::string,RuntimeDialogueNode,std::less<>> m_Nodes;
        std::optional<std::string> m_Current;
    };
}
