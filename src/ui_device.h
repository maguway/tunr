#pragma once
#include "SerialPortManager.h"
#include <string>
#include <vector>

// Структура, которая хранит общие настройки и порты
struct AppState
{
    SerialPortManager serial1;
    SerialPortManager serial2;
    std::vector<std::string> cachedPorts;

    std::string selectedPortName1;
    int selectedBaudRate1 = 115200;

    std::string selectedPortName2;
    int selectedBaudRate2 = 115200;

    double lastPortRefresh = -1.0;
};

// Функция отрисовки первой вкладки
void DrawDeviceTab(AppState& state);

// Функция отрисовки верхней панели (выбор портов и кнопки)
void DrawTopBar(AppState& state);
