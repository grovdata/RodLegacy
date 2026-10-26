/*
 * RodLegacy - Car factory: CarBuilder builds cars and parts, fits SR_data specs and decodes save-file part
 * variants.
 *
 * Created: 2025-10-20
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once

#include "core/types.h"
#include "car/engine_part.h"
#include "car/car.h"
#include "car/cardata.h"
#include "car/car_system.h"

class CarBuilder {
public:
    static CarPtr BuildCar(CarModel model);

    static CarPtr BuildRandomCar(CarModel model);

    static Part CreateTransmissionPart(TransmissionType type);
    static Part CreateManifoldPart(ManifoldType type, CarVendor_t vendor);
    static Part CreateCarburetorPart(CarburetorType type);

    // False when the save variant has no confirmed mapping, so callers can fall back explicitly.
    static bool TransmissionTypeFromSaveVariant(int variant, TransmissionType& out);
    static bool CarburetorTypeFromSaveVariant(int variant, CarburetorType& out);
    static bool EngineTypeFromSaveVariant(int variant, CarEngine_t& outType, AmigaLibAsset& outAsset);
    static bool ManifoldTypeFromSaveVariant(int variant, ManifoldType& out);
    static bool TireTypeFromSaveVariant(int variant, CarWheelType& out);

    // Newspaper cars carry fixed parts from their SR_data spec, not random ones.
    static void ApplySpec(Car* car, const SrCarSpecEntry& spec, CarVendor_t vendor);

private:
    static CarEngine_t GetDefaultEngineForVendor(CarVendor_t vendor);

    static CarEngine_t SelectCompatibleEngine(const CarModel& model);
    static TransmissionType SelectCompatibleTransmission(CarEngine_t engine);
    static ManifoldType SelectCompatibleManifold(CarEngine_t engine);
    static CarburetorType SelectCompatibleCarburetor(ManifoldType manifold);
};
