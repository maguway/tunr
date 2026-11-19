#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include "ui_device.h"
#include "ui_graph.h"
#include "AppSettings.h" // <-- ДОБАВИТЬ

int main() {
    if (!glfwInit()) return -1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    GLFWwindow *window = glfwCreateWindow(1400, 750, "Turn - Dual Monitor", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    AppState state;
    state.serial2.setParseMarker(false);
    state.serial1.setParseMode(ParseMode::RawFloat);

    state.serial1.setSessionLogger(&state.logger, 1);
    state.serial2.setSessionLogger(&state.logger, 2);

    // --- НОВОЕ: Загрузка настроек ---
    SettingsData settings = AppSettings::load("turn_settings.ini");
    state.selectedPortName1 = settings.port1;
    state.selectedBaudRate1 = settings.baud1;
    state.selectedPortName2 = settings.port2;
    state.selectedBaudRate2 = settings.baud2;
    state.trailDuration = settings.trailDuration;
    state.infiniteTrail = settings.infiniteTrail;
    state.radiusMode = settings.radiusMode;

    // Автоподключение портов
    if (!state.selectedPortName1.empty()) {
        state.serial1.open(state.selectedPortName1, state.selectedBaudRate1);
    }
    if (!state.selectedPortName2.empty()) {
        state.serial2.open(state.selectedPortName2, state.selectedBaudRate2);
    }
    // ------------------------------

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("Dual Serial Monitor", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoBringToFrontOnFocus);

        DrawTopBar(state);
        ImGui::Separator();

        if (ImGui::BeginTabBar("MainTabs")) {
            if (ImGui::BeginTabItem("Monitor")) {
                DrawDeviceTab(state);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Angle Graph")) {
                DrawGraphTab(state);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        ImGui::End();
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    // --- НОВОЕ: Сохранение настроек перед выходом ---
    SettingsData newSettings;
    newSettings.port1 = state.selectedPortName1;
    newSettings.baud1 = state.selectedBaudRate1;
    newSettings.port2 = state.selectedPortName2;
    newSettings.baud2 = state.selectedBaudRate2;
    newSettings.trailDuration = state.trailDuration;
    newSettings.infiniteTrail = state.infiniteTrail;
    newSettings.radiusMode = state.radiusMode;
    AppSettings::save(newSettings, "turn_settings.ini");
    // ---------------------------------------------

    state.logger.stop();
    state.serial1.close();
    state.serial2.close();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
