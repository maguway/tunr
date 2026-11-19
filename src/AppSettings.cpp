#include "AppSettings.h"
#include <fstream>
#include <sstream>

SettingsData AppSettings::load(const std::string &filename) {
    SettingsData data;
    std::ifstream file(filename);
    if (!file.is_open()) return data;

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string key;
        if (!(iss >> key)) continue;

        if (key == "Port1") iss >> data.port1;
        else if (key == "Baud1") iss >> data.baud1;
        else if (key == "Port2") iss >> data.port2;
        else if (key == "Baud2") iss >> data.baud2;
        else if (key == "TrailDuration") iss >> data.trailDuration;
        else if (key == "InfiniteTrail") iss >> data.infiniteTrail;
        else if (key == "RadiusMode") iss >> data.radiusMode;
    }
    return data;
}

void AppSettings::save(const SettingsData &data, const std::string &filename) {
    std::ofstream file(filename);
    if (!file.is_open()) return;

    file << "Port1 " << data.port1 << "\n";
    file << "Baud1 " << data.baud1 << "\n";
    file << "Port2 " << data.port2 << "\n";
    file << "Baud2 " << data.baud2 << "\n";
    file << "TrailDuration " << data.trailDuration << "\n";
    file << "InfiniteTrail " << data.infiniteTrail << "\n";
    file << "RadiusMode " << data.radiusMode << "\n";
}
