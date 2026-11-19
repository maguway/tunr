#pragma once
#include "SerialPortManager.h"
#include "SessionLogger.h"
#include <string>
#include <vector>

// Структура, которая хранит общие настройки и порты
struct AppState
{
    // ВАЖНО: логгер объявлен ПЕРВЫМ. Члены уничтожаются в обратном порядке, поэтому
    // он переживёт оба порта, и поток чтения никогда не обратится к уже разрушенному логгеру.
    SessionLogger logger;

    SerialPortManager serial1;
    SerialPortManager serial2;
    std::vector<std::string> cachedPorts;

    std::string selectedPortName1;
    int selectedBaudRate1 = 115200;

    std::string selectedPortName2;
    int selectedBaudRate2 = 115200;

    double lastPortRefresh = -1.0;

    // Состояние окна Dev2 (раньше хранилось в static-переменных внутри DrawDeviceTab)
    float dev2Vals[6] = {0, 0, 0, 0, 0, 0}; // Angle, X, Y, Dist, Power, Res
    std::string dev2Log;                    // текст лога Dev2 в окне
    size_t dev2LastTotalBytes = 0;
};

// Функция отрисовки первой вкладки
void DrawDeviceTab(AppState& state);

// Функция отрисовки верхней панели (выбор портов и кнопки)
void DrawTopBar(AppState& state);
