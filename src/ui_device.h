#pragma once
#include "SerialPortManager.h"
#include "SessionLogger.h"
#include "AppSettings.h" // <-- ДОБАВИТЬ
#include <string>
#include <vector>

struct AppState {
    SessionLogger logger;
    SerialPortManager serial1;
    SerialPortManager serial2;
    std::vector<std::string> cachedPorts;

    std::string selectedPortName1;
    int selectedBaudRate1 = 115200;
    std::string selectedPortName2;
    int selectedBaudRate2 = 115200;
    double lastPortRefresh = -1.0;

    // --- НОВЫЕ ПОЛЯ ДЛЯ СОХРАНЕНИЯ НАСТРОЕК ГРАФИКИ ---
    float trailDuration = 1.5f;
    bool infiniteTrail = false;
    int radiusMode = 0;

    // Состояние окна Dev2
    float dev2Vals[6] = {0, 0, 0, 0, 0, 0};
    std::string dev2Log;
    size_t dev2LastTotalBytes = 0;
};

void DrawDeviceTab(AppState &state);

void DrawTopBar(AppState &state);
