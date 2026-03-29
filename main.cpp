#include "process_finder.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// ── dark theme ───────────────────────────────────────────────────────────────

static void apply_theme()
{
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding    = 4.0f;
    style.FrameRounding     = 3.0f;
    style.GrabRounding      = 3.0f;
    style.ScrollbarRounding = 4.0f;
    style.TabRounding       = 3.0f;
    style.WindowPadding     = ImVec2(12, 12);
    style.FramePadding      = ImVec2(8, 4);
    style.ItemSpacing       = ImVec2(8, 6);

    ImVec4* c = style.Colors;
    c[ImGuiCol_WindowBg]          = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
    c[ImGuiCol_ChildBg]           = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
    c[ImGuiCol_PopupBg]           = ImVec4(0.15f, 0.15f, 0.17f, 1.00f);
    c[ImGuiCol_Border]            = ImVec4(0.30f, 0.30f, 0.35f, 0.50f);
    c[ImGuiCol_FrameBg]           = ImVec4(0.18f, 0.18f, 0.21f, 1.00f);
    c[ImGuiCol_FrameBgHovered]    = ImVec4(0.25f, 0.25f, 0.28f, 1.00f);
    c[ImGuiCol_FrameBgActive]     = ImVec4(0.30f, 0.30f, 0.33f, 1.00f);
    c[ImGuiCol_TitleBg]           = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
    c[ImGuiCol_TitleBgActive]     = ImVec4(0.14f, 0.14f, 0.16f, 1.00f);
    c[ImGuiCol_MenuBarBg]         = ImVec4(0.14f, 0.14f, 0.16f, 1.00f);
    c[ImGuiCol_ScrollbarBg]       = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
    c[ImGuiCol_ScrollbarGrab]     = ImVec4(0.30f, 0.30f, 0.35f, 1.00f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.40f, 0.45f, 1.00f);
    c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.50f, 0.50f, 0.55f, 1.00f);
    c[ImGuiCol_CheckMark]         = ImVec4(0.45f, 0.72f, 0.96f, 1.00f);
    c[ImGuiCol_SliderGrab]        = ImVec4(0.45f, 0.72f, 0.96f, 1.00f);
    c[ImGuiCol_Button]            = ImVec4(0.24f, 0.52f, 0.88f, 1.00f);
    c[ImGuiCol_ButtonHovered]     = ImVec4(0.30f, 0.58f, 0.94f, 1.00f);
    c[ImGuiCol_ButtonActive]      = ImVec4(0.20f, 0.46f, 0.80f, 1.00f);
    c[ImGuiCol_Header]            = ImVec4(0.22f, 0.22f, 0.26f, 1.00f);
    c[ImGuiCol_HeaderHovered]     = ImVec4(0.28f, 0.28f, 0.32f, 1.00f);
    c[ImGuiCol_HeaderActive]      = ImVec4(0.32f, 0.32f, 0.36f, 1.00f);
    c[ImGuiCol_Separator]         = ImVec4(0.30f, 0.30f, 0.35f, 0.50f);
    c[ImGuiCol_Tab]               = ImVec4(0.18f, 0.18f, 0.21f, 1.00f);
    c[ImGuiCol_TabHovered]        = ImVec4(0.30f, 0.58f, 0.94f, 1.00f);
    c[ImGuiCol_TableHeaderBg]     = ImVec4(0.16f, 0.16f, 0.19f, 1.00f);
    c[ImGuiCol_TableBorderStrong] = ImVec4(0.30f, 0.30f, 0.35f, 0.60f);
    c[ImGuiCol_TableBorderLight]  = ImVec4(0.25f, 0.25f, 0.30f, 0.40f);
    c[ImGuiCol_TableRowBg]        = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    c[ImGuiCol_TableRowBgAlt]     = ImVec4(1.00f, 1.00f, 1.00f, 0.03f);
    c[ImGuiCol_Text]              = ImVec4(0.92f, 0.92f, 0.94f, 1.00f);
    c[ImGuiCol_TextDisabled]      = ImVec4(0.50f, 0.50f, 0.55f, 1.00f);
}

// ── confirm-kill popup ───────────────────────────────────────────────────────

struct KillRequest {
    bool   active   = false;
    bool   killAll  = false;
    uint32_t pid    = 0;
    std::string name;
};

static KillRequest g_killReq;

static void open_kill_popup(uint32_t pid, const std::string& name)
{
    g_killReq = {true, false, pid, name};
    ImGui::OpenPopup("Confirm Kill");
}

static void open_kill_all_popup()
{
    g_killReq = {true, true, 0, ""};
    ImGui::OpenPopup("Confirm Kill");
}

