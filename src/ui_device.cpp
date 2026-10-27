#include "ui_device.h"
#include "imgui.h"
#include <cstring>
#include <cstdio>
#include <cmath>
#include <algorithm>

static const int baudRates[] = {9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600};

void DrawDeviceTab(AppState& state)
{
    // Обновляем кэш портов раз в секунду
    double now = ImGui::GetTime();
    if (now - state.lastPortRefresh > 1.0)
    {
        state.cachedPorts = state.serial1.getAvailablePorts();
        state.lastPortRefresh = now;
        if (!state.selectedPortName1.empty() && !state.serial1.isOpen())
        {
            bool found = false;
            for (const auto& p : state.cachedPorts)
                if (p == state.selectedPortName1)
                {
                    found = true;
                    break;
                }
            if (!found) state.selectedPortName1.clear();
        }
    }

    // const int baudRates[] = {9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600};
    static std::vector<char> displayBuffer(1 << 16, 0);

    // ==========================================
    // ЛЕВАЯ КОЛОНКА (Устройство 1)
    // ==========================================
    ImGui::BeginChild("##device1", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f - 5.0f, 0),
                      ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);

    ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "DEVICE 1 (Turn Mechanism)");
    ImGui::Separator();

    /*
    // Панель управления 1
    ImGui::SetNextItemWidth(120);
    const char* preview1 = state.selectedPortName1.empty() ? "Select port..." : state.selectedPortName1.c_str();
    if (ImGui::BeginCombo("##port1", preview1))
    {
        for (int i = 0; i < (int)state.cachedPorts.size(); ++i)
        {
            bool isSelected = (state.selectedPortName1 == state.cachedPorts[i]);
            if (ImGui::Selectable(state.cachedPorts[i].c_str(), isSelected))
            {
                state.selectedPortName1 = state.cachedPorts[i];
                state.serial1.close();
                state.serial1.open(state.cachedPorts[i], state.selectedBaudRate1);
            }
            if (isSelected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80);
    if (ImGui::BeginCombo("##baud1", std::to_string(state.selectedBaudRate1).c_str()))
    {
        for (int rate : baudRates)
        {
            bool isSelected = (state.selectedBaudRate1 == rate);
            if (ImGui::Selectable(std::to_string(rate).c_str(), isSelected))
            {
                state.selectedBaudRate1 = rate;
                if (!state.selectedPortName1.empty())
                {
                    state.serial1.close();
                    state.serial1.open(state.selectedPortName1, state.selectedBaudRate1);
                }
            }
            if (isSelected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear")) state.serial1.clearBuffer();

    ImGui::SameLine();
    if (state.serial1.isRecording())
    {
        if (ImGui::Button("Stop Rec")) state.serial1.stopRecording();
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "REC");
    }
    else
    {
        if (ImGui::Button("Record")) state.serial1.startRecording("log_device1.txt");
    }

    ImGui::Text("Status: %s | Bytes: %zu", state.serial1.isOpen() ? "Conn" : "Disc",
                state.serial1.totalBytesReceived());
    */

    // Данные устройства 1
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float radius = 50.0f;
    ImVec2 center = ImVec2(p.x + radius + 10, p.y + radius + 10);
    draw_list->AddCircleFilled(center, radius, IM_COL32(50, 50, 50, 255));
    draw_list->AddCircle(center, radius, IM_COL32(200, 200, 200, 255), 0, 2.0f);
    float angle_rad = (state.serial1.getCurrentAngle() - 90.0f) * (3.14159265f / 180.0f);
    ImVec2 arrow_end = ImVec2(center.x + (radius - 10.0f) * cosf(angle_rad),
                              center.y + (radius - 10.0f) * sinf(angle_rad));
    draw_list->AddLine(center, arrow_end, IM_COL32(255, 100, 100, 255), 3.0f);

    ImGui::Dummy(ImVec2(radius * 2 + 20, radius * 2 + 20));
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::SetWindowFontScale(1.5f);
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "Angle: %.2f", state.serial1.getCurrentAngle());
    ImGui::SetWindowFontScale(1.0f);
    ImGui::Text("Raw: %u", state.serial1.getRawValue());
    ImGui::EndGroup();
    ImGui::Separator();

    // Лог устройства 1
    ImGui::BeginChild("##log1", ImVec2(0, 0), ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
    std::string displayData1 = state.serial1.getFormattedData();
    size_t copyLen1 = std::min(displayData1.size(), displayBuffer.size() - 1);
    if (copyLen1 > 0) std::memcpy(displayBuffer.data(), displayData1.data(), copyLen1);
    displayBuffer[copyLen1] = '\0';
    ImGui::TextUnformatted(displayBuffer.data());
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f) ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();

    ImGui::EndChild(); // Конец левой колонки

    ImGui::SameLine();

    // ==========================================
    // ПРАВАЯ КОЛОНКА (Устройство 2)
    // ==========================================
    ImGui::BeginChild("##device2", ImVec2(0, 0), ImGuiChildFlags_Borders);

    ImGui::TextColored(ImVec4(0.4f, 0.6f, 1.0f, 1.0f), "DEVICE 2 (Incoming Info)");
    ImGui::Separator();

    /*
    // Панель управления 2
    ImGui::SetNextItemWidth(120);
    const char* preview2 = state.selectedPortName2.empty() ? "Select port..." : state.selectedPortName2.c_str();
    if (ImGui::BeginCombo("##port2", preview2))
    {
        for (int i = 0; i < (int)state.cachedPorts.size(); ++i)
        {
            bool isSelected = (state.selectedPortName2 == state.cachedPorts[i]);
            if (ImGui::Selectable(state.cachedPorts[i].c_str(), isSelected))
            {
                state.selectedPortName2 = state.cachedPorts[i];
                state.serial2.close();
                state.serial2.open(state.cachedPorts[i], state.selectedBaudRate2);
            }
            if (isSelected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80);
    if (ImGui::BeginCombo("##baud2", std::to_string(state.selectedBaudRate2).c_str()))
    {
        for (int rate : baudRates)
        {
            bool isSelected = (state.selectedBaudRate2 == rate);
            if (ImGui::Selectable(std::to_string(rate).c_str(), isSelected))
            {
                state.selectedBaudRate2 = rate;
                if (!state.selectedPortName2.empty())
                {
                    state.serial2.close();
                    state.serial2.open(state.selectedPortName2, state.selectedBaudRate2);
                }
            }
            if (isSelected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear##2")) state.serial2.clearBuffer();

    ImGui::SameLine();
    if (state.serial2.isRecording())
    {
        if (ImGui::Button("Stop Rec##2")) state.serial2.stopRecording();
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "REC");
    }
    else
    {
        if (ImGui::Button("Record##2")) state.serial2.startRecording("log_device2.txt");
    }

    ImGui::Text("Status: %s | Bytes: %zu", state.serial2.isOpen() ? "Conn" : "Disc",
                state.serial2.totalBytesReceived());
    */
    ImGui::Separator();

    // Данные устройства 2 (стрелка + показания)
    static float vals2[6] = {0};
    std::vector<unsigned char> raw2_pre = state.serial2.getRawData();
    if (raw2_pre.size() >= 24)
    {
        size_t startIdx = raw2_pre.size() - 24;
        for (int i = 0; i < 6; ++i)
        {
            std::memcpy(&vals2[i], &raw2_pre[startIdx + (i * 4)], 4);
        }
    }

    ImDrawList* draw_list2 = ImGui::GetWindowDrawList();
    ImVec2 p2 = ImGui::GetCursorScreenPos();
    float radius2 = 50.0f;
    ImVec2 center2 = ImVec2(p2.x + radius2 + 10, p2.y + radius2 + 10);
    draw_list2->AddCircleFilled(center2, radius2, IM_COL32(50, 50, 50, 255));
    draw_list2->AddCircle(center2, radius2, IM_COL32(200, 200, 200, 255), 0, 2.0f);
    float angle_rad2 = (vals2[0] - 90.0f) * (3.14159265f / 180.0f);
    ImVec2 arrow_end2 = ImVec2(center2.x + (radius2 - 10.0f) * cosf(angle_rad2),
                               center2.y + (radius2 - 10.0f) * sinf(angle_rad2));
    draw_list2->AddLine(center2, arrow_end2, IM_COL32(100, 150, 255, 255), 3.0f);

    ImGui::Dummy(ImVec2(radius2 * 2 + 20, radius2 * 2 + 20));
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
    static std::string logDevice2;
    static size_t lastTotalBytes2 = 0;
    size_t totalBytes = state.serial2.totalBytesReceived();

    if (raw2_pre.size() >= 24 && totalBytes != lastTotalBytes2)
    {
        lastTotalBytes2 = totalBytes;
        size_t startIdx = raw2_pre.size() - 24;
        std::string hexStr;
        for (size_t j = startIdx; j < raw2_pre.size(); ++j)
        {
            char buf[8];
            snprintf(buf, sizeof(buf), "%02X ", raw2_pre[j]);
            hexStr += buf;
        }
        char parsedStr[256];
        snprintf(parsedStr, sizeof(parsedStr), " | Angle: %.2f, X: %.2f, Y: %.2f, Dist: %.2f, Power: %.2e, Res: %.2f",
                 vals2[0], vals2[1], vals2[2], vals2[3], vals2[4], vals2[5]);
        logDevice2 += hexStr + parsedStr + "\n";

        if (state.serial2.isRecording()) state.serial2.writeLogLine(hexStr + parsedStr);

        const size_t maxLogSize = 256 * 1024;
        if (logDevice2.size() > maxLogSize)
        {
            size_t cutPos = logDevice2.find('\n', logDevice2.size() - maxLogSize);
            if (cutPos != std::string::npos) logDevice2.erase(0, cutPos + 1);
            else logDevice2.clear();
        }
    }
    ImGui::TextUnformatted(logDevice2.c_str());
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f) ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();

    ImGui::EndChild(); // Конец правой колонки
}

