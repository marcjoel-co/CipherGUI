#pragma once

#include "imgui.h"
#include <cmath>
#include <random>
#include <chrono>

namespace shenanigans_ui
{

    // --- BUG FIX ---
    // We need to store the default style to properly reset colors.
    static ImGuiStyle default_style;

    // Global shenanigans state
    struct kupalState
    {
        // --- KEPT FEATURES ---
        bool gay_mode = false;    // Rainbow everything
        bool gay_buttons = false; // Rainbow buttons only
        bool gay_table = false;   // Rainbow table only
        bool disco_mode = false;  // Rotating rainbow background for WINDOWS
        // 'background_rainbow_mode' has been removed.
        bool chaos_mode = false;      // Random button positions
        bool invisible_mode = false;  // Everything becomes transparent
        bool seizure_warning = false; // Flashing colors (with warning)

        
        float time_accumulator = 0.0f;
        std::mt19937 rng{static_cast<unsigned int>(std::chrono::steady_clock::now().time_since_epoch().count())};

        // Chaos mode button positions
        struct ChaoticButton
        {
            ImVec2 offset;
            ImVec2 velocity;
        };
        ChaoticButton chaotic_buttons[10] = {};

        void reset()
        {
            // --- BUG FIX ---
            // Restore the default ImGui style to reset all colors properly.
            ImGui::GetStyle() = default_style;

            // Reset all the feature flags
            gay_mode = gay_buttons = gay_table = disco_mode = false;
            chaos_mode = invisible_mode = seizure_warning = false;
            time_accumulator = 0.0f;

            // Reset chaos buttons
            for (int i = 0; i < 10; i++)
            {
                chaotic_buttons[i] = {};
            }
        }
    };

    static kupalState lolo;

    // --- NEW FUNCTION ---
    // Call this ONCE at the start of your program, after ImGui is initialized.
    inline void initializeShenanigans()
    {
        // Save the default style so we can restore it later.
        default_style = ImGui::GetStyle();
    }

    // Utility functions
    inline ImVec4 getRainbowColor(float time, float offset = 0.0f)
    {
        float r = 0.5f + 0.5f * sinf(time * 2.0f + offset);
        float g = 0.5f + 0.5f * sinf(time * 2.0f + offset + 2.094f);
        float b = 0.5f + 0.5f * sinf(time * 2.0f + offset + 4.188f);
        return ImVec4(r, g, b, 1.0f);
    }

    inline float getRandomFloat(float min, float max)
    {
        std::uniform_real_distribution<float> dist(min, max);
        return dist(lolo.rng);
    }

    // Applies visual effects that are kept for the demo
    inline void applyVisualEffects()
    {
        lolo.time_accumulator += ImGui::GetIO().DeltaTime;

        // Disco mode - rotating rainbow background for the ImGui window
        if (lolo.disco_mode)
        {
            ImVec4 disco_color = getRainbowColor(lolo.time_accumulator * 3.0f);
            ImGui::GetStyle().Colors[ImGuiCol_WindowBg] = ImVec4(disco_color.x * 0.3f, disco_color.y * 0.3f, disco_color.z * 0.3f, 0.8f);
        }

        // Gay mode - rainbow everything
        if (lolo.gay_mode)
        {
            ImGuiStyle &style = ImGui::GetStyle();
            for (int i = 0; i < ImGuiCol_COUNT; i++)
            {
                if (i == ImGuiCol_Text)
                    continue; // Keep text readable
                style.Colors[i] = getRainbowColor(lolo.time_accumulator, i * 0.1f);
            }
        }

        // Seizure mode - flashing background
        if (lolo.seizure_warning)
        {
            float flash = sinf(lolo.time_accumulator * 30.0f) > 0 ? 1.0f : 0.0f;
            ImGui::GetStyle().Colors[ImGuiCol_WindowBg] = ImVec4(flash, flash, flash, 0.8f);
        }

        // Invisible mode - fade everything
        if (lolo.invisible_mode)
        {
            // Use a simple fixed alpha to avoid flickering when combined with other effects
            float alpha = 0.2f;
            ImGuiStyle &style = ImGui::GetStyle();
            for (int i = 0; i < ImGuiCol_COUNT; i++)
            {
                style.Colors[i].w = alpha;
            }
        }
    }

