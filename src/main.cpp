// Dear ImGui: standalone example application for GLFW + OpenGL 3, using programmable pipeline
// (GLFW is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#define GL_SILENCE_DEPRECATION
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

// [Win32] Our example includes a copy of glfw3.lib pre-compiled with VS2010 to maximize ease of testing and compatibility with old VS compilers.
// To link with VS2010-era libraries, VS2015+ requires linking with legacy_stdio_definitions.lib, which we do using this pragma.
// Your own project should not be affected, as you are likely to link with a newer binary of GLFW that is adequate for your version of Visual Studio.
#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

// This example can also compile and run with Emscripten! See 'Makefile.emscripten' for details.
#ifdef __EMSCRIPTEN__
#include "../libs/emscripten/emscripten_mainloop_stub.h"
#endif

static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

// Main code
int main(int argc, char** argv) {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100
    const char* glsl_version = "#version 100";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
    // GL 3.2 + GLSL 150
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif

    // Create window with graphics context
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Open-picross", nullptr, nullptr);
    if (window == nullptr)
        return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    //io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
    //io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows
    //io.ConfigViewportsNoAutoMerge = true;
    //io.ConfigViewportsNoTaskBarIcon = true;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
#ifdef __EMSCRIPTEN__
    ImGui_ImplGlfw_InstallEmscriptenCanvasResizeCallback("#canvas");
#endif
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Our state
    ImVec4 clear_color = ImVec4(0.1, 0.1, 0.1, 1.00f);

    const int size = argc > 1 ? std::stoi(argv[1]) : 15;
    if (size < 5 || size > 50) throw std::invalid_argument("size must be between 5 and 50");
    bool map[size][size], revealed[size][size], cross[size][size];
    uint mistakes = 0, tiles = 0, open = 0;

    memset(revealed, 0, sizeof(revealed));
    memset(cross, 0, sizeof(cross));
    std::random_device rand;
    for(int i = 0; i < size; i++) {
        for(int j = 0; j < size; j++) {
            map[j][i] = (rand() % 3) > 0;
            if(map[j][i]) tiles++;
        }
    }
    int numsX[size][size], numsY[size][size];
    memset(numsX, 0, sizeof(numsX));
    memset(numsY, 0, sizeof(numsY));
    for(int i = 0; i < size; i++) {
        int idx = 0, idy = 0;
        for(int j = 0; j < size; j++) {
            if(map[i][j]) {
                numsY[i][idy]++;
            } else {
                if(numsY[i][idy] > 0)
                    idy++;
            }
            if(map[j][i]) {
                numsX[i][idx]++;
            } else {
                if(numsX[i][idx] > 0)
                    idx++;
            }
        }
    }

    // Main loop
#ifdef __EMSCRIPTEN__
    // For an Emscripten build we are disabling file-system access, so let's not attempt to do a fopen() of the imgui.ini file.
    // You may manually call LoadIniSettingsFromMemory() to load settings from your own storage.
    io.IniFilename = nullptr;
    EMSCRIPTEN_MAINLOOP_BEGIN
#else
    while (!glfwWindowShouldClose(window))
#endif
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        constexpr int sz = 27;
        ImVec2 pos(10, 10),
                p(sz*size/2, sz*size/2);
        ImDrawList* bg = ImGui::GetBackgroundDrawList();

        if(io.MouseDown[0]) {
            float x = io.MousePos.x - p.x,
                y = io.MousePos.y - p.y;
            int px = x / sz,
                py = y / sz;
            if(x >= 0 && y >= 0 &&
                px < size && py < size) {
                if(!map[px][py] && !revealed[px][py] && !cross[px][py]) {
                    mistakes++;
                    cross[px][py] = true;
                } else {
                    if(!revealed[px][py] && ++open == tiles) {
                        ImGui::OpenPopup("Win");
                    }
                }
                revealed[px][py] = true;
            } 
        }

        if(io.MouseDown[1]) {
            float x = io.MousePos.x - p.x,
                y = io.MousePos.y - p.y;
            int px = x / sz,
                py = y / sz;
            if(x >= 0 && y >= 0 &&
                px < size && py < size) {
                if(!revealed[px][py] && !cross[px][py]) {
                    if(map[px][py]) {
                        revealed[px][py] = true;
                        mistakes++;
                        if(++open == tiles) {
                            ImGui::OpenPopup("Win");
                        }
                    } else
                        cross[px][py] = true;
                }
            }
        }

        if (ImGui::BeginPopup("Win")) {
            if(mistakes > 10)
                ImGui::Text("You can do better...");
            else
                ImGui::Text("You Win");
            ImGui::EndPopup();
        }

        std::string mis = std::string("Mistakes : ") + std::to_string(mistakes);
        bg->AddText(ImGui::GetFont(), ImGui::GetFontSize()*2, ImVec2(10, size*sz*1.5), ImColor(255, 255, 255, 255), mis.c_str(), 0, 1500.0f, 0);
        for(int i = size-1; i >= 0; i--) {
            ImVec2 start(p.x + i*sz, p.y - sz*size/2), end(p.x + (i+1)*sz, p.y),
                    start2(p.x - sz*size/2, p.y + i*sz), end2(p.x, p.y + (i+1)*sz);
            bg->AddRectFilled(start, end, ImColor(255, 255, 255, 255));
            bg->AddRect(start, end, ImColor(100, 100, 100, 255));
            bg->AddRectFilled(start2, end2, ImColor(255, 255, 255, 255));
            bg->AddRect(start2, end2, ImColor(100, 100, 100, 255));
            for(int j = 0; j < size; j++) {
                if(numsY[i][j] == 0) break;
                std::string s = std::to_string(numsY[i][j]);
                if(numsY[i][j] >= 10)
                    bg->AddText(ImGui::GetFont(), sz, ImVec2(start.x, start.y + j*sz),
                        ImColor(0, 0, 0, 255), s.c_str(), 0, 1500.0f, 0);
                else
                    bg->AddText(ImGui::GetFont(), sz, ImVec2(start.x + 5, start.y + j*sz),
                        ImColor(0, 0, 0, 255), s.c_str(), 0, 1500.0f, 0);
            }
            for(int j = 0; j < size; j++) {
                if(numsX[i][j] == 0) break;
                std::string s = std::to_string(numsX[i][j]);
                if(numsX[i][j] >= 10)
                    bg->AddText(ImGui::GetFont(), sz, ImVec2(start2.x + j*sz, start2.y),
                        ImColor(0, 0, 0, 255), s.c_str(), 0, 1500.0f, 0);
                else
                    bg->AddText(ImGui::GetFont(), sz, ImVec2(start2.x + 5 + j*sz, start2.y),
                        ImColor(0, 0, 0, 255), s.c_str(), 0, 1500.0f, 0);
            }
        }
        for(int x = size-1; x >= 0; x--)
            for(int y = size-1; y >= 0; y--) {
                ImVec2 start(p.x + x*sz, p.y + y*sz), end(start.x + sz, start.y + sz);
                ImColor color = (revealed[x][y] && map[x][y]) ? ImColor(255, 255, 255, 255) : ImColor(0, 0, 0, 255);
                bg->AddRectFilled(start, end, color);
                bg->AddRect(start, end, ImColor(100, 100, 100, 255));
                if(cross[x][y]) {
                    bg->AddLine(ImVec2(start.x + sz/4, start.y + sz/4), ImVec2(start.x + 3*sz/4, start.y + 3*sz/4), ImColor(255, 255, 255, 255), 2);
                    bg->AddLine(ImVec2(start.x + 3*sz/4, start.y + sz/4), ImVec2(start.x + sz/4, start.y + 3*sz/4), ImColor(255, 255, 255, 255), 2);
                }
            }
        if (size > 5)
            for (int i = 5; i < size - 1; i += 5) {
                ImVec2 start(p.x + i*sz, p.y), end(start.x, start.y + sz*size);
                bg->AddLine(start, end, ImColor(200, 200, 200, 255), 3);
                bg->AddLine(ImVec2(p.x, p.y + i*sz), ImVec2(p.x + size*sz, p.y + i*sz), ImColor(200, 200, 200, 255), 3);
            }

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Update and Render additional Platform Windows
        // (Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste this code elsewhere.
        //  For this specific demo app we could also call glfwMakeContextCurrent(window) directly)
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }

        glfwSwapBuffers(window);
    }
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_END;
#endif

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
