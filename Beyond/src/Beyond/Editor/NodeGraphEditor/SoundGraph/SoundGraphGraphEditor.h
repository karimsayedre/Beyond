#pragma once

#include "Beyond/Editor/NodeGraphEditor/NodeGraphEditor.h"
#include "Beyond/Editor/NodeGraphEditor/SoundGraph/SoundGraphEditorTypes.h"

namespace Beyond {
	class SoundGraphNodeGraphEditor final : public IONodeGraphEditor
	{
	public:
		SoundGraphNodeGraphEditor();

		void SetAsset(const Ref<Asset>& asset) override;

	protected:

		void OnUpdate(Timestep ts) override;
		void OnRenderOnCanvas(ImVec2 min, ImVec2 max) override;
		bool DrawPinPropertyEdit(PinPropertyContext& context) override;
		void DrawPinIcon(const Pin* pin, bool connected, int alpha) override;
	};

} // namespace Beyond
