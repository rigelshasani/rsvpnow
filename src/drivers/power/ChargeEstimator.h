#pragma once

#include <algorithm>
#include <cstdint>

namespace BoardDrivers {

    // Guesses whether a battery is charging from its voltage alone, for boards with no charger
    // status or USB power sense line. Feed it one reading every couple of minutes.
    //
    // Charging shows as a sharp step up when power arrives, a steady climb afterwards, or a cell
    // that stays full instead of sagging. A sharp drop or a steady decline means it stopped.
    // It is an estimate: it can lag by a few minutes and a large change in load can fool it.
    class ChargeEstimator {
    public:
        void addSample(float volts) {
            if (!hasPrevious_) {
                hasPrevious_ = true;
                previous_ = streakStart_ = peak_ = volts;
                fullSamples_ = volts >= kFullVolts ? 1 : 0;
                return;
            }
            const float delta = volts - previous_;
            if (delta >= kStepVolts) {
                if (rises_ == 0)
                    streakStart_ = previous_;
                rises_ = static_cast<uint8_t>(std::min(rises_ + 1, 250));
                falls_ = 0;
            } else if (delta <= -kStepVolts) {
                falls_ = static_cast<uint8_t>(std::min(falls_ + 1, 250));
                rises_ = 0;
            }
            fullSamples_ = volts >= kFullVolts ? static_cast<uint8_t>(std::min(fullSamples_ + 1, 250)) : 0;

            if (!charging_) {
                const bool pluggedIn = delta >= kJumpVolts;
                const bool climbing = rises_ >= 2 && volts - streakStart_ >= kClimbVolts;
                if (pluggedIn || climbing || fullSamples_ >= 2) {
                    charging_ = true;
                    peak_ = volts;
                }
            } else {
                peak_ = std::max(peak_, volts);
                if (peak_ - volts >= kDropVolts || falls_ >= 2) {
                    charging_ = false;
                    rises_ = 0;
                    fullSamples_ = 0;
                }
            }
            previous_ = volts;
        }

        bool charging() const {
            return charging_;
        }

    private:
        static constexpr float kStepVolts = 0.008f;   // Smaller changes are measurement noise.
        static constexpr float kJumpVolts = 0.060f;   // One reading to the next: power was connected.
        static constexpr float kClimbVolts = 0.025f;  // Across a run of rising readings.
        static constexpr float kDropVolts = 0.040f;   // Below the highest reading while charging.
        static constexpr float kFullVolts = 4.16f;    // A cell off the charger soon sags below this.

        float previous_ = 0.0f;
        float streakStart_ = 0.0f;
        float peak_ = 0.0f;
        uint8_t rises_ = 0;
        uint8_t falls_ = 0;
        uint8_t fullSamples_ = 0;
        bool hasPrevious_ = false;
        bool charging_ = false;
    };

} // namespace BoardDrivers
