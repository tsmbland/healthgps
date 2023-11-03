#pragma once

#include "HealthGPS.Core/forward_type.h"
#include "HealthGPS.Core/identifier.h"

#include "person.h"
#include "risk_factor_model.h"
#include "runtime_context.h"

namespace hgps {

/// @brief Health GPS simulation modules types enumeration
enum class SimulationModuleType : uint8_t {
    /// @brief Risk factor module
    RiskFactor,

    /// @brief Socio-economic status module
    SES,

    /// @brief Demographic module
    Demographic,

    /// @brief Disease module coordinator
    Disease,

    /// @brief Statistical analysis module, e.g. BoD module
    Analysis,
};

/// @brief Simulation modules interface
class SimulationModule {
  public:
    /// @brief Destroys a SimulationModule instance
    virtual ~SimulationModule() = default;

    /// @brief Gets the module type identifier
    /// @return The module type identifier
    virtual SimulationModuleType type() const noexcept = 0;

    /// @brief Gets the module name
    /// @return The human-readable module name
    virtual const std::string &name() const noexcept = 0;

    /// @brief Initialises the virtual population
    /// @param context The simulation shared runtime context instance
    virtual void initialise_population(RuntimeContext &context) = 0;
};

/// @brief Generic disease module interface to host multiple diseases model
class UpdatableModule : public SimulationModule {
  public:
    /// @brief Updates the virtual population status
    /// @param context The simulation run-time context
    virtual void update_population(RuntimeContext &context) = 0;
};

/// @brief Generic disease module interface to host multiple disease models
class DiseaseHostModule : public UpdatableModule {
  public:
    /// @brief Gets the number of diseases model hosted
    /// @return Number of hosted diseases models
    virtual std::size_t size() const noexcept = 0;

    /// @brief Indicates whether the host contains an disease identified by code.
    /// @param disease_id The disease unique identifier
    /// @return true if the disease is found, otherwise false.
    virtual bool contains(const core::Identifier &disease_id) const noexcept = 0;

    /// @brief Gets the mortality rate associated with a disease for an individual
    /// @param disease_id The disease unique identifier
    /// @param entity The entity associated with the mortality value
    /// @return the mortality rate value, if found, otherwise zero.
    virtual double get_excess_mortality(const core::Identifier &disease_id,
                                        const Person &entity) const noexcept = 0;
};

/// @brief Generic risk factors module interface to host risk factor models
class RiskFactorHostModule : public UpdatableModule {
  public:
    /// @brief Gets the number of diseases model hosted
    /// @return Number of hosted diseases models
    virtual std::size_t size() const noexcept = 0;

    /// @brief Indicates whether the host contains a model of type.
    /// @param modelType The model type identifier
    /// @return true if the model is found, otherwise false.
    virtual bool contains(const RiskFactorModelType &modelType) const noexcept = 0;

    /// @brief Apply baseline risk factor adjustments to population
    /// @param context The simulation run-time context
    virtual void apply_baseline_adjustments(RuntimeContext &context) = 0;
};

/// @brief Define the population record data type for the demographic dataset
struct PopulationRecord {
    /// @brief Initialise a new instance of the PopulationRecord structure
    /// @param pop_age Age reference
    /// @param num_males Number of males
    /// @param num_females Number of females
    PopulationRecord(int pop_age, float num_males, float num_females)
        : age{pop_age}, males{num_males}, females{num_females} {}

    /// @brief Age reference in years
    int age{};

    /// @brief Number of males
    float males{};

    /// @brief NUmber of females
    float females{};

    /// @brief Gets the total number at age
    /// @return Total number for age
    float total() const noexcept { return males + females; }
};

/// @brief Demographic prospects module interface
class DemographicModule : public SimulationModule {
  public:
    /// @brief Gets the total population at a specific point in time
    /// @param time_year The reference point in time (in year)
    /// @return The respective total population size
    virtual std::size_t get_total_population_size(int time_year) const noexcept = 0;

    /// @brief Gets the population age distribution at a specific point in time
    /// @param time_year The reference point in time (in year)
    /// @return The respective population age distribution
    virtual const std::map<int, PopulationRecord> &
    get_population_distribution(int time_year) const = 0;

    /// @brief Updates the virtual population status
    /// @param context The simulation run-time context
    /// @param disease_host The diseases host module instance
    virtual void update_population(RuntimeContext &context,
                                   const DiseaseHostModule &disease_host) = 0;
};

/// @brief Diseases model interface
class DiseaseModel {
  public:
    /// @brief Destroys a DiseaseModel instance
    virtual ~DiseaseModel() = default;

    /// @brief Gets the disease group
    /// @return The disease group identifier
    virtual core::DiseaseGroup group() const noexcept = 0;

    /// @brief Gets the model disease type unique identifier
    /// @return The disease type identifier
    virtual const core::Identifier &disease_type() const noexcept = 0;

    /// @brief Initialises the population disease status.
    /// @param context The simulation run-time context
    virtual void initialise_disease_status(RuntimeContext &context) = 0;

    /// @brief Initialises the average relative risks once all diseases status were initialised.
    /// @param context The simulation run-time context
    virtual void initialise_average_relative_risk(RuntimeContext &context) = 0;

    /// @brief Updates the disease cases remission and incidence in the population
    /// @param context The simulation run-time context
    virtual void update_disease_status(RuntimeContext &context) = 0;

    /// @brief Gets the excess mortality associated with a disease for an individual
    /// @param entity The entity associated with the mortality rate
    /// @return the mortality rate value, if found, otherwise zero.
    virtual double get_excess_mortality(const Person &entity) const noexcept = 0;
};

} // namespace hgps
