#include "configuration.h"
#include "configuration_parsing.h"
#include "jsonparser.h"
#include "version.h"

#include "HealthGPS/baseline_scenario.h"
#include "HealthGPS/fiscal_scenario.h"
#include "HealthGPS/food_labelling_scenario.h"
#include "HealthGPS/marketing_dynamic_scenario.h"
#include "HealthGPS/marketing_scenario.h"
#include "HealthGPS/mtrandom.h"
#include "HealthGPS/physical_activity_scenario.h"
#include "HealthGPS/simple_policy_scenario.h"

#include "HealthGPS.Core/poco.h"
#include "HealthGPS.Core/scoped_timer.h"

#include <chrono>
#include <fmt/chrono.h>
#include <fmt/color.h>
#include <fstream>
#include <iostream>
#include <optional>
#include <thread>
#include <utility>

#if USE_TIMER
#define MEASURE_FUNCTION()                                                                         \
    hgps::core::ScopedTimer timer { __func__ }
#else
#define MEASURE_FUNCTION()
#endif

namespace host {
using namespace hgps;
using json = nlohmann::json;

ConfigurationError::ConfigurationError(const std::string &msg) : std::runtime_error{msg} {}

Configuration get_configuration(CommandOptions &options) {
    MEASURE_FUNCTION();
    using namespace host::poco;

    bool success = true;

    Configuration config;
    config.job_id = options.job_id;

    // verbosity
    config.verbosity = core::VerboseMode::none;
    if (options.verbose) {
        config.verbosity = core::VerboseMode::verbose;
    }

    std::ifstream ifs(options.config_file, std::ifstream::in);
    if (!ifs) {
        throw ConfigurationError(
            fmt::format("File {} doesn't exist.", options.config_file.string()));
    }

    const auto opt = [&ifs]() {
        try {
            return json::parse(ifs);
        } catch (const std::exception &e) {
            throw ConfigurationError(fmt::format("Could not parse JSON: {}", e.what()));
        }
    }();

    // Check the file format version
    try {
        check_version(opt);
    } catch (const ConfigurationError &) {
        success = false;
    }

    // Base dir for relative paths
    config.root_path = options.config_file.parent_path();

    // input dataset file
    try {
        load_input_info(opt, config);
        fmt::print("Input dataset file: {}\n", config.file.name.string());
    } catch (const std::exception &e) {
        success = false;
        fmt::print(fg(fmt::color::red), "Could not load dataset file: {}\n", e.what());
    }

    // Modelling information
    try {
        load_modelling_info(opt, config);
    } catch (const std::exception &e) {
        success = false;
        fmt::print(fg(fmt::color::red), "Could not load modelling info: {}\n", e.what());
    }

    // Run-time info
    try {
        load_running_info(opt, config);
    } catch (const std::exception &e) {
        success = false;
        fmt::print(fg(fmt::color::red), "Could not load running info: {}\n", e.what());
    }

    try {
        load_output_info(opt, config);
    } catch (const ConfigurationError &) {
        success = false;
        fmt::print(fg(fmt::color::red), "Could not load output info");
    }

    if (!success) {
        throw ConfigurationError{"Error loading config file"};
    }

    return config;
}

std::vector<core::DiseaseInfo> get_diseases_info(core::Datastore &data_api, Configuration &config) {
    std::vector<core::DiseaseInfo> result;
    auto diseases = data_api.get_diseases();
    fmt::print("\nThere are {} diseases in storage, {} selected.\n", diseases.size(),
               config.diseases.size());

    result.reserve(config.diseases.size());

    for (const auto &code : config.diseases) {
        result.emplace_back(data_api.get_disease_info(code));
    }

    return result;
}

ModelInput create_model_input(core::DataTable &input_table, core::Country country,
                              const Configuration &config,
                              std::vector<core::DiseaseInfo> diseases) {
    // Create simulation configuration
    auto comorbidities = config.output.comorbidities;
    auto diseases_number = static_cast<unsigned int>(diseases.size());
    if (comorbidities > diseases_number) {
        comorbidities = diseases_number;
        fmt::print(fg(fmt::color::salmon), "Comorbidities value: {}, set to # of diseases: {}.\n",
                   config.output.comorbidities, comorbidities);
    }

    auto settings =
        Settings(std::move(country), config.settings.size_fraction, config.settings.age_range);
    auto job_custom_seed = create_job_seed(config.job_id, config.custom_seed);
    auto run_info = RunInfo{
        .start_time = config.start_time,
        .stop_time = config.stop_time,
        .sync_timeout_ms = config.sync_timeout_ms,
        .seed = job_custom_seed,
        .verbosity = config.verbosity,
        .comorbidities = comorbidities,
    };

    auto ses_mapping =
        SESDefinition{.fuction_name = config.ses.function, .parameters = config.ses.parameters};

    auto mapping = std::vector<MappingEntry>();
    for (const auto &item : config.modelling.risk_factors) {
        mapping.emplace_back(item.name, item.level, item.range);
    }

    return {input_table,
            settings,
            run_info,
            ses_mapping,
            HierarchicalMapping(std::move(mapping)),
            std::move(diseases)};
}

std::string create_output_file_name(const poco::OutputInfo &info, int job_id) {
    namespace fs = std::filesystem;

    fs::path output_folder = expand_environment_variables(info.folder);
    auto tp = std::chrono::system_clock::now();
    auto timestamp_tk = fmt::format("{0:%F_%H-%M-}{1:%S}", tp, tp.time_since_epoch());

    // filename token replacement
    auto file_name = info.file_name;
    std::size_t tk_end = 0;
    auto tk_start = file_name.find_first_of('{', tk_end);
    if (tk_start != std::string::npos) {
        tk_end = file_name.find_first_of('}', tk_start + 1);
        if (tk_end != std::string::npos) {
            auto token_str = file_name.substr(tk_start, tk_end - tk_start + 1);
            if (!core::case_insensitive::equals(token_str, "{TIMESTAMP}")) {
                throw std::logic_error(fmt::format("Unknown output file token: {}", token_str));
            }

            file_name.replace(tk_start, tk_end - tk_start + 1, timestamp_tk);
        }
    }

    auto log_file_name = fmt::format("HealthGPS_result_{}.json", timestamp_tk);
    if (tk_end > 0) {
        log_file_name = file_name;
    }

    if (job_id > 0) {
        tk_start = log_file_name.find_last_of('.');
        if (tk_start != std::string::npos) {
            log_file_name.replace(tk_start, size_t{1}, fmt::format("_{}.", std::to_string(job_id)));
        } else {
            log_file_name.append(fmt::format("_{}.json", std::to_string(job_id)));
        }
    }

    log_file_name = (output_folder / log_file_name).string();
    fmt::print(fg(fmt::color::yellow_green), "Output file: {}.\n", log_file_name);
    return log_file_name;
}

ResultFileWriter create_results_file_logger(const Configuration &config,
                                            const hgps::ModelInput &input) {
    return {create_output_file_name(config.output, config.job_id),
            ExperimentInfo{.model = config.app_name,
                           .version = config.app_version,
                           .intervention = config.active_intervention
                                               ? config.active_intervention->identifier
                                               : "",
                           .job_id = config.job_id,
                           .seed = input.seed().value_or(0u)}};
}

std::unique_ptr<hgps::Scenario> create_baseline_scenario(hgps::SyncChannel &channel) {
    return std::make_unique<BaselineScenario>(channel);
}

hgps::HealthGPS create_baseline_simulation(hgps::SyncChannel &channel,
                                           hgps::SimulationModuleFactory &factory,
                                           hgps::EventAggregator &event_bus,
                                           hgps::ModelInput &input) {
    auto baseline_rnd = std::make_unique<hgps::MTRandom32>();
    auto baseline_scenario = create_baseline_scenario(channel);
    return HealthGPS{
        SimulationDefinition{input, std::move(baseline_scenario), std::move(baseline_rnd)}, factory,
        event_bus};
}

hgps::HealthGPS create_intervention_simulation(hgps::SyncChannel &channel,
                                               hgps::SimulationModuleFactory &factory,
                                               hgps::EventAggregator &event_bus,
                                               hgps::ModelInput &input,
                                               const poco::PolicyScenarioInfo &info) {
    auto policy_scenario = create_intervention_scenario(channel, info);
    auto policy_rnd = std::make_unique<hgps::MTRandom32>();
    return HealthGPS{SimulationDefinition{input, std::move(policy_scenario), std::move(policy_rnd)},
                     factory, event_bus};
}

std::unique_ptr<hgps::InterventionScenario>
create_intervention_scenario(SyncChannel &channel, const poco::PolicyScenarioInfo &info) {
    using namespace hgps;

    fmt::print(fg(fmt::color::light_coral), "\nIntervention policy: {}.\n\n", info.identifier);
    auto period = PolicyInterval(info.active_period.start_time, info.active_period.finish_time);
    auto risk_impacts = std::vector<PolicyImpact>{};
    for (const auto &item : info.impacts) {
        risk_impacts.emplace_back(PolicyImpact{core::Identifier{item.risk_factor},
                                               item.impact_value, item.from_age, item.to_age});
    }

    // TODO: Validate intervention JSON definitions!!!
    if (info.identifier == "simple") {
        auto impact_type = PolicyImpactType::absolute;
        if (core::case_insensitive::equals(info.impact_type, "relative")) {
            impact_type = PolicyImpactType::relative;
        } else if (!core::case_insensitive::equals(info.impact_type, "absolute")) {
            throw std::logic_error(fmt::format("Unknown policy impact type: {}", info.impact_type));
        }

        auto definition = SimplePolicyDefinition(impact_type, risk_impacts, period);
        return std::make_unique<SimplePolicyScenario>(channel, std::move(definition));
    }

    if (info.identifier == "marketing") {
        auto definition = MarketingPolicyDefinition(period, risk_impacts);
        return std::make_unique<MarketingPolicyScenario>(channel, std::move(definition));
    }

    if (info.identifier == "dynamic_marketing") {
        auto dynamic = PolicyDynamic{info.dynamics};
        auto definition = MarketingDynamicDefinition{period, risk_impacts, dynamic};
        return std::make_unique<MarketingDynamicScenario>(channel, std::move(definition));
    }

    if (info.identifier == "food_labelling") {
        // I'm not sure if this is safe or not, but I'm supressing the warnings anyway -- Alex
        // NOLINTBEGIN(bugprone-unchecked-optional-access)
        const auto cutoff_time = info.coverage_cutoff_time.value();
        const auto cutoff_age = info.child_cutoff_age.value();
        // NOLINTEND(bugprone-unchecked-optional-access)

        const auto &adjustment = info.adjustments.at(0);
        auto definition = FoodLabellingDefinition{
            .active_period = period,
            .impacts = risk_impacts,
            .adjustment_risk_factor = AdjustmentFactor{adjustment.risk_factor, adjustment.value},
            .coverage = PolicyCoverage{info.coverage_rates, cutoff_time},
            .transfer_coefficient = TransferCoefficient{info.coefficients, cutoff_age}};

        return std::make_unique<FoodLabellingScenario>(channel, std::move(definition));
    }

    if (info.identifier == "physical_activity") {
        auto definition = PhysicalActivityDefinition{.active_period = period,
                                                     .impacts = risk_impacts,
                                                     .coverage_rate = info.coverage_rates.at(0)};

        return std::make_unique<PhysicalActivityScenario>(channel, std::move(definition));
    }

    if (info.identifier == "fiscal") {
        auto impact_type = parse_fiscal_impact_type(info.impact_type);
        auto definition = FiscalPolicyDefinition(impact_type, period, risk_impacts);
        return std::make_unique<FiscalPolicyScenario>(channel, std::move(definition));
    }

    throw std::invalid_argument(
        fmt::format("Unknown intervention policy identifier: {}", info.identifier));
}

#ifdef _WIN32
#pragma warning(disable : 4996)
#endif
std::string expand_environment_variables(const std::string &path) {
    if (path.find("${") == std::string::npos) {
        return path;
    }

    std::string pre = path.substr(0, path.find("${"));
    std::string post = path.substr(path.find("${") + 2);
    if (post.find('}') == std::string::npos) {
        return path;
    }

    std::string variable = post.substr(0, post.find('}'));
    std::string value;

    post = post.substr(post.find('}') + 1);
    if (const char *v = std::getenv(variable.c_str())) { // C4996, but safe here.
        value = v;
    }

    return expand_environment_variables(pre + value + post);
}

std::optional<unsigned int> create_job_seed(int job_id, std::optional<unsigned int> user_seed) {
    if (job_id > 0 && user_seed.has_value()) {
        auto rnd = hgps::MTRandom32{user_seed.value()};
        auto jump_size = static_cast<unsigned long>(1.618 * job_id * std::pow(2, 16));
        rnd.discard(jump_size);
        return rnd();
    }

    return user_seed;
}
} // namespace host
