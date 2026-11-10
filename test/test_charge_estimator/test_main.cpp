#include <unity.h>

#include <initializer_list>

#include "drivers/power/ChargeEstimator.h"

void setUp() {}
void tearDown() {}

namespace {
    // Readings are two minutes apart, as the reader takes them.
    bool chargingAfter(std::initializer_list<float> volts) {
        BoardDrivers::ChargeEstimator estimator;
        for (const float reading: volts)
            estimator.addSample(reading);
        return estimator.charging();
    }

    void test_running_on_battery_is_not_charging() {
        TEST_ASSERT_FALSE(chargingAfter({3.92f}));
        TEST_ASSERT_FALSE(chargingAfter({3.92f, 3.92f, 3.91f, 3.91f, 3.90f, 3.89f}));
        // Measurement noise around a level.
        TEST_ASSERT_FALSE(chargingAfter({3.800f, 3.805f, 3.799f, 3.804f, 3.801f}));
        // Wi-Fi switching off lets the cell recover once; it does not keep climbing.
        TEST_ASSERT_FALSE(chargingAfter({3.76f, 3.80f, 3.80f, 3.79f}));
    }

    void test_power_arriving_shows_as_a_step_or_a_climb() {
        TEST_ASSERT_TRUE(chargingAfter({3.80f, 3.90f}));
        TEST_ASSERT_TRUE(chargingAfter({3.80f, 3.815f, 3.83f}));
        TEST_ASSERT_TRUE(chargingAfter({3.80f, 3.90f, 3.91f, 3.93f, 3.95f}));
    }

    void test_full_cell_on_the_charger_stays_charging() {
        TEST_ASSERT_TRUE(chargingAfter({4.18f, 4.18f}));
        TEST_ASSERT_TRUE(chargingAfter({4.10f, 4.17f, 4.19f, 4.19f, 4.19f, 4.19f}));
    }

    void test_unplugging_clears_it() {
        TEST_ASSERT_FALSE(chargingAfter({3.80f, 3.90f, 3.92f, 3.85f}));
        TEST_ASSERT_FALSE(chargingAfter({4.19f, 4.19f, 4.19f, 4.14f}));
        TEST_ASSERT_FALSE(chargingAfter({3.80f, 3.90f, 3.89f, 3.88f}));
    }
} // namespace

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_running_on_battery_is_not_charging);
    RUN_TEST(test_power_arriving_shows_as_a_step_or_a_climb);
    RUN_TEST(test_full_cell_on_the_charger_stays_charging);
    RUN_TEST(test_unplugging_clears_it);
    return UNITY_END();
}
