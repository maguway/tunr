#include "ui_graph.h"
#include "imgui.h"
#include <cmath>
#include <cstring>
#include <algorithm>

void DrawGraphTab(AppState& state)
{
    // Получаем текущие углы
    float angle1 = state.serial1.getCurrentAngle();

    // Для второго устройства угол берем из последних 24 байт
    float angle2 = 0.0f;
    std::vector<unsigned char> raw2 = state.serial2.getRawData();
    if (raw2.size() >= 24)
    {
        std::memcpy(&angle2, &raw2[raw2.size() - 24], 4);
    }

    // Нормализуем углы в диапазон 0..359
    angle1 = fmodf(angle1, 360.0f);
    if (angle1 < 0) angle1 += 360.0f;
    angle2 = fmodf(angle2, 360.0f);
    if (angle2 < 0) angle2 += 360.0f;

    // --- Отрисовка графика ---
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "X (Dev 1): %.2f", angle1);
    ImGui::SameLine();
    ImGui::Text(" | ");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "Y (Dev 2): %.2f", angle2);
    ImGui::Separator();

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();

    // Размеры области графика (оставляем отступы для осей)
    float availWidth = ImGui::GetContentRegionAvail().x;
    float availHeight = ImGui::GetContentRegionAvail().y;

    // Делаем квадратный график, чтобы шкалы X и Y были одинаковыми
    float size = (availWidth < availHeight) ? availWidth : availHeight;
    size -= 60.0f; // Отступы

    // Левый нижний угол графика (начало осей)
    ImVec2 origin = ImVec2(p.x + 50.0f, p.y + size + 20.0f);
    ImVec2 topRight = ImVec2(origin.x + size, origin.y - size);

    // 1. Рисуем фон графика
    draw_list->AddRectFilled(topRight, origin, IM_COL32(25, 25, 25, 255));

    // 2. Рисуем рамку
    draw_list->AddRect(topRight, origin, IM_COL32(100, 100, 100, 255), 0.0f, 0, 2.0f);

    // 3. Рисуем сетку (каждые 45 градусов) и подписи
    for (int i = 0; i <= 360; i += 45)
    {
        float t = (float)i / 360.0f;

        // Вертикальные линии (ось X)
        float xPos = origin.x + t * size;
        draw_list->AddLine(ImVec2(xPos, origin.y), ImVec2(xPos, topRight.y), IM_COL32(60, 60, 60, 255), 1.0f);

        // Горизонтальные линии (ось Y)
        float yPos = origin.y - t * size;
        draw_list->AddLine(ImVec2(origin.x, yPos), ImVec2(topRight.x, yPos), IM_COL32(60, 60, 60, 255), 1.0f);

        // Подписи осей (только для 0, 90, 180, 270, 360)
        if (i % 90 == 0)
        {
            char label[8];
            snprintf(label, sizeof(label), "%d", i);

            // Подпись для оси X (внизу)
            ImVec2 textSizeX = ImGui::CalcTextSize(label);
            draw_list->AddText(ImVec2(xPos - textSizeX.x * 0.5f, origin.y + 5.0f), IM_COL32(150, 150, 150, 255), label);

            // Подпись для оси Y (слева)
            ImVec2 textSizeY = ImGui::CalcTextSize(label);
            draw_list->AddText(ImVec2(origin.x - textSizeY.x - 10.0f, yPos - textSizeY.y * 0.5f),
                               IM_COL32(150, 150, 150, 255), label);
        }
    }

    // 4. Вычисляем координаты точек на осях
    // Нормализуем (0..360) -> (0..1)
    float t1 = angle1 / 360.0f;
    float t2 = angle2 / 360.0f;

    // Точка на оси X (внизу, зеленая)
    ImVec2 ptX = ImVec2(origin.x + t1 * size, origin.y);
    // Точка на оси Y (слева, синяя)
    ImVec2 ptY = ImVec2(origin.x, origin.y - t2 * size);

    // Точка пересечения (внутри графика)
    // ImVec2 intersect = ImVec2(origin.x + t1 * size, origin.y - t2 * size);

    // 5. Рисуем линии-перпендикуляры ("прицел")
    // Вертикальная линия от оси X (зеленая) до самого верха
    draw_list->AddLine(ImVec2(ptX.x, origin.y), ImVec2(ptX.x, topRight.y), IM_COL32(100, 255, 100, 150), 1.5f);

    // Горизонтальная линия от оси Y (синяя) до самого правого края
    draw_list->AddLine(ImVec2(origin.x, ptY.y), ImVec2(topRight.x, ptY.y), IM_COL32(100, 150, 255, 150), 1.5f);

    // 6. Рисуем точки на осях
    draw_list->AddCircleFilled(ptX, 6.0f, IM_COL32(100, 255, 100, 255)); // Зеленая на X
    draw_list->AddCircleFilled(ptY, 6.0f, IM_COL32(100, 150, 255, 255)); // Синяя на Y

    // 7. Рисуем точку пересечения (там, где пересекаются вертикальная и горизонтальная линии)
    ImVec2 intersect = ImVec2(ptX.x, ptY.y); // Пересечение всегда на этих координатах
    draw_list->AddCircleFilled(intersect, 5.0f, IM_COL32(255, 50, 50, 255)); // Белая в центре

    // Сдвигаем курсор, чтобы ImGui знал, что мы заняли место
    ImGui::Dummy(ImVec2(size + 60.0f, size + 60.0f));
}
