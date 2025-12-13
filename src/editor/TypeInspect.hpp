// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#ifndef InspectType_hpp
#define InspectType_hpp

#include "InspectorState.hpp"
#include "misc/cpp/imgui_stdlib.h" // ImGui widgets for std::string

namespace eeng::editor {

    // struct InspectorState;

    /// General type inspection template
    template<class T>
    bool inspect_type(T& t, InspectorState& inspector)
    {
        ImGui::TextDisabled("No widget for type");
        return false;
    }

    // TODO
    // unsigned char
    // unsigned int

    /// Inspect float
    template<>
    inline bool inspect_type<float>(float& t, InspectorState& inspector)
    {
        return ImGui::InputFloat("##label", &t, 1.0f);
    }

    /// Inspect const float
    template<>
    inline bool inspect_type<const float>(const float& t, InspectorState& inspector)
    {
        ImGui::TextDisabled("%.2f", t);
        return false;
    }

    /// Inspect double
    template<>
    inline bool inspect_type<double>(double& t, InspectorState& inspector)
    {
        return ImGui::InputDouble("##label", &t, 1.0f);
    }

    /// Inspect const double
    template<>
    inline bool inspect_type<const double>(const double& t, InspectorState& inspector)
    {
        ImGui::TextDisabled("%.2f", t);
        return false;
    }

    /// Inspect int
    template<>
    inline bool inspect_type<int>(int& t, InspectorState& inspector)
    {
        return ImGui::InputInt("##label", &t, 1);
    }

    /// Inspect const int
    template<>
    inline bool inspect_type<const int>(const int& t, InspectorState& inspector)
    {
        ImGui::TextDisabled("%i", t);
        return false;
    }

    /// Inspect uint8_t
    template<>
    inline bool inspect_type<uint8_t>(uint8_t& value, InspectorState& inspector)
    {
        return ImGui::InputScalar("Input uint32_t", ImGuiDataType_U8, &value);
    }

    /// Inspect const uint8_t
    template<>
    inline bool inspect_type<const uint8_t>(const uint8_t& t, InspectorState& inspector)
    {
        ImGui::TextDisabled("%u", t);
        return false;
    }

    /// Inspect uint32_t
    template<>
    inline bool inspect_type<uint32_t>(uint32_t& value, InspectorState& inspector)
    {
        return ImGui::InputScalar("Input uint32_t", ImGuiDataType_U32, &value);
    }

    /// Inspect const uint32_t
    template<>
    inline bool inspect_type<const uint32_t>(const uint32_t& t, InspectorState& inspector)
    {
        ImGui::TextDisabled("%u", t);
        return false;
    }

    /// Inspect bool
    template<>
    inline bool inspect_type<bool>(bool& t, InspectorState& inspector)
    {
        return ImGui::Checkbox("##label", &t);
    }

    /// Inspect const bool
    template<>
    inline bool inspect_type<const bool>(const bool& t, InspectorState& inspector)
    {
        inspector.begin_disabled();
        bool b = t;
        ImGui::Checkbox("##label", &b);
        inspector.end_disabled();
        return false;
    }

    /// Inspect std::string
    template<>
    inline bool inspect_type<std::string>(std::string& t, InspectorState& inspector)
    {
        return ImGui::InputText("##label", &t); // label cannot be empty
    }

    /// Inspect const std::string
    template<>
    inline bool inspect_type<const std::string>(const std::string& t, InspectorState& inspector)
    {
        ImGui::TextDisabled("%s", t.c_str());
        return false;
    }
} // namespace Editor

#endif // InspectorState_hpp
