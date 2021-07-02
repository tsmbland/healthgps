#include "demographic.h"
#include <numeric>
#include <cassert>

namespace hgps {
	DemographicModule::DemographicModule(std::map<int, std::map<int, AgeRecord>>&& data)
		: data_{ data } {
		if (!data_.empty()) {
			auto first_entry = data_.begin();
			time_range_ = core::IntegerInterval(first_entry->first, (--data_.end())->first);
			if (!first_entry->second.empty()) {
				age_range_ = core::IntegerInterval(
					first_entry->second.begin()->first,
					(--first_entry->second.end())->first);
			}
		}
	}

	SimulationModuleType DemographicModule::type() const {
		return SimulationModuleType::Demographic;
	}

	std::string DemographicModule::name() const {
		return "Demographic";
	}

	size_t DemographicModule::get_total_population(const int time_year) const noexcept {
		auto total = 0.0f;
		if (data_.contains(time_year)) {
			auto year_data = data_.at(time_year);
			total = std::accumulate(year_data.begin(), year_data.end(), 0.0f,
				[](const float previous, const auto& element)
				{ return previous + element.second.total(); });
		}

		return (size_t)total;
	}

	std::map<int, GenderPair> DemographicModule::get_age_gender_distribution(const int time_year) const noexcept {

		std::map<int, GenderPair> result;
		if (!data_.contains(time_year)) {
			return result;
		}

		auto year_data = data_.at(time_year);
		if (!year_data.empty()) {
			double total_ratio = 1.0 / get_total_population(time_year);

			for (auto& age : year_data) {
				result.emplace(age.first, GenderPair(
					age.second.num_males * total_ratio,
					age.second.num_females * total_ratio));
			}
		}

		return result;
	}

	void DemographicModule::initialise_population(RuntimeContext& context, const int time_year) {

		auto age_gender_dist = get_age_gender_distribution(time_year);

		auto index = 0;
		int pop_size = static_cast<int>(context.population().size());
		for (auto& entry : age_gender_dist) {
			auto num_males = static_cast<int>(std::round(pop_size * entry.second.male));
			auto num_females = static_cast<int>(std::round(pop_size * entry.second.female));
			auto num_required = index + num_males + num_females;
			if (num_required > pop_size) {
				// Adjust size
				auto pop_diff = pop_size - num_required;
				num_males -= static_cast<int>(std::round(pop_diff * entry.second.male));
				num_females -= static_cast<int>(std::round(pop_diff * entry.second.female));

				pop_diff = pop_size - (index + num_males + num_females);
				if (pop_diff > 0) {
					if (entry.second.male > entry.second.female) {
						num_males -= pop_diff;
					}
					else {
						num_females -= pop_diff;
					}
				}

				num_required = index + num_males + num_females;
				pop_diff = pop_size - num_required;
				assert(pop_diff <= 1);
			}

			// [index, index + num_males)
			for (size_t i = 0; i < num_males; i++) {
				context.population()[index].age = entry.first;
				context.population()[index].gender = core::Gender::male;
				index++;
			}

			// [index + num_males, num_required)
			for (size_t i = 0; i < num_females; i++) {
				context.population()[index].age = entry.first;
				context.population()[index].gender = core::Gender::female;
				index++;
			}
		}
	}

	std::unique_ptr<DemographicModule> build_demographic_module(core::Datastore& manager, ModelInput& config) {
		// year => age [age, male, female]
		auto data = std::map<int, std::map<int, AgeRecord>>();

		auto min_time = std::min(config.start_time(), config.settings().reference_time());

		auto pop = manager.get_population(config.settings().country(), [&](const unsigned int& value) { 
			return value >= min_time && value <= config.stop_time(); });

		for (auto& item : pop) {
			data[item.year].emplace(item.age, AgeRecord(item.age, item.males, item.females));
		}

		return std::make_unique<DemographicModule>(std::move(data));
	}
}