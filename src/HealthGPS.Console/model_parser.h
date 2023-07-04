#pragma once
#include "HealthGPS/repository.h"
#include "HealthGPS/riskfactor_adjustment_types.h"

#include "jsonparser.h"
#include "options.h"

namespace host {
/// @brief Loads baseline adjustments information from a file
/// @param info Baseline file information
/// @return An instance of the hgps::BaselineAdjustment type
hgps::BaselineAdjustment load_baseline_adjustments(const poco::BaselineInfo &info);

/// @brief Loads the full hierarchical linear regression model definition from a JSON file
/// @param model_filename The model definition file full name
/// @return An instance of the hgps::HierarchicalLinearModelDefinition type
std::shared_ptr<hgps::HierarchicalLinearModelDefinition>
load_static_risk_model_definition(const host::poco::json &opt);

/// @brief Loads the lite hierarchical linear regression model definition from a JSON file
/// @param model_filename The model definition file full name
/// @return An instance of the hgps::LiteHierarchicalModelDefinition type
std::shared_ptr<hgps::LiteHierarchicalModelDefinition>
load_dynamic_risk_model_definition(const host::poco::json &opt);

/// @brief Loads the new energy balance model definition from a JSON file
/// @param model_filename The model definition file full name
/// @return An instance of the hgps::LiteHierarchicalModelDefinition type
std::shared_ptr<hgps::EnergyBalanceModelDefinition>
load_newebm_risk_model_definition(const host::poco::json &opt);

/// @brief Registers a risk factor model definition with the repository
/// @param repository The repository instance to register
/// @param info The model definition information
/// @param settings The associated experiment settings
void register_risk_factor_model_definitions(hgps::CachedRepository &repository,
                                            const poco::ModellingInfo &info,
                                            const poco::SettingsInfo &settings);
} // namespace host
