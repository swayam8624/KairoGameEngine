#pragma once

#include <condition_variable>
#include <cstddef>
#include <exception>
#include <functional>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace kairo::player::detail
{
    // One CPU producer, one render-thread consumer. Fixed slots bound the
    // number of queued, running AND completed (not yet uploaded) meshes.
    // No renderer API may be called by the generator. Destroying this queue
    // joins its worker before any captured configuration is destroyed.
    template<class Coordinate,class Mesh>
    class TerrainGenerationQueue final
    {
        static_assert(std::is_nothrow_move_constructible_v<Coordinate>);
        static_assert(std::is_nothrow_copy_constructible_v<Coordinate>);
        static_assert(std::is_nothrow_move_constructible_v<Mesh>);
        enum class State { Free,Queued,Running,Ready };
    public:
        struct Completion
        {
            Coordinate Cell;
            std::optional<Mesh> Value;
            std::exception_ptr Failure;
        };

        TerrainGenerationQueue(std::size_t capacity,
            std::function<Mesh(Coordinate)> generate)
            : m_Generate(std::move(generate)),m_Slots(ValidateCapacity(capacity)),
              m_Worker([this]{Run();}) {}
        TerrainGenerationQueue(const TerrainGenerationQueue&)=delete;
        TerrainGenerationQueue& operator=(const TerrainGenerationQueue&)=delete;
        ~TerrainGenerationQueue()
        {
            {
                std::lock_guard lock(m_Mutex);
                m_Stop=true;
            }
            m_Wake.notify_one();
            m_Worker.join();
        }

        // False means no reservation was adopted. The caller must settle
        // the corresponding planner load as failed and may retry next frame.
        bool TrySubmit(Coordinate coordinate)
        {
            {
                std::lock_guard lock(m_Mutex);
                if(m_Stop)return false;
                for(auto& slot:m_Slots)
                    if(slot.Status==State::Free)
                    {
                        slot.Cell.emplace(coordinate);
                        slot.Status=State::Queued;
                        ++m_Occupied;
                        m_Wake.notify_one();
                        return true;
                    }
            }
            return false;
        }

        // A completion slot remains occupied until the render thread
        // consumes it, bounding the memory of generated but unuploaded data.
        [[nodiscard]] std::optional<Completion> TryTake()
        {
            std::lock_guard lock(m_Mutex);
            for(std::size_t i=0;i<m_Slots.size();++i)
            {
                const auto index=(m_NextReady+i)%m_Slots.size();
                auto& slot=m_Slots[index];
                if(slot.Status==State::Ready)
                {
                    std::optional<Completion> result(std::in_place,
                        Completion{*slot.Cell,std::move(slot.Value),slot.Failure});
                    slot.Cell.reset();
                    slot.Value.reset();
                    slot.Failure=nullptr;
                    slot.Status=State::Free;
                    --m_Occupied;
                    m_NextReady=(index+1u)%m_Slots.size();
                    return result;
                }
            }
            return std::nullopt;
        }

        [[nodiscard]] std::size_t Occupied() const
        {
            std::lock_guard lock(m_Mutex);
            return m_Occupied;
        }

    private:
        struct Slot
        {
            State Status=State::Free;
            std::optional<Coordinate> Cell;
            std::optional<Mesh> Value;
            std::exception_ptr Failure;
        };

        static std::size_t ValidateCapacity(std::size_t capacity)
        {
            if(capacity==0 || capacity>1024)
                throw std::invalid_argument("Terrain generation capacity must be 1..1024.");
            return capacity;
        }

        // Slot selection and publication are protected; the expensive
        // generator runs entirely outside the mutex.
        void Run()
        {
            for(;;)
            {
                std::unique_lock lock(m_Mutex);
                m_Wake.wait(lock,[&]{
                    if(m_Stop)return true;
                    for(const auto& slot:m_Slots)
                        if(slot.Status==State::Queued)return true;
                    return false;
                });
                if(m_Stop)return;
                Slot* work=nullptr;
                for(std::size_t i=0;i<m_Slots.size();++i)
                {
                    const auto index=(m_NextWork+i)%m_Slots.size();
                    if(m_Slots[index].Status==State::Queued)
                    {
                        work=&m_Slots[index];
                        m_NextWork=(index+1u)%m_Slots.size();
                        break;
                    }
                }
                work->Status=State::Running;
                const Coordinate cell=*work->Cell;
                lock.unlock();
                std::optional<Mesh> mesh;
                std::exception_ptr failure;
                try{mesh.emplace(m_Generate(cell));}
                catch(...){failure=std::current_exception();}
                lock.lock();
                if(mesh)work->Value.emplace(std::move(*mesh));
                work->Failure=failure;
                work->Status=State::Ready;
            }
        }

        std::function<Mesh(Coordinate)> m_Generate;
        std::vector<Slot> m_Slots;
        mutable std::mutex m_Mutex;
        std::condition_variable m_Wake;
        std::size_t m_Occupied=0,m_NextWork=0,m_NextReady=0;
        bool m_Stop=false;
        std::thread m_Worker;
    };
}
