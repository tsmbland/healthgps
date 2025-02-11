#include "riskfactor_adjustment.h"
#include "HealthGPS.Core/thread_util.h"
#include "baseline_sync_message.h"

#include <cmath>
#include <memory>

namespace hgps {

RiskfactorAdjustmentModel::RiskfactorAdjustmentModel(BaselineAdjustment &adjustments)
    : adjustments_{adjustments} {}

void RiskfactorAdjustmentModel::Apply(RuntimeContext &context) {
    auto &pop = context.population();
    auto coefficients = get_adjustment_coefficients(context);
    std::for_each(core::execution_policy, pop.begin(), pop.end(), [&](auto &entity) {
        if (!entity.is_active()) {
            return;
        }

        auto &table = coefficients.row(entity.gender);
        for (auto &factor : table) {
            auto current_value = entity.get_risk_factor_value(factor.first);
            auto adjustment = factor.second.at(entity.age);
            entity.risk_factors.at(factor.first) = current_value + adjustment;
        }
    });

    if (context.scenario().type() == ScenarioType::baseline) {
        context.scenario().channel().send(std::make_unique<BaselineAdjustmentMessage>(
            context.current_run(), context.time_now(), std::move(coefficients)));
    }
}

FactorAdjustmentTable
RiskfactorAdjustmentModel::get_adjustment_coefficients(RuntimeContext &context) const {
    if (context.scenario().type() == ScenarioType::baseline) {
        return calculate_adjustment_coefficients(context);
    }

    // Receive message with timeout
    auto message = context.scenario().channel().try_receive(context.sync_timeout_millis());
    if (!message.has_value()) {
        throw std::runtime_error(
            "Simulation out of sync, receive baseline adjustments message has timed out");
    }

    auto &basePtr = message.value();
    auto *messagePrt = dynamic_cast<BaselineAdjustmentMessage *>(basePtr.get());
    if (!messagePrt) {
        throw std::runtime_error(
            "Simulation out of sync, failed to receive a baseline adjustments message");
    }

    return messagePrt->data();
}

FactorAdjustmentTable
RiskfactorAdjustmentModel::calculate_adjustment_coefficients(RuntimeContext &context) const {
    const auto &age_range = context.age_range();
    auto max_age = age_range.upper() + 1;
    auto coefficients = std::map<core::Gender, std::map<core::Identifier, std::vector<double>>>{};

    auto simulated_means = calculate_simulated_mean(context.population(), age_range);
    auto &baseline_means = adjustments_.get().values;
    for (auto &gender : simulated_means) {
        coefficients.emplace(gender.first, std::map<core::Identifier, std::vector<double>>{});
        for (auto &factor : gender.second) {
            coefficients.at(gender.first).emplace(factor.first, std::vector<double>(max_age, 0.0));
            for (auto age = age_range.lower(); age <= age_range.upper(); age++) {
                auto coeff_value =
                    baseline_means.at(gender.first, factor.first).at(age) - factor.second.at(age);
                if (!std::isnan(coeff_value)) {
                    coefficients.at(gender.first).at(factor.first).at(age) = coeff_value;
                }
            }
        }
    }

    return FactorAdjustmentTable{std::move(coefficients)};
}

FactorAdjustmentTable
RiskfactorAdjustmentModel::calculate_simulated_mean(Population &population,
                                                    const core::IntegerInterval &age_range) {
    auto max_age = age_range.upper() + 1;
    auto moments = std::map<core::Gender, std::map<core::Identifier, std::vector<FirstMoment>>>{};

    moments.emplace(core::Gender::male, std::map<core::Identifier, std::vector<FirstMoment>>{});
    moments.emplace(core::Gender::female, std::map<core::Identifier, std::vector<FirstMoment>>{});
    for (const auto &entity : population) {
        if (!entity.is_active()) {
            continue;
        }

        auto &table = moments.at(entity.gender);
        for (const auto &factor : entity.risk_factors) {
            if (!table.contains(factor.first)) {
                table.emplace(factor.first, std::vector<FirstMoment>(max_age));
            }

            table.at(factor.first).at(entity.age).append(factor.second);
        }
    }

    auto means = std::map<core::Gender, std::map<core::Identifier, std::vector<double>>>{};
    for (auto &gender : moments) {
        means.emplace(gender.first, std::map<core::Identifier, std::vector<double>>{});
        for (auto &factor : gender.second) {
            means.at(gender.first).emplace(factor.first, std::vector<double>(max_age, 0.0));
            for (auto age = age_range.lower(); age <= age_range.upper(); age++) {
                means.at(gender.first).at(factor.first).at(age) = factor.second.at(age).mean();
            }
        }
    }

    return FactorAdjustmentTable{std::move(means)};
}
} // namespace hgps