// ── main ─────────────────────────────────────────────────────────────────────

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR lpCmdLine, int)
{
    // --- Parse command-line for initial path ---
    std::string initialPath;
    if (lpCmdLine && lpCmdLine[0] != '\0') {
        initialPath = lpCmdLine;
        // Strip surrounding quotes if present
        if (initialPath.size() >= 2 &&
            initialPath.front() == '"' && initialPath.back() == '"') {
            initialPath = initialPath.substr(1, initialPath.size() - 2);
        }
    }

    // --- GLFW init ---
    if (!glfwInit()) return 1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(960, 560, "WhoLockThis", nullptr, nullptr);
    if (!window) { glfwTerminate(); return 1; }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);  // vsync

    // --- Dear ImGui init ---
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;  // no imgui.ini

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    apply_theme();

    // Scale fonts a bit
    io.Fonts->AddFontDefault();

    // --- App state ---
    char pathBuf[1024] = {};
    if (!initialPath.empty()) {
        std::strncpy(pathBuf, initialPath.c_str(), sizeof(pathBuf) - 1);
    }

    std::vector<ProcessInfo> processes;
    std::string statusMsg;
    bool         autoScanned = false;
    ImVec4       statusColor = ImVec4(0.5f, 0.5f, 0.55f, 1.0f);

    // Lambda: run the scan
    auto do_scan = [&]() {
        std::string target(pathBuf);
        if (target.empty()) {
            statusMsg   = "Please enter or browse for a path first.";
            statusColor = ImVec4(1.0f, 0.8f, 0.3f, 1.0f);
            return;
        }
        auto files = collect_files(target);
        if (files.empty()) {
            processes.clear();
            statusMsg   = "No files found at the given path.";
            statusColor = ImVec4(1.0f, 0.8f, 0.3f, 1.0f);
            return;
        }
        processes = find_locking_processes(files);
        if (processes.empty()) {
            statusMsg   = "No processes are locking the target.";
            statusColor = ImVec4(0.3f, 0.9f, 0.4f, 1.0f);  // green
        } else {
            char buf[128];
            std::snprintf(buf, sizeof(buf), "Found %d process(es) locking the target.",
                          (int)processes.size());
            statusMsg   = buf;
            statusColor = ImVec4(1.0f, 0.45f, 0.35f, 1.0f); // red
        }
    };

    // --- Main loop ---
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Auto-scan on first frame if path was provided via command line
        if (!autoScanned && pathBuf[0] != '\0') {
            autoScanned = true;
            do_scan();
        }

        // Full-window ImGui panel
        const ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->WorkPos);
        ImGui::SetNextWindowSize(vp->WorkSize);

        ImGuiWindowFlags winFlags =
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove     | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoBringToFrontOnFocus;

        ImGui::Begin("##Main", nullptr, winFlags);

        // ── Title ────────────────────────────────────────────────────────
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.72f, 0.96f, 1.0f));
        ImGui::SetWindowFontScale(1.4f);
        ImGui::Text("WhoLockThis");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::TextDisabled("  Find out who's locking your files");
        ImGui::Separator();
        ImGui::Spacing();

        // ── Path input row ───────────────────────────────────────────────
        ImGui::Text("Target:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 280);
        bool enterPressed = ImGui::InputText("##path", pathBuf, sizeof(pathBuf),
                                             ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        if (ImGui::Button("File", ImVec2(80, 0))) {
            std::string f = browse_for_file();
            if (!f.empty()) {
                std::strncpy(pathBuf, f.c_str(), sizeof(pathBuf) - 1);
                do_scan();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Folder", ImVec2(80, 0))) {
            std::string f = browse_for_folder();
            if (!f.empty()) {
                std::strncpy(pathBuf, f.c_str(), sizeof(pathBuf) - 1);
                do_scan();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Scan", ImVec2(80, 0)) || enterPressed) {
            do_scan();
        }

        ImGui::Spacing();

        // ── Status bar ───────────────────────────────────────────────────
        if (!statusMsg.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, statusColor);
            ImGui::TextWrapped("%s", statusMsg.c_str());
            ImGui::PopStyleColor();
            ImGui::Spacing();
        }

        // ── Process table ────────────────────────────────────────────────
        if (!processes.empty()) {
            float tableHeight = ImGui::GetContentRegionAvail().y - 45;

            ImGuiTableFlags tableFlags =
                ImGuiTableFlags_Borders      | ImGuiTableFlags_RowBg       |
                ImGuiTableFlags_Resizable    | ImGuiTableFlags_ScrollY     |
                ImGuiTableFlags_SizingStretchProp;

            if (ImGui::BeginTable("##procs", 8, tableFlags, ImVec2(0, tableHeight))) {
                ImGui::TableSetupScrollFreeze(0, 1); // freeze header row
                ImGui::TableSetupColumn("#",       ImGuiTableColumnFlags_WidthFixed,  30);
                ImGui::TableSetupColumn("PID",     ImGuiTableColumnFlags_WidthFixed,  65);
                ImGui::TableSetupColumn("Name",    ImGuiTableColumnFlags_WidthFixed, 120);
                ImGui::TableSetupColumn("Path",    ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Owner",   ImGuiTableColumnFlags_WidthFixed, 120);
                ImGui::TableSetupColumn("Type",    ImGuiTableColumnFlags_WidthFixed, 110);
                ImGui::TableSetupColumn("Memory",  ImGuiTableColumnFlags_WidthFixed,  70);
                ImGui::TableSetupColumn("Action",  ImGuiTableColumnFlags_WidthFixed,  55);
                ImGui::TableHeadersRow();

                for (int i = 0; i < (int)processes.size(); ++i) {
                    auto& p = processes[i];
                    ImGui::TableNextRow();

                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%d", i + 1);

                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%u", p.pid);

                    ImGui::TableSetColumnIndex(2);
                    ImGui::TextUnformatted(p.name.c_str());

                    ImGui::TableSetColumnIndex(3);
                    ImGui::TextUnformatted(p.fullPath.c_str());
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("%s\nStarted: %s", p.fullPath.c_str(),
                                          p.startTime.c_str());
                    }

                    ImGui::TableSetColumnIndex(4);
                    ImGui::TextUnformatted(p.owner.c_str());

                    ImGui::TableSetColumnIndex(5);
                    ImGui::TextUnformatted(p.type.c_str());

                    ImGui::TableSetColumnIndex(6);
                    ImGui::TextUnformatted(p.memory.c_str());

                    ImGui::TableSetColumnIndex(7);
                    ImGui::PushID(i);
                    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.75f, 0.22f, 0.17f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.30f, 0.25f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.65f, 0.15f, 0.12f, 1.0f));
                    if (ImGui::SmallButton("Kill")) {
                        open_kill_popup(p.pid, p.name);
                    }
                    ImGui::PopStyleColor(3);
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }

            // ── Bottom button row ────────────────────────────────────────
            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.75f, 0.22f, 0.17f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.30f, 0.25f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.65f, 0.15f, 0.12f, 1.0f));
            if (ImGui::Button("Kill All", ImVec2(100, 0))) {
                open_kill_all_popup();
            }
            ImGui::PopStyleColor(3);

            ImGui::SameLine();
            if (ImGui::Button("Refresh", ImVec2(100, 0))) {
                do_scan();
            }
        }

        // ── Confirm Kill popup ───────────────────────────────────────────
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if (ImGui::BeginPopupModal("Confirm Kill", nullptr,
                                   ImGuiWindowFlags_AlwaysAutoResize)) {
            if (g_killReq.killAll) {
                ImGui::Text("Kill ALL %d process(es)?", (int)processes.size());
            } else {
                ImGui::Text("Kill PID %u (%s)?", g_killReq.pid, g_killReq.name.c_str());
            }
            ImGui::Text("This action cannot be undone.");
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::Button("Yes, Kill", ImVec2(120, 0))) {
                if (g_killReq.killAll) {
                    int killed = 0;
                    for (auto& p : processes) {
                        if (kill_process(p.pid)) killed++;
                    }
                    char buf[128];
                    std::snprintf(buf, sizeof(buf), "Killed %d process(es).", killed);
                    statusMsg   = buf;
                    statusColor = ImVec4(1.0f, 0.6f, 0.2f, 1.0f);
                } else {
                    if (kill_process(g_killReq.pid)) {
                        char buf[128];
                        std::snprintf(buf, sizeof(buf), "Killed PID %u (%s).",
                                      g_killReq.pid, g_killReq.name.c_str());
                        statusMsg   = buf;
                        statusColor = ImVec4(0.3f, 0.9f, 0.4f, 1.0f);
                    } else {
                        char buf[128];
                        std::snprintf(buf, sizeof(buf),
                                      "Failed to kill PID %u (access denied?).",
                                      g_killReq.pid);
                        statusMsg   = buf;
                        statusColor = ImVec4(1.0f, 0.45f, 0.35f, 1.0f);
                    }
                }
                // Re-scan after kill
                do_scan();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::End(); // ##Main

        // ── Render ───────────────────────────────────────────────────────
        ImGui::Render();
        int fbW, fbH;
        glfwGetFramebufferSize(window, &fbW, &fbH);
        glViewport(0, 0, fbW, fbH);
        glClearColor(0.12f, 0.12f, 0.14f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    // ── Cleanup ──────────────────────────────────────────────────────────
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
