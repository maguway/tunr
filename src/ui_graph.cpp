#include "ui_graph.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <deque>
#include <vector>

static const float kPi = 3.14159265f;

// Точка шлейфа хранится НЕ в пикселях, а в "координатах данных" (x, y),
// поэтому шлейф корректно переживает изменение размера окна.
//   Декартов график: x = angle1 / 360, y = angle2 / 360          (0..1)
//   Полярный график: x, y - точка на единичном круге (радиус 0..1)
// brk == true означает "разрыв": от предыдущей точки линия к этой не рисуется
// (переход через 360° -> 0°, пауза в данных, смена режима и т.п.).
struct TrailPoint {
    float x;
    float y;
    double t;
    bool brk;
};

static std::deque<TrailPoint> trailHistoryCartesian;
static std::deque<TrailPoint> trailHistoryPolar;

struct TrailSettings {
    float duration = 1.5f;
    bool infinite = false;
};

static TrailSettings g_trailCart;
static TrailSettings g_trailPolar;

static int g_radiusMode = 0;
[[maybe_unused]] static float g_currentPowerRaw = 0.0f;

// --- Настройки авто-нормализации ---
[[maybe_unused]] static bool g_auto_scale_power = true;
static float g_noise_floor_db = 100.0f;  // Текущий уровень шума (центр)
static float g_dynamic_range_db = 40.0f; // На сколько дБ должен увеличиться сигнал, чтобы дойти до края
// Скорость, с которой уровень шума "подтягивается" к текущему значению, когда сигнал выше него.
// 16.67 с эквивалентно прежним 0.001 на кадр при 60 FPS, но теперь не зависит от частоты кадров.
static const float kNoiseReleaseSeconds = 16.67f;

// Максимум точек в шлейфе (раньше 5000 ≈ 83 с при 60 FPS, из-за чего слайдер до 600 с и "Infinite" не работали).
static const size_t kMaxTrailPoints = 30000;

// Для определения разрывов шлейфа
static bool g_havePrevCart = false;
static float g_prevCartA1 = 0.0f;
static float g_prevCartA2 = 0.0f;
static bool g_havePrevPolar = false;
static float g_prevPolarRadius = 0.0f;

static void ClearAllGraphs() {
    trailHistoryCartesian.clear();
    trailHistoryPolar.clear();
    g_havePrevCart = false;
    g_havePrevPolar = false;
    // Сбрасываем авто-настройки
    g_noise_floor_db = 100.0f;
}

static void PushTrailPoint(std::deque<TrailPoint> &history, float x, float y, bool forceBreak) {
    const double now = ImGui::GetTime();
    bool brk = forceBreak;

    if (!history.empty()) {
        TrailPoint &last = history.back();
        // Давно не было точек (вкладка была скрыта) - не соединяем линией через весь график.
        if (now - last.t > 0.5) brk = true;
        // Точка не сдвинулась: новую не добавляем (не раздуваем историю), только "освежаем" время.
        if (!brk && std::fabs(last.x - x) < 1e-5f && std::fabs(last.y - y) < 1e-5f) {
            last.t = now;
            return;
        }
    }
    history.push_back({x, y, now, brk});
}

// offset/scale переводят координаты данных в пиксели: pixel = offset + (x * scale.x, y * scale.y)
static void DrawTrail(ImDrawList *draw_list, std::deque<TrailPoint> &history,
                      const TrailSettings &settings, ImU32 baseColor, float maxThickness,
                      ImVec2 offset, ImVec2 scale) {
    const double currentTime = ImGui::GetTime();
    if (!settings.infinite) {
        while (!history.empty() && (currentTime - history.front().t > settings.duration)) {
            history.pop_front();
        }
    }
    while (history.size() > kMaxTrailPoints) {
        history.pop_front();
    }

    if (history.size() < 2) return;

    const int cr = (baseColor >> IM_COL32_R_SHIFT) & 0xFF;
    const int cg = (baseColor >> IM_COL32_G_SHIFT) & 0xFF;
    const int cb = (baseColor >> IM_COL32_B_SHIFT) & 0xFF;

    for (size_t i = 0; i + 1 < history.size(); ++i) {
        const TrailPoint &a = history[i];
        const TrailPoint &b = history[i + 1];
        if (b.brk) continue; // разрыв: между a и b линию не рисуем

        const float age = static_cast<float>(currentTime - a.t);
        float lifeRatio;
        if (settings.infinite) lifeRatio = 1.0f;
        else lifeRatio = settings.duration > 0.0f ? (1.0f - (age / settings.duration)) : 0.0f;

        if (lifeRatio > 0.0f) {
            const float thickness = 0.5f + (maxThickness - 0.5f) * lifeRatio;
            const float alpha = 200.0f * lifeRatio;
            const ImU32 color = IM_COL32(cr, cg, cb, (int) alpha);
            draw_list->AddLine(ImVec2(offset.x + a.x * scale.x, offset.y + a.y * scale.y),
                               ImVec2(offset.x + b.x * scale.x, offset.y + b.y * scale.y),
                               color, thickness);
        }
    }
}

