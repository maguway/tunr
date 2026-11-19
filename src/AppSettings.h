#pragma once
#include <string>

struct SettingsData {
    std::string port1;
    int baud1 = 115200;
    std::string port2;
    int baud2 = 115200;
    float trailDuration = 1.5f;
    bool infiniteTrail = false;
    int radiusMode = 0;
};

class AppSettings {
public:
    static SettingsData load(const std::string &filename = "turn_settings.ini");

    static void save(const SettingsData &data, const std::string &filename = "turn_settings.ini");
};