    // Simplified button function for Chaos mode
    inline bool ulol(const char *label, int button_index = 0)
    {
        bool clicked = false;

        if (lolo.chaos_mode)
        {
            auto &btn = lolo.chaotic_buttons[button_index % 10];
            btn.offset.x += btn.velocity.x * ImGui::GetIO().DeltaTime;
            btn.offset.y += btn.velocity.y * ImGui::GetIO().DeltaTime;

            ImVec2 window_size = ImGui::GetWindowSize();
            if (btn.offset.x < -50 || btn.offset.x > window_size.x - 100)
                btn.velocity.x *= -1;
            if (btn.offset.y < -20 || btn.offset.y > window_size.y - 50)
                btn.velocity.y *= -1;

            if (getRandomFloat(0, 1) < 0.01f)
            {
                btn.velocity.x += getRandomFloat(-50, 50);
                btn.velocity.y += getRandomFloat(-50, 50);
            }

            ImVec2 current_pos = ImGui::GetCursorPos();
            ImGui::SetCursorPos(ImVec2(current_pos.x + btn.offset.x, current_pos.y + btn.offset.y));
        }

        if (lolo.gay_buttons)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, getRainbowColor(lolo.time_accumulator, button_index * 0.5f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, getRainbowColor(lolo.time_accumulator + 1.0f, button_index * 0.5f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, getRainbowColor(lolo.time_accumulator + 2.0f, button_index * 0.5f));
            clicked = ImGui::Button(label);
            ImGui::PopStyleColor(3);
        }
        else
        {
            clicked = ImGui::Button(label);
        }

        return clicked;
    }

    // Simplified table for Gay Table mode
    inline bool ulolTable(const char *name, int columns, ImGuiTableFlags flags = 0)
    {
        if (lolo.gay_table)
        {
            ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, getRainbowColor(lolo.time_accumulator));
            ImGui::PushStyleColor(ImGuiCol_TableRowBg, getRainbowColor(lolo.time_accumulator + 1.0f));
            ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, getRainbowColor(lolo.time_accumulator + 2.0f));
        }
        return ImGui::BeginTable(name, columns, flags);
    }

    inline void EndTable()
    {
        ImGui::EndTable();
        if (lolo.gay_table)
        {
            ImGui::PopStyleColor(3);
        }
    }

    // The simplified menu for the presentation
    inline void drawShenanigansMenu()
    {
        if (ImGui::BeginMenu("🎭 Shenanigans (Demo Mode)"))
        {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 1.0f, 1.0f), "Pre calc pls wag!");
            ImGui::Separator();

            if (ImGui::BeginMenu(" Gay Mode Collection"))
            {
                ImGui::MenuItem("Turn Everything Gay", nullptr, &lolo.gay_mode);
                ImGui::MenuItem("Gay Buttons Only", nullptr, &lolo.gay_buttons);
                ImGui::MenuItem("Table Only", nullptr, &lolo.gay_table);
                ImGui::EndMenu();
            }

            ImGui::MenuItem("Disco Mode (Window BG)", nullptr, &lolo.disco_mode);
            // The "Rainbow Background" menu item has been removed.

            if (ImGui::MenuItem("💥 Chaos Mode", nullptr, &lolo.chaos_mode))
            {
                if (lolo.chaos_mode)
                {
                    for (int i = 0; i < 10; i++)
                    {
                        lolo.chaotic_buttons[i].velocity = ImVec2(getRandomFloat(-100, 100), getRandomFloat(-100, 100));
                    }
                }
            }

            ImGui::MenuItem(" Invisible Mode", nullptr, &lolo.invisible_mode);
            ImGui::MenuItem("Seizure Warning", nullptr, &lolo.seizure_warning);

            ImGui::Separator();

            if (ImGui::MenuItem("Reset All Shenanigans"))
            {
                lolo.reset();
            }

            ImGui::EndMenu();
        }
    }

    // Call this before drawing your main window
    inline void beginShenanigansFrame()
    {
        applyVisualEffects();
    }

    // Public function to get the clear color for the main application loop.
    // This now always returns the default color.
    inline ImVec4 getBackgroundColor()
    {
        return default_style.Colors[ImGuiCol_WindowBg];
    }

// --- MACROS ---
#define SHENANIGANS_BUTTON(label, index) shenanigans_ui::ulol(label, index)
#define SHENANIGANS_TABLE_BEGIN(name, cols, flags) shenanigans_ui::ulolTable(name, cols, flags)
#define SHENANIGANS_TABLE_END() shenanigans_ui::EndTable()

} // namespace shenanigans_ui