void DrawTopBar(AppState& state)
{
    // Разделяем экран на две половины для двух устройств
    // float halfWidth = ImGui::GetContentRegionAvail().x * 0.5f - 5.0f;

    // --- Левая половина (Устройство 1) ---
    ImGui::BeginGroup();
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "Dev 1:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    const char* preview1 = state.selectedPortName1.empty() ? "Select port..." : state.selectedPortName1.c_str();
    if (ImGui::BeginCombo("##port1", preview1))
    {
        for (int i = 0; i < (int)state.cachedPorts.size(); ++i)
        {
            bool isSelected = (state.selectedPortName1 == state.cachedPorts[i]);
            if (ImGui::Selectable(state.cachedPorts[i].c_str(), isSelected))
            {
                state.selectedPortName1 = state.cachedPorts[i];
                state.serial1.close();
                state.serial1.open(state.selectedPortName1, state.selectedBaudRate1);
            }
            if (isSelected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80);
    if (ImGui::BeginCombo("##baud1", std::to_string(state.selectedBaudRate1).c_str()))
    {
        for (int rate : baudRates)
        {
            bool isSelected = (state.selectedBaudRate1 == rate);
            if (ImGui::Selectable(std::to_string(rate).c_str(), isSelected))
            {
                state.selectedBaudRate1 = rate;
                if (!state.selectedPortName1.empty())
                {
                    state.serial1.close();
                    state.serial1.open(state.selectedPortName1, state.selectedBaudRate1);
                }
            }
            if (isSelected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (state.serial1.isRecording())
    {
        if (ImGui::Button("Stop##1")) state.serial1.stopRecording();
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "REC");
    }
    else
    {
        if (ImGui::Button("Rec##1")) state.serial1.startRecording("log_device1.txt");
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear##1")) state.serial1.clearBuffer();
    ImGui::SameLine();
    ImGui::Text("(%s)", state.serial1.isOpen() ? "Conn" : "Disc");
    ImGui::EndGroup();

    ImGui::SameLine();
    ImGui::Dummy(ImVec2(10, 0)); // Отступ между устройствами
    ImGui::SameLine();

    // --- Правая половина (Устройство 2) ---
    ImGui::BeginGroup();
    ImGui::TextColored(ImVec4(0.4f, 0.6f, 1.0f, 1.0f), "Dev 2:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    const char* preview2 = state.selectedPortName2.empty() ? "Select port..." : state.selectedPortName2.c_str();
    if (ImGui::BeginCombo("##port2", preview2))
    {
        for (int i = 0; i < (int)state.cachedPorts.size(); ++i)
        {
            bool isSelected = (state.selectedPortName2 == state.cachedPorts[i]);
            if (ImGui::Selectable(state.cachedPorts[i].c_str(), isSelected))
            {
                state.selectedPortName2 = state.cachedPorts[i];
                state.serial2.close();
                state.serial2.open(state.selectedPortName2, state.selectedBaudRate2);
            }
            if (isSelected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80);
    if (ImGui::BeginCombo("##baud2", std::to_string(state.selectedBaudRate2).c_str()))
    {
        for (int rate : baudRates)
        {
            bool isSelected = (state.selectedBaudRate2 == rate);
            if (ImGui::Selectable(std::to_string(rate).c_str(), isSelected))
            {
                state.selectedBaudRate2 = rate;
                if (!state.selectedPortName2.empty())
                {
                    state.serial2.close();
                    state.serial2.open(state.selectedPortName2, state.selectedBaudRate2);
                }
            }
            if (isSelected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (state.serial2.isRecording())
    {
        if (ImGui::Button("Stop##2")) state.serial2.stopRecording();
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "REC");
    }
    else
    {
        if (ImGui::Button("Rec##2")) state.serial2.startRecording("log_device2.txt");
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear##2")) state.serial2.clearBuffer();
    ImGui::SameLine();
    ImGui::Text("(%s)", state.serial2.isOpen() ? "Conn" : "Disc");
    ImGui::EndGroup();

    ImGui::Separator();
}
