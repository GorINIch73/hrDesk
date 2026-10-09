#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "app/Fonts.h"
#include <iostream>
#include <cstdlib>

int main() {
    if (!glfwInit()) {
        std::cerr << "glfwInit failed\n";
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

    GLFWwindow* window = glfwCreateWindow(900, 600, "HR-Desk", nullptr, nullptr);
    if (!window) {
        std::cerr << "glfwCreateWindow failed\n";
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    std::cout << "OK: окно создано, DISPLAY=" << (getenv("DISPLAY") ? getenv("DISPLAY") : "нет") << std::endl;

    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    LoadedFonts fonts = loadFonts(HRDESK_ASSETS_DIR);

    while (!glfwWindowShouldClose(window)) {
        glfwWaitEventsTimeout(0.05);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::PushFont(fonts.uiBold);
        ImGui::Text("HR-Desk — кадровая база данных");
        ImGui::PopFont();

        ImGui::PushFont(fonts.ui);
        ImGui::Text("Версия: %s", HRDESK_VERSION);
        ImGui::Text("Сотрудников: 0 · Подразделений: 0");
        ImGui::Separator();
        ImGui::Text("Привет! Кириллица загружена корректно.");
        char searchBuf[64] = "";
        ImGui::InputText("Поиск", searchBuf, sizeof(searchBuf));
        ImGui::PopFont();

        ImGui::Render();
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.12f, 0.13f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
