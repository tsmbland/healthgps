#include "population.h"
#include <execution>

namespace hgps {
    Population::Population(const std::size_t size) 
        : initial_size_{size}, people_(size)
    {}

    std::size_t Population::size() const noexcept {
        return people_.size();
    }

    std::size_t Population::initial_size() const noexcept {
        return initial_size_;
    }

    std::size_t Population::current_active_size() const noexcept {
        auto active_pop_size = std::count_if(std::execution::par, people_.cbegin(),people_.cend(),
            [](const auto& p) { return p.is_active(); });

        return active_pop_size;
    }

    Person& Population::operator[](std::size_t index) {
        return people_[index];
    }

    const Person& Population::operator[](std::size_t index) const {
        return people_[index];
    }

    void Population::add(Person&& entity, unsigned int time) noexcept {
        auto recycle = find_index_of_recyclables(time, 1);
        if (recycle.size() > 0) {
            people_.at(recycle.at(0)) = entity;
            return;
        }

        people_.emplace_back(entity);
    }

    void Population::add_newborn_babies(const std::size_t number, core::Gender gender, unsigned int time) noexcept {
        auto recycle = find_index_of_recyclables(time, number);
        auto remaining = number;
        if (recycle.size() > 0) {
            auto replacebles = std::min(number, recycle.size());
            for (auto index = std::size_t{ 0 }; index < replacebles; index++)
            {
                people_.at(recycle.at(index)) = Person{ gender };
                remaining--;
            }
        }

        for (auto i = std::size_t{ 0 }; i < remaining; i++) {
            people_.emplace_back(Person{ gender });
        }
    }

    std::vector<int> Population::find_index_of_recyclables(unsigned int time, std::size_t top) const noexcept
    {
        auto indices = std::vector<int>{};
        for (auto index = 0; auto& entity : people_) {
            if (!entity.is_active() && entity.time_of_death() < time && entity.time_of_migration() < time) {
                indices.emplace_back(index);
                if (top > 0 && indices.size() >= top) {
                    break;
                }
            }

            index++;
        }

        return indices;
    }
}