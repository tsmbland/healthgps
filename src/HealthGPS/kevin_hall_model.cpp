#include "kevin_hall_model.h"
#include "runtime_context.h"

#include "HealthGPS.Core/exception.h"

#include <utility>

/*
 * Suppress this clang-tidy warning for now, because most (all?) of these methods won't
 * be suitable for converting to statics once the model is finished.
 */
// NOLINTBEGIN(readability-convert-member-functions-to-static)
namespace hgps {

KevinHallModel::KevinHallModel(
    const std::unordered_map<core::Identifier, double> &energy_equation,
    const std::unordered_map<core::Identifier, core::DoubleInterval> &nutrient_ranges,
    const std::unordered_map<core::Identifier, std::map<core::Identifier, double>>
        &nutrient_equations,
    const std::unordered_map<core::Identifier, std::optional<double>> &food_prices,
    const std::unordered_map<core::Gender, std::vector<double>> &age_mean_height)
    : energy_equation_{energy_equation}, nutrient_ranges_{nutrient_ranges},
      nutrient_equations_{nutrient_equations}, food_prices_{food_prices},
      age_mean_height_{age_mean_height} {

    if (energy_equation_.empty()) {
        throw core::HgpsException("Energy equation mapping is empty");
    }
    if (nutrient_ranges_.empty()) {
        throw core::HgpsException("Nutrient range mapping is empty");
    }
    if (nutrient_equations_.empty()) {
        throw core::HgpsException("Nutrient equation mapping is empty");
    }
    if (food_prices_.empty()) {
        throw core::HgpsException("Food price mapping is empty");
    }
    if (age_mean_height_.empty()) {
        throw core::HgpsException("Age mean height mapping is empty");
    }
}

RiskFactorModelType KevinHallModel::type() const noexcept { return RiskFactorModelType::Dynamic; }

std::string KevinHallModel::name() const noexcept { return "Dynamic"; }

void KevinHallModel::generate_risk_factors(RuntimeContext &context) {

    // Initialise everyone.
    for (auto &person : context.population()) {
        initialise_nutrient_intakes(person);
        initialise_energy_intake(person);
    }
}

void KevinHallModel::update_risk_factors(RuntimeContext &context) {

    // Initialise newborns and update others.
    for (auto &person : context.population()) {
        // Ignore if inactive.
        if (!person.is_active()) {
            continue;
        }

        if (person.age == 0) {
            initialise_nutrient_intakes(person);
            initialise_energy_intake(person);
        } else {
            update_nutrient_intakes(person);
            update_energy_intake(person);
        }
    }

    // TODO: Compute target body weight.
    const float target_BW = 100.0;

    // Trial run.
    double mean_sim_body_weight = 0.0;
    double mean_adjustment_coefficient = 0.0;
    for (auto &person : context.population()) {
        // Ignore if inactive.
        if (!person.is_active()) {
            continue;
        }

        // Simulate person and compute adjustment coefficient.
        SimulatePersonState state = simulate_person(person, 0.0);
        mean_sim_body_weight += state.BW;
        mean_adjustment_coefficient += state.adjust;
    }

    // Compute model adjustment term.
    const size_t population_size = context.population().current_active_size();
    mean_sim_body_weight /= population_size;
    mean_adjustment_coefficient /= population_size;
    double shift = (target_BW - mean_sim_body_weight) / mean_adjustment_coefficient;

    // Final run.
    for (auto &person : context.population()) {
        // Ignore if inactive.
        if (!person.is_active()) {
            continue;
        }

        // TODO: Simulate person and update risk factors.
        simulate_person(person, shift);
        // SimulatePersonState state = simulate_person(person, shift);
        // person.risk_factors["Height"_id] = state.H;
        // person.risk_factors["Weight"_id] = state.BW;
        // person.risk_factors["PhysicalActivity"_id] = state.PAL;
        // person.risk_factors["RestingMetabolicRate"_id] = state.RMR;
        // person.risk_factors["BodyFat"_id] = state.F;
        // person.risk_factors["LeanTissue"_id] = state.L;
        // person.risk_factors["ExtracellularFluid"_id] = state.ECF;
        // person.risk_factors["Glycogen"_id] = state.G;
        // person.risk_factors["EnergyExpenditure"_id] = state.EE;
        // person.risk_factors["EnergyIntake"_id] = state.EI;
    }
}

void KevinHallModel::initialise_nutrient_intakes(Person &person) const {
    auto nutrient_intakes = compute_nutrient_intakes(person);

    // Initialise nutrient intakes.
    for (auto &[nutrient_key, nutrient_value] : nutrient_intakes) {
        person.risk_factors[nutrient_key] = nutrient_value;
    }

    // TODO: set old value to new value (only some nutrients)
}

void KevinHallModel::update_nutrient_intakes(Person &person) const {
    auto nutrient_intakes = compute_nutrient_intakes(person);

    // Update nutrient intakes.
    for (auto &[nutrient_key, nutrient_value] : nutrient_intakes) {
        person.risk_factors[nutrient_key] = nutrient_value;
    }

    // TODO: set old value to previous value (only some nutrients)
}

std::unordered_map<core::Identifier, double>
KevinHallModel::compute_nutrient_intakes(Person &person) const {
    std::unordered_map<core::Identifier, double> nutrient_intakes;

    for (const auto &[food_key, nutrient_coefficients] : nutrient_equations_) {
        double food_intake = person.get_risk_factor_value(food_key);
        for (const auto &[nutrient_key, nutrient_coefficient] : nutrient_coefficients) {
            nutrient_intakes[nutrient_key] += food_intake * nutrient_coefficient;
        }
    }

    return nutrient_intakes;
}

void KevinHallModel::initialise_energy_intake(Person &person) const {
    double energy_intake = compute_energy_intake(person);
    person.risk_factors["EnergyIntake"_id] = energy_intake;

    // Begin at steady state (EI = EE).
    person.risk_factors["EnergyExpenditure"_id] = energy_intake;

    // TODO: set old value to new value
}

void KevinHallModel::update_energy_intake(Person &person) const {
    double energy_intake = compute_energy_intake(person);
    person.risk_factors["EnergyIntake"_id] = energy_intake;

    // TODO: set old value to previous value
}

double KevinHallModel::compute_energy_intake(Person &person) const {
    double energy_intake = 0.0;

    for (const auto &[nutrient_key, energy_coefficient] : energy_equation_) {
        double nutrient_intake = person.get_risk_factor_value(nutrient_key);
        energy_intake += nutrient_intake * energy_coefficient;
    }

    return energy_intake;
}

SimulatePersonState KevinHallModel::simulate_person(Person &person, double shift) const {
    // Initial simulated person state.
    const double H_0 = person.get_risk_factor_value("Height"_id);
    const double BW_0 = person.get_risk_factor_value("Weight"_id);
    const double PAL_0 = person.get_risk_factor_value("PhysicalActivity"_id);
    const double F_0 = person.get_risk_factor_value("BodyFat"_id);
    const double L_0 = person.get_risk_factor_value("LeanTissue"_id);
    const double ECF_0 = person.get_risk_factor_value("ExtracellularFluid"_id);
    const double G_0 = person.get_risk_factor_value("Glycogen"_id);
    const double EI_0 = person.get_risk_factor_value("EnergyIntake"_id);
    const double CI_0 = person.get_risk_factor_value("Carbohydrate"_id);

    // TODO: Compute height.
    double H = H_0;

    // TODO: Compute physical activity level.
    double PAL = PAL_0;

    // TODO: some nutrient values need saving from previous year.

    // Updated energy intake and carbohydrate intake.
    // TODO: above, set EI_0 and CI_0 to risk_factors[*_previous].
    double EI = person.get_risk_factor_value("EnergyIntake"_id);
    double CI = person.get_risk_factor_value("Carbohydrate"_id);

    // Compute glycogen and water.
    double G = compute_G(CI, CI_0, G_0);
    double W = compute_W(G);

    // Compute extracellular fluid.
    double ECF = compute_ECF(EI, EI_0, CI, CI_0, ECF_0);

    // Compute energy cost per unit body weight.
    double delta = compute_delta(PAL, BW_0, H, person.age, person.gender);

    // Compute thermic effect of food.
    double TEF = compute_TEF(EI, EI_0);

    // Compute adaptive thermogenesis.
    double AT = compute_AT(EI, EI_0);

    // TODO: Compute intercept value.
    const double K = 0.0; // TODO: Intercept value.

    // Energy partitioning.
    const double C = 10.4 * rho_L / rho_F;
    const double p = C / (C + F_0);

    // First equation ax + by = e.
    double a1 = p * rho_F;
    double b1 = -(1.0 - p) * rho_L;
    double c1 = p * rho_F * F_0 - (1 - p) * rho_L * L_0;

    // Second equation cx + dy = f.
    double a2 = gamma_F + delta;
    double b2 = gamma_L + delta;
    double c2 = EI - shift - K - TEF - AT - delta * (G + W + ECF);

    // Compute body fat and lean tissue steady state.
    double steady_F = -(b1 * c2 - b2 * c1) / (a1 * b2 - a2 * b1);
    double steady_L = -(c1 * a2 - c2 * a1) / (a1 * b2 - a2 * b1);

    double x = p * eta_L / rho_L + (1.0 - p) * eta_F / rho_F;

    // Compute time constant.
    double tau = rho_L * rho_F * (1.0 + x) /
                 ((gamma_F + delta) * (1.0 - p) * rho_L + (gamma_L + delta) * p * rho_F);

    // Compute body fat and lean tissue.
    double F = steady_F - (steady_F - F_0) * exp(-365.0 / tau);
    double L = steady_L - (steady_L - L_0) * exp(-365.0 / tau);

    // Compute adjustment coefficient.
    double adjust = -(a1 - b1) * (1.0 - exp(-365.0 / tau)) / (a1 * b2 - a2 * b1);

    // Compute body weight.
    double BW = F + L + G + W + ECF;

    // Compute energy expenditure.
    double EE =
        (shift + K + gamma_F * F + gamma_L * L + delta * BW + TEF + AT + EI * x) / (1.0 + x);

    // New simulated person state.
    return SimulatePersonState{.H = H,
                               .BW = BW,
                               .PAL = PAL,
                               .F = F,
                               .L = L,
                               .ECF = ECF,
                               .G = G,
                               .EE = EE,
                               .EI = EI,
                               .adjust = adjust};
}

double KevinHallModel::compute_G(double CI, double CI_0, double G_0) const {
    double k_G = CI_0 / (G_0 * G_0);
    return sqrt(CI / k_G);
}

double KevinHallModel::compute_W(double G) const { return 2.7 * G; }

double KevinHallModel::compute_ECF(double EI, double EI_0, double CI, double CI_0,
                                   double ECF_0) const {
    double Na_b = 4000.0;
    double Na_f = Na_b * EI / EI_0;
    double Delta_Na_diet = Na_f - Na_b;
    return ECF_0 + (Delta_Na_diet - xi_CI * (1.0 - CI / CI_0)) / xi_Na;
}

double KevinHallModel::compute_delta(double PAL, double BW, double H, unsigned int age,
                                     core::Gender gender) const {
    // Resting metabolic rate (Mifflin-St Jeor).
    double RMR = 9.99 * BW + 6.25 * H * 100.0 - 4.92 * age;
    RMR += gender == core::Gender::male ? 5.0 : -161.0;
    RMR *= 4.184; // From kcal to kJ

    // Energy expenditure per kg of body weight.
    return ((1.0 - beta_TEF) * PAL - 1.0) * RMR / BW;
}

double KevinHallModel::compute_TEF(double EI, double EI_0) const {
    double delta_EI = EI - EI_0;
    return beta_TEF * delta_EI;
}

double KevinHallModel::compute_AT(double EI, double EI_0) const {
    double delta_EI = EI - EI_0;
    return beta_AT * delta_EI;
}

KevinHallModelDefinition::KevinHallModelDefinition(
    std::unordered_map<core::Identifier, double> energy_equation,
    std::unordered_map<core::Identifier, core::DoubleInterval> nutrient_ranges,
    std::unordered_map<core::Identifier, std::map<core::Identifier, double>> nutrient_equations,
    std::unordered_map<core::Identifier, std::optional<double>> food_prices,
    std::unordered_map<core::Gender, std::vector<double>> age_mean_height)
    : energy_equation_{std::move(energy_equation)}, nutrient_ranges_{std::move(nutrient_ranges)},
      nutrient_equations_{std::move(nutrient_equations)}, food_prices_{std::move(food_prices)},
      age_mean_height_{std::move(age_mean_height)} {

    if (energy_equation_.empty()) {
        throw core::HgpsException("Energy equation mapping is empty");
    }
    if (nutrient_ranges_.empty()) {
        throw core::HgpsException("Nutrient ranges mapping is empty");
    }
    if (nutrient_equations_.empty()) {
        throw core::HgpsException("Nutrient equation mapping is empty");
    }
    if (food_prices_.empty()) {
        throw core::HgpsException("Food prices mapping is empty");
    }
    if (age_mean_height_.empty()) {
        throw core::HgpsException("Age mean height mapping is empty");
    }
}

std::unique_ptr<RiskFactorModel> KevinHallModelDefinition::create_model() const {
    return std::make_unique<KevinHallModel>(energy_equation_, nutrient_ranges_, nutrient_equations_,
                                            food_prices_, age_mean_height_);
}

} // namespace hgps
// NOLINTEND(readability-convert-member-functions-to-static)
