#include <stdexcept>

#include "settings.h"

namespace hgps {

	Settings::Settings(const core::Country country, const unsigned int reference_time,
		const float size_fraction, const std::string linkage_col, const core::IntegerInterval age_range)
		: country_{country}, reference_time_{ reference_time }, size_fraction_{ size_fraction },
		linkage_column_{linkage_col}, age_range_{ age_range } {

		// TODO: Create a fraction type wrapper
		if (size_fraction <= 0.0 || size_fraction > 1.0) {
			throw std::invalid_argument(
				"Invalid population size fraction value, must be between in range (0, 1].");
		}
	}

	core::Country Settings::country() const noexcept {
		return country_;
	}

	unsigned int Settings::reference_time() const noexcept	{
		return reference_time_;
	}

	float Settings::size_fraction() const noexcept {
		return size_fraction_;
	}

	std::string Settings::linkage_column() const noexcept
	{
		return std::string();
	}

	core::IntegerInterval Settings::age_range() const noexcept {
		return age_range_;
	}
}
