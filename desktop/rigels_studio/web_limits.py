"""Device setting limits (src/settings/SettingsModel.h BoundedValue<min, max, step>), mirrored in web/timing.js."""

LIMITS = {
    "wpm": (10, 1000, 10),
    "longWordDelayMs": (0, 600, 50),
    "complexWordDelayMs": (0, 600, 50),
    "punctuationDelayMs": (0, 600, 50),
}


def clamp_settings(settings: dict) -> dict:
    """Snap each setting to a value the reader accepts."""
    values = {}
    for name, (low, high, step) in LIMITS.items():
        value = int(round(float(settings[name]) / step) * step)
        values[name] = min(high, max(low, value))
    return values
