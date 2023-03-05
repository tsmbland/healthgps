#pragma once
#include "HealthGPS/repository.h"
#include "HealthGPS/hierarchical_model_types.h"
#include "HealthGPS/riskfactor_adjustment_types.h"

#include "options.h"

namespace host
{
	/// @brief Loads baseline adjustments information from a file
	/// @param info Baseline file information
	/// @return An instance of the hgps::BaselineAdjustment type
	hgps::BaselineAdjustment load_baseline_adjustments(const poco::BaselineInfo& info);

	/// @brief Loads the full hierarchical linear regression model definition from a JSON file
	/// @param model_filename The model definition file full name
	/// @return An instance of the hgps::HierarchicalLinearModelDefinition type
	hgps::HierarchicalLinearModelDefinition load_static_risk_model_definition(std::string model_filename);

	/// @brief Loads the lite hierarchical linear regression model definition from a JSON file
	/// @param model_filename The model definition file full name
	/// @return An instance of the hgps::LiteHierarchicalModelDefinition type
	hgps::LiteHierarchicalModelDefinition load_dynamic_risk_model_info(std::string model_filename);

	/// @brief Registers a risk factor model definition with the repository
	/// @param repository The repository instance to register
	/// @param info The model definition information
	/// @param settings The associated experiment settings
	void register_risk_factor_model_definitions(hgps::CachedRepository& repository,
		const poco::ModellingInfo& info, const poco::SettingsInfo& settings);
}