void DrawGraphTab(AppState &state) {
    float angle1 = state.serial1.getCurrentAngle();
    float angle2 = 0.0f;
    float power = 0.0f;

    // Последние 24 байта Dev2 (одна короткая копия вместо копирования всего буфера каждый кадр)
    static float lastAngle2 = 0.0f; // последние корректные значения на случай мусорных байтов (NaN/Inf)
    static float lastPower = 0.0f;
    const std::vector<unsigned char> raw2 = state.serial2.getTailBytes(24);
    if (raw2.size() >= 24) {
        std::memcpy(&angle2, &raw2[0], 4);
        std::memcpy(&power, &raw2[16], 4);
        if (std::isfinite(angle2)) lastAngle2 = angle2; else angle2 = lastAngle2;
        if (std::isfinite(power)) lastPower = power; else power = lastPower;
    }
    if (!std::isfinite(angle1)) angle1 = 0.0f;
    g_currentPowerRaw = power;

    angle1 = fmodf(angle1, 360.0f);
    if (angle1 < 0) angle1 += 360.0f;
    angle2 = fmodf(angle2, 360.0f);
    if (angle2 < 0) angle2 += 360.0f;

    // --- Логика авто-нормализации мощности ---
    // Уровень шума "прилипает" к минимуму: если сигнал ниже него, уровень сразу опускается.
    // Если сигнал выше, уровень медленно подтягивается вверх к текущему значению
    // (с постоянной времени kNoiseReleaseSeconds, не зависящей от FPS).
    if (power < g_noise_floor_db) {
        g_noise_floor_db = power;
    } else {
        const float dt = ImGui::GetIO().DeltaTime;
        const float k = 1.0f - expf(-dt / kNoiseReleaseSeconds);
        g_noise_floor_db += (power - g_noise_floor_db) * k;
    }

    // Вычисляем радиус для мощности
    float radius_val_power = 0.0f;

    // Считаем разницу между текущим сигналом и шумом
    float diff_db = power - g_noise_floor_db;

    // Нормируем: 0 дБ = 0, g_dynamic_range_db (40) и больше = 1.0
    float normalized = diff_db / g_dynamic_range_db;
    if (normalized < 0.0f) normalized = 0.0f;
    if (normalized > 1.0f) normalized = 1.0f;

    radius_val_power = normalized * 360.0f;

    // --- Верхняя панель управления ---
    ImGui::SetWindowFontScale(1.5f);
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "X (Dev 1): %.2f", angle1);
    ImGui::SameLine();
    ImGui::Text(" | ");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "Y (Dev 2): %.2f", angle2);
    ImGui::SameLine();
    ImGui::Text(" | ");
    ImGui::SameLine();

    // Выводим сырую мощность и текущий уровень шума
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Power: %.1f (Noise: %.1f)",
                       power, g_noise_floor_db);
    ImGui::SetWindowFontScale(1.0f);

    // Настройки шлейфа
    ImGui::SetNextItemWidth(200.0f);
    ImGui::SliderFloat("Trail duration (sec)", &g_trailCart.duration, 0.0f, 600.0f, "%.1f");

    ImGui::SameLine();
    ImGui::Checkbox("Infinite Trail", &g_trailCart.infinite);

    ImGui::SameLine();
    if (ImGui::Button("Clear Graph")) {
        ClearAllGraphs();
    }

    ImGui::Separator();

    // Радио-кнопки
    ImGui::Text("Polar Radius Source:");
    ImGui::SameLine();
    ImGui::RadioButton("Angle 2", &g_radiusMode, 0);
    ImGui::SameLine();
    ImGui::RadioButton("|Angle 1 - Angle 2|", &g_radiusMode, 1);
    ImGui::SameLine();
    ImGui::RadioButton("Power", &g_radiusMode, 2);

    // При смене источника радиуса старый полярный шлейф теряет смысл (другая геометрия) - очищаем его
    static int lastRadiusMode = g_radiusMode;
    if (g_radiusMode != lastRadiusMode) {
        trailHistoryPolar.clear();
        g_havePrevPolar = false;
        lastRadiusMode = g_radiusMode;
    }

    ImGui::Separator();

    g_trailPolar.duration = g_trailCart.duration;
    g_trailPolar.infinite = g_trailCart.infinite;

    ImDrawList *draw_list = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();

    float availWidth = ImGui::GetContentRegionAvail().x;
    float availHeight = ImGui::GetContentRegionAvail().y;
    float halfWidth = availWidth * 0.5f;
    float size = (halfWidth < availHeight ? halfWidth : availHeight) - 40.0f;
    size = std::max(size, 50.0f); // в очень маленьком окне размер не должен стать отрицательным

    // ЛЕВЫЙ ГРАФИК
    ImVec2 origin = ImVec2(p.x + 50.0f, p.y + size + 20.0f);
    ImVec2 topRight = ImVec2(origin.x + size, origin.y - size);

    draw_list->AddRectFilled(topRight, origin, IM_COL32(25, 25, 25, 255));
    draw_list->AddRect(topRight, origin, IM_COL32(100, 100, 100, 255), 0.0f, 0, 2.0f);

    for (int i = 0; i <= 360; i += 45) {
        float t = (float) i / 360.0f;
        float xPos = origin.x + t * size;
        float yPos = origin.y - t * size;
        draw_list->AddLine(ImVec2(xPos, origin.y), ImVec2(xPos, topRight.y), IM_COL32(60, 60, 60, 255), 1.0f);
        draw_list->AddLine(ImVec2(origin.x, yPos), ImVec2(topRight.x, yPos), IM_COL32(60, 60, 60, 255), 1.0f);
        if (i % 90 == 0) {
            char label[16];
            snprintf(label, sizeof(label), "%d", i);
            ImVec2 textSize = ImGui::CalcTextSize(label);
            draw_list->AddText(ImVec2(xPos - textSize.x * 0.5f, origin.y + 5.0f), IM_COL32(150, 150, 150, 255), label);
            draw_list->AddText(ImVec2(origin.x - textSize.x - 10.0f, yPos - textSize.y * 0.5f),
                               IM_COL32(150, 150, 150, 255), label);
        }
    }

    ImVec2 ptX = ImVec2(origin.x + (angle1 / 360.0f) * size, origin.y);
    ImVec2 ptY = ImVec2(origin.x, origin.y - (angle2 / 360.0f) * size);
    ImVec2 intersect = ImVec2(ptX.x, ptY.y);

    draw_list->AddLine(ImVec2(ptX.x, origin.y), ImVec2(ptX.x, topRight.y), IM_COL32(100, 255, 100, 150), 1.5f);
    draw_list->AddLine(ImVec2(origin.x, ptY.y), ImVec2(topRight.x, ptY.y), IM_COL32(100, 150, 255, 150), 1.5f);
    draw_list->AddCircleFilled(ptX, 6.0f, IM_COL32(100, 255, 100, 255));
    draw_list->AddCircleFilled(ptY, 6.0f, IM_COL32(100, 150, 255, 255));

    // Для декартова графика (красный шлейф).
    // Переход через 360° -> 0° по любой оси разрывает линию, а не рисует её через весь график.
    {
        const bool wrap = g_havePrevCart && (std::fabs(angle1 - g_prevCartA1) > 180.0f ||
                                             std::fabs(angle2 - g_prevCartA2) > 180.0f);
        PushTrailPoint(trailHistoryCartesian, angle1 / 360.0f, angle2 / 360.0f, wrap);
        g_prevCartA1 = angle1;
        g_prevCartA2 = angle2;
        g_havePrevCart = true;
    }
    DrawTrail(draw_list, trailHistoryCartesian, g_trailCart, IM_COL32(255, 50, 50, 255), 4.0f,
              origin, ImVec2(size, -size));
    draw_list->AddCircleFilled(intersect, 4.0f, IM_COL32(255, 50, 50, 255));

    // ПРАВЫЙ ГРАФИК
    ImVec2 polarCenter = ImVec2(p.x + halfWidth + 20.0f + size * 0.5f, p.y + size * 0.5f + 20.0f);
    float radius = size * 0.5f - 10.0f;

    draw_list->AddCircleFilled(polarCenter, radius, IM_COL32(25, 25, 25, 255));
    draw_list->AddCircle(polarCenter, radius, IM_COL32(100, 100, 100, 255), 0, 2.0f);

    for (int i = 1; i <= 3; ++i) {
        float r = radius * (i / 4.0f);
        draw_list->AddCircle(polarCenter, r, IM_COL32(60, 60, 60, 255), 0, 1.0f);
    }

    // Подписи: 0 - вертикаль вверх, дальше по часовой стрелке (90 справа, 180 снизу, 270 слева).
    // Это то же направление, что у стрелок на вкладке Monitor и у самой точки графика (angle1 - 90°).
    const char *labels[4] = {"0", "90", "180", "270"};
    for (int i = 0; i < 8; ++i) {
        float a = (i * 45.0f) * (kPi / 180.0f) - kPi * 0.5f;
        ImVec2 edge = ImVec2(polarCenter.x + radius * cosf(a), polarCenter.y + radius * sinf(a));
        draw_list->AddLine(polarCenter, edge, IM_COL32(60, 60, 60, 255), 1.0f);
        if (i % 2 == 0) {
            int labelIdx = i / 2;
            ImVec2 textSize = ImGui::CalcTextSize(labels[labelIdx]);
            ImVec2 textPos = ImVec2(polarCenter.x + (radius + 15.0f) * cosf(a) - textSize.x * 0.5f,
                                    polarCenter.y + (radius + 15.0f) * sinf(a) - textSize.y * 0.5f);
            draw_list->AddText(textPos, IM_COL32(150, 150, 150, 255), labels[labelIdx]);
        }
    }

    float radius_value = 0.0f;
    if (g_radiusMode == 0) {
        radius_value = angle2;
    } else if (g_radiusMode == 1) {
        radius_value = fabsf(angle1 - angle2);
        if (radius_value > 360.0f) radius_value = 360.0f;
    } else if (g_radiusMode == 2) {
        radius_value = radius_val_power;
    }

    // angle1 - угловой сектор (0 - вертикаль вверх)
    // radius_value - расстояние от центра (0..360)
    float rad1 = (angle1 - 90.0f) * (kPi / 180.0f);
    float rNorm = radius_value / 360.0f; // 0..1
    float unitX = rNorm * cosf(rad1);
    float unitY = rNorm * sinf(rad1);

    ImVec2 polarPoint = ImVec2(polarCenter.x + unitX * radius, polarCenter.y + unitY * radius);

    // Для полярного графика (оранжевый шлейф).
    // В режимах 0 и 1 радиус сам "заворачивается" через 360, это тоже разрыв линии.
    {
        const bool wrap = g_havePrevPolar && g_radiusMode != 2 &&
                          std::fabs(radius_value - g_prevPolarRadius) > 180.0f;
        PushTrailPoint(trailHistoryPolar, unitX, unitY, wrap);
        g_prevPolarRadius = radius_value;
        g_havePrevPolar = true;
    }
    DrawTrail(draw_list, trailHistoryPolar, g_trailPolar, IM_COL32(255, 150, 50, 255), 4.0f,
              polarCenter, ImVec2(radius, radius));
    draw_list->AddCircleFilled(polarPoint, 4.0f, IM_COL32(255, 200, 50, 255));

    ImGui::Dummy(ImVec2(availWidth, size + 40.0f));
}
