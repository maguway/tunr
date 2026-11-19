#include "ui_device.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

static const int baudRates[] = {9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600};

static const ImVec4 COL_DEV1 = ImVec4(0.4f, 0.8f, 0.4f, 1.0f);
static const ImVec4 COL_DEV2 = ImVec4(0.4f, 0.6f, 1.0f, 1.0f);
static const ImVec4 COL_REC = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);

// Обновляем кэш портов раз в секунду.
// Вызывается из DrawTopBar, поэтому работает на ЛЮБОЙ вкладке (раньше - только на Monitor).
static void RefreshPortCache(AppState &state) {
    const double now = ImGui::GetTime();
    if (now - state.lastPortRefresh <= 1.0) return;

    state.cachedPorts = state.serial1.getAvailablePorts();
    state.lastPortRefresh = now;

    // Если выбранный порт исчез из системы (и порт уже не открыт/связь потеряна),
    // освобождаем его и сбрасываем выбор. Теперь это делается для ОБОИХ устройств.
    auto dropIfGone = [&](SerialPortManager &serial, std::string &portName) {
        if (portName.empty() || serial.isOpen()) return;
        const bool found = std::find(state.cachedPorts.begin(), state.cachedPorts.end(), portName)
                           != state.cachedPorts.end();
        if (!found) {
            serial.close();
            portName.clear();
        }
    };
    dropIfGone(state.serial1, state.selectedPortName1);
    dropIfGone(state.serial2, state.selectedPortName2);
}

// Круглый индикатор со стрелкой (угол в градусах, 0 - вертикаль вверх, по часовой стрелке).
static void DrawDial(float angleDeg, ImU32 arrowColor) {
    ImDrawList *draw_list = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    const float radius = 50.0f;
    ImVec2 center = ImVec2(p.x + radius + 10, p.y + radius + 10);
    draw_list->AddCircleFilled(center, radius, IM_COL32(50, 50, 50, 255));
    draw_list->AddCircle(center, radius, IM_COL32(200, 200, 200, 255), 0, 2.0f);
    if (!std::isfinite(angleDeg)) angleDeg = 0.0f; // мусорные байты не должны ломать отрисовку
    const float angle_rad = (angleDeg - 90.0f) * (3.14159265f / 180.0f);
    ImVec2 arrow_end = ImVec2(center.x + (radius - 10.0f) * cosf(angle_rad),
                              center.y + (radius - 10.0f) * sinf(angle_rad));
    draw_list->AddLine(center, arrow_end, arrowColor, 3.0f);
    ImGui::Dummy(ImVec2(radius * 2 + 20, radius * 2 + 20));
}

// Красный кружок + текст "REC" (символ "●" отсутствует в шрифте ImGui по умолчанию и рисовался как "?").
static void DrawRecIndicator() {
    const float h = ImGui::GetTextLineHeight();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(pos.x + h * 0.5f, pos.y + h * 0.5f), h * 0.3f,
                                                IM_COL32(255, 77, 77, 255));
    ImGui::Dummy(ImVec2(h, h));
    ImGui::SameLine(0.0f, 0.0f);
    ImGui::TextColored(COL_REC, "REC");
}

