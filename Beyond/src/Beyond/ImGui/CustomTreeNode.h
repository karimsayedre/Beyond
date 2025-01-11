#pragma once

#include "Beyond/Core/Ref.h"

#include <imgui/imgui.h>

namespace Beyond {
	class Texture2D;
}

namespace ImGui {
	bool TreeNodeWithIcon(Beyond::Ref<Beyond::Texture2D> icon, ImGuiID id, ImGuiTreeNodeFlags flags, const char* label, const char* label_end, ImColor iconTint = IM_COL32_WHITE);
	bool TreeNodeWithIcon(Beyond::Ref<Beyond::Texture2D> icon, const void* ptr_id, ImGuiTreeNodeFlags flags, ImColor iconTint, const char* fmt, ...);
	bool TreeNodeWithIcon(Beyond::Ref<Beyond::Texture2D> icon, const char* label, ImGuiTreeNodeFlags flags, ImColor iconTint = IM_COL32_WHITE);

} // namespace UI