void DrawDeviceTab(AppState &state) {
    RefreshPortCache(state);

    // ==========================================
    // ЛЕВАЯ КОЛОНКА (Устройство 1)
    // ==========================================
    ImGui::BeginChild("##device1", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f - 5.0f, 0),
                      ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);

    ImGui::TextColored(COL_DEV1, "DEVICE 1 (Turn Mechanism)");
    ImGui::Separator();

    // Данные устройства 1
    const float angle1 = state.serial1.getCurrentAngle();
    DrawDial(angle1, IM_COL32(255, 100, 100, 255));
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::SetWindowFontScale(1.5f);
    ImGui::TextColored(COL_DEV1, "Angle: %.2f", angle1);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::Text("Raw: %u", state.serial1.getRawValue());
    ImGui::EndGroup();
    ImGui::Separator();

    // Лог устройства 1
    ImGui::BeginChild("##log1", ImVec2(0, 0), ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
    const std::string displayData1 = state.serial1.getFormattedData();
    ImGui::TextUnformatted(displayData1.c_str(), displayData1.c_str() + displayData1.size());
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f) ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();

    ImGui::EndChild(); // Конец левой колонки

    ImGui::SameLine();

    // ==========================================
    // ПРАВАЯ КОЛОНКА (Устройство 2)
    // ==========================================
    ImGui::BeginChild("##device2", ImVec2(0, 0), ImGuiChildFlags_Borders);

    ImGui::TextColored(COL_DEV2, "DEVICE 2 (Incoming Info)");
    ImGui::Separator();
    ImGui::Separator();

    // Данные устройства 2 (стрелка + показания).
    // Берём только последние 24 байта (одна короткая копия), а не весь накопленный буфер.
    float *vals2 = state.dev2Vals;
    const std::vector<unsigned char> raw2 = state.serial2.getTailBytes(24);
    if (raw2.size() >= 24) {
        for (int i = 0; i < 6; ++i) {
            std::memcpy(&vals2[i], &raw2[i * 4], 4);
        }
    }

    DrawDial(vals2[0], IM_COL32(100, 150, 255, 255));
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::SetWindowFontScale(1.2f);
    ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "Angle: %.2f deg", vals2[0]);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::Text("X: %.4f", vals2[1]);
    ImGui::Text("Y: %.4f", vals2[2]);
    ImGui::Text("Dist: %.4f", vals2[3]);
    ImGui::Text("Power: %.2e", vals2[4]);
    ImGui::EndGroup();
    ImGui::Separator();

    // Лог устройства 2
    ImGui::BeginChild("##log2", ImVec2(0, 0), ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
    const size_t totalBytes = state.serial2.totalBytesReceived();

    if (raw2.size() >= 24 && totalBytes != state.dev2LastTotalBytes) {
        state.dev2LastTotalBytes = totalBytes;
        std::string hexStr;
        for (size_t j = 0; j < raw2.size(); ++j) {
            char buf[8];
            snprintf(buf, sizeof(buf), "%02X ", raw2[j]);
            hexStr += buf;
        }
        char parsedStr[256];
        snprintf(parsedStr, sizeof(parsedStr), " | Angle: %.2f, X: %.2f, Y: %.2f, Dist: %.2f, Power: %.2e, Res: %.2f",
                 vals2[0], vals2[1], vals2[2], vals2[3], vals2[4], vals2[5]);
        state.dev2Log += hexStr + parsedStr + "\n";

        if (state.serial2.isRecording()) state.serial2.writeLogLine(hexStr + parsedStr);

        const size_t maxLogSize = 256 * 1024;
        if (state.dev2Log.size() > maxLogSize) {
            size_t cutPos = state.dev2Log.find('\n', state.dev2Log.size() - maxLogSize);
            if (cutPos != std::string::npos) state.dev2Log.erase(0, cutPos + 1);
            else state.dev2Log.clear();
        }
    }
    ImGui::TextUnformatted(state.dev2Log.c_str(), state.dev2Log.c_str() + state.dev2Log.size());
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f) ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();

    ImGui::EndChild(); // Конец правой колонки
}

// Блок управления одним устройством: выбор порта, скорость, Rec/Stop, Clear, статус.
// Идентификаторы ImGui (##port1, ##baud1, Rec##1, Stop##1, Clear##1 и т.д.) те же, что и раньше.
// uiLogToClear - необязательный текст лога в окне, который тоже очищается кнопкой Clear.
static void DrawPortControls(AppState &state, int idx, const char *title, const ImVec4 &color,
                             SerialPortManager &serial, std::string &portName, int &baudRate,
                             const char *recFile, std::string *uiLogToClear) {
    char id[32];

    ImGui::BeginGroup();
    ImGui::TextColored(color, "%s", title);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    const char *preview = portName.empty() ? "Select port..." : portName.c_str();
    std::snprintf(id, sizeof(id), "##port%d", idx);
    if (ImGui::BeginCombo(id, preview)) {
        for (int i = 0; i < (int) state.cachedPorts.size(); ++i) {
            bool isSelected = (portName == state.cachedPorts[i]);
            if (ImGui::Selectable(state.cachedPorts[i].c_str(), isSelected)) {
                portName = state.cachedPorts[i];
                serial.close();
                serial.open(portName, baudRate);
            }
            if (isSelected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80);
    std::snprintf(id, sizeof(id), "##baud%d", idx);
    if (ImGui::BeginCombo(id, std::to_string(baudRate).c_str())) {
        for (int rate: baudRates) {
            bool isSelected = (baudRate == rate);
            if (ImGui::Selectable(std::to_string(rate).c_str(), isSelected)) {
                baudRate = rate;
                if (!portName.empty()) {
                    serial.close();
                    serial.open(portName, baudRate);
                }
            }
            if (isSelected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();

    // Старая кнопка Rec для устройства (оставим для совместимости)
    if (serial.isRecording()) {
        std::snprintf(id, sizeof(id), "Stop##%d", idx);
        if (ImGui::Button(id)) serial.stopRecording();
        ImGui::SameLine();
        ImGui::TextColored(COL_REC, "REC");
    } else {
        std::snprintf(id, sizeof(id), "Rec##%d", idx);
        if (ImGui::Button(id)) serial.startRecording(recFile);
    }
    ImGui::SameLine();

    // Очистка буфера и окна лога устройства
    std::snprintf(id, sizeof(id), "Clear##%d", idx);
    if (ImGui::Button(id)) {
        serial.clearBuffer();
        if (uiLogToClear) uiLogToClear->clear();
    }
    ImGui::SameLine();

    const char *status = serial.isOpen() ? "Conn" : (serial.connectionLost() ? "Lost" : "Disc");
    ImGui::Text("(%s)", status);
    if (!serial.isOpen() && ImGui::IsItemHovered()) {
        const std::string err = serial.lastError();
        if (!err.empty()) ImGui::SetTooltip("%s", err.c_str());
    }
    ImGui::EndGroup();
}

void DrawTopBar(AppState &state) {
    // Список портов обновляется здесь, поэтому он актуален на любой вкладке
    RefreshPortCache(state);

    // --- Левая половина (Устройство 1) ---
    DrawPortControls(state, 1, "Dev 1:", COL_DEV1, state.serial1, state.selectedPortName1,
                     state.selectedBaudRate1, "log_device1.txt", nullptr);

    ImGui::SameLine();
    ImGui::Dummy(ImVec2(10, 0)); // Отступ между устройствами
    ImGui::SameLine();

    // --- Правая половина (Устройство 2) ---
    DrawPortControls(state, 2, "Dev 2:", COL_DEV2, state.serial2, state.selectedPortName2,
                     state.selectedBaudRate2, "log_device2.txt", &state.dev2Log);

    // --- Общая запись сессии (вне групп устройств) ---
    ImGui::SameLine();
    ImGui::Dummy(ImVec2(20, 0)); // Отступ
    ImGui::SameLine();

    if (state.logger.isRecording()) {
        if (ImGui::Button("Stop Session", ImVec2(120, 0))) {
            state.logger.stop();
        }
        ImGui::SameLine();
        DrawRecIndicator();
    } else {
        if (ImGui::Button("Record Session", ImVec2(120, 0))) {
            state.logger.start("session_log.csv");
        }
        // Если файл не удалось открыть или запись оборвалась - показываем причину
        const std::string err = state.logger.lastError();
        if (!err.empty()) {
            ImGui::SameLine();
            ImGui::TextColored(COL_REC, "%s", err.c_str());
        }
    }

    ImGui::Separator();
}
