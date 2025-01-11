#include "pch.h"
#include "AnimationGraph.h"

#include "Beyond/Asset/AssetManager.h"
#include "Beyond/Debug/Profiler.h"
#include "Beyond/Utilities/ContainerUtils.h"
#include "Beyond/Utilities/StringUtils.h"

#include <algorithm>

namespace Beyond::AnimationGraph {

	Graph::Graph(std::string_view dbgName, UUID id) : NodeProcessor(dbgName, id), EndpointOutputStreams(*this)
	{
		out_Event.AddDestination(std::make_shared<InputEvent>([&](Identifier eventID)
		{
			OutgoingEvents.push(eventID);
		}));

		AddOutEvent(IDs::Event, out_Event);
		OutgoingEvents.reset(1024);
	}


	NodeProcessor* Graph::FindNodeByID(UUID id)
	{
		auto it = std::find_if(Nodes.begin(), Nodes.end(), [id](const Scope<NodeProcessor>& nodePtr)
		{
			return nodePtr->ID == id;
		});

		if (it != Nodes.end())
			return it->get();
		else
			return nullptr;
	}


	void Graph::AddNode(Scope<NodeProcessor>&& node)
	{
		BEY_CORE_ASSERT(node);
		Nodes.emplace_back(std::move(node));
	}


	void Graph::AddNode(NodeProcessor*&& node)
	{
		BEY_CORE_ASSERT(node);
		Nodes.emplace_back(Scope<NodeProcessor>(std::move(node)));
	}


	bool Graph::AddValueConnection(UUID sourceNodeID, Identifier sourceEndpointID, UUID destinationNodeID, Identifier destinationEndpointID) noexcept
	{
		auto* sourceNode = FindNodeByID(sourceNodeID);
		auto* destinationNode = FindNodeByID(destinationNodeID);

		if (!sourceNode || !destinationNode)
		{
			BEY_CORE_ASSERT(false);
			return false;
		}

		AddConnection(sourceNode->OutValue(sourceEndpointID), destinationNode->InValue(destinationEndpointID));
		return true;
	}


	bool Graph::AddEventConnection(UUID sourceNodeID, Identifier sourceEndpointID, UUID destinationNodeID, Identifier destinationEndpointID) noexcept
	{
		auto* sourceNode = FindNodeByID(sourceNodeID);
		auto* destinationNode = FindNodeByID(destinationNodeID);

		if (!sourceNode || !destinationNode)
		{
			BEY_CORE_ASSERT(false);
			return false;
		}

		AddConnection(sourceNode->OutEvent(sourceEndpointID), destinationNode->InEvent(destinationEndpointID));
		return true;
	}


	bool Graph::AddInputValueRoute(Ref<Graph> graph, Identifier graphInputID, UUID destinationNodeID, Identifier destinationEndpointID) noexcept
	{
		auto* destinationNode = FindNodeByID(destinationNodeID);
		auto endpoint = std::find_if(graph->EndpointInputStreams.begin(), graph->EndpointInputStreams.end(), [graphInputID](const Scope<StreamWriter>& endpoint)
		{
			return endpoint->DestinationID == graphInputID; // find EndpointInputStream thats pointing at runtime graphs input value
		});

		if (!destinationNode || endpoint == graph->EndpointInputStreams.end())
		{
			BEY_CORE_ASSERT(false);
			return false;
		}

		AddConnection((*endpoint)->outV.getViewReference(), destinationNode->InValue(destinationEndpointID));
		return true;
	}

	bool Graph::AddInputValueRouteToGraphOutput(Ref<Graph> graph, Identifier graphInputID, Identifier graphOutValueID) noexcept
	{
		auto endpoint = std::find_if(graph->EndpointInputStreams.begin(), graph->EndpointInputStreams.end(), [graphInputID](const Scope<StreamWriter>& endpoint)
		{
			return endpoint->DestinationID == graphInputID; // find EndpointInputStream thats pointing at runtime graphs input value
		});

		if (endpoint == graph->EndpointInputStreams.end())
		{
			BEY_CORE_ASSERT(false);
			return false;
		}

		AddConnection((*endpoint)->outV.getViewReference(), EndpointOutputStreams.InValue(graphOutValueID));

		return true;
	}

	bool Graph::AddInputValueRouteToEvent(Ref<Graph> graph, Identifier graphInputID, UUID destinationNodeID, Identifier destinationEndpointID) noexcept
	{
		auto* destinationNode = FindNodeByID(destinationNodeID);

		if (!destinationNode)
		{
			BEY_CORE_ASSERT(false);
			return false;
		}

		graph->AddInEvent(graphInputID);
		graph->AddRoute(graph->InEvent(graphInputID), destinationNode->InEvent(destinationEndpointID));
		return true;
	}

	bool Graph::AddToGraphOutputConnection(UUID sourceNodeID, Identifier sourceEndpointID, Identifier graphOutValueID)
	{
		auto* sourceNode = FindNodeByID(sourceNodeID);

		if (!sourceNode)
		{
			BEY_CORE_ASSERT(false);
			return false;
		}

		AddConnection(sourceNode->OutValue(sourceEndpointID), EndpointOutputStreams.InValue(graphOutValueID));
		return true;
	}


	bool Graph::AddToGraphOutEventConnection(UUID sourceNodeID, Identifier sourceEndpointID, Ref<Graph> graph, Identifier graphOutEventID)
	{
		auto* sourceNode = FindNodeByID(sourceNodeID);

		if (!sourceNode)
		{
			BEY_CORE_ASSERT(false);
			return false;
		}

		AddRoute(sourceNode->OutEvent(sourceEndpointID), graph->OutEvent(IDs::Event), graphOutEventID);
		return true;
	}


	bool Graph::AddInputValueRouteToGraphOutEventConnection(Ref<Graph> graph, Identifier graphInputID, Identifier graphOutEventID) noexcept
	{
		graph->AddInEvent(graphInputID);
		graph->AddRoute(graph->InEvent(graphInputID), graph->OutEvent(IDs::Event), graphOutEventID);
		return true;
	}


	bool Graph::AddLocalVariableRoute(Identifier graphLocalVariableID, UUID destinationNodeID, Identifier destinationEndpointID) noexcept
	{
		auto* destinationNode = FindNodeByID(destinationNodeID);
		auto endpoint = std::find_if(LocalVariables.begin(), LocalVariables.end(), [graphLocalVariableID](const Scope<StreamWriter>& endpoint) { return endpoint->DestinationID == graphLocalVariableID; });

		if (!destinationNode || endpoint == LocalVariables.end())
		{
			BEY_CORE_ASSERT(false);
			return false;
		}

		AddConnection((*endpoint)->outV.getViewReference(), destinationNode->InValue(destinationEndpointID));
		return true;
	}


	void Graph::Init(const Skeleton* skeleton)
	{
		for (auto& node : Nodes)
			node->Init(skeleton);
	}

	float Graph::Process(float timestep)
	{
		BEY_PROFILE_FUNC();
		for (auto& node : Nodes)
			node->Process(timestep);

		return 0.0f;
	}


	void Graph::AddConnection(OutputEvent& source, InputEvent& destination) noexcept
	{
		source.AddDestination(std::make_shared<InputEvent>(destination));
	}


	void Graph::AddConnection(const choc::value::ValueView& source, choc::value::ValueView& destination) noexcept
	{
		destination = source;
	}


	void Graph::AddRoute(InputEvent& source, InputEvent& destination) noexcept
	{
		InputEvent* dest = &destination;
		source.Event = [dest](Identifier eventID) { (*dest)(eventID); };
	}


	void Graph::AddRoute(InputEvent& source, OutputEvent& destination, Identifier eventID) noexcept
	{
		OutputEvent* dest = &destination;
		source.Event = [dest](Identifier eventID) { (*dest)(eventID); };
	}


	void Graph::AddRoute(OutputEvent& source, OutputEvent& destination, Identifier eventID) noexcept
	{
		OutputEvent* dest = &destination;
		// note: input id is ignored here.  Might want to consider passing it to the output event (?)
		source.AddDestination(std::make_shared<InputEvent>([dest, eventID](Identifier id) { (*dest)(eventID); }));
	}


	void Graph::HandleOutgoingEvents(void* userContext, HandleOutgoingEventFn* handleEvent)
	{
		Identifier eventID;
		while (OutgoingEvents.pop(eventID))
			if(handleEvent)
				handleEvent(userContext, eventID);
	}


	AnimationGraph::AnimationGraph(std::string_view dbgName, UUID id, AssetHandle skeleton) : Graph(dbgName, id)
	{
		auto skeletonAsset = AssetManager::GetAsset<SkeletonAsset>(skeleton);
		
		// TODO (0x): Storing raw skeleton pointer here is going to go badly if the skeleton asset is ever reloaded!
		//            Need to make sure that when that happens, any graphs using the skeleton are re-initialized.
		m_Skeleton = skeletonAsset && skeletonAsset->IsValid() ? &skeletonAsset->GetSkeleton() : nullptr;
	}


	AnimationGraph::AnimationGraph(std::string_view dbgName, UUID id, const Skeleton* skeleton) : Graph(dbgName, id), m_Skeleton(skeleton) {}


	const Skeleton* AnimationGraph::GetSkeleton() const
	{
		return m_Skeleton;
	}


	void AnimationGraph::Init()
	{
		Graph::Init(GetSkeleton());

		auto& outputPose = EndpointOutputStreams.InValue(IDs::Pose);

		// If pose is an int64 then it is not linked to any other nodes in the graph (it would be an array of transforms if that were the case).
		// Initialize pose to be the skeleton bind pose (or leave as identity transforms if there is no skeleton)
		if (outputPose.isInt64())
		{
			outputPose = choc::value::Value(PoseType);
			if (m_Skeleton)
			{
				Pose* pose = static_cast<Pose*>(outputPose.getRawData());

				for (size_t i = 0, N = m_Skeleton->GetBoneNames().size(); i < N; ++i)
				{
					// BoneTransforms[0] is for artificial root bone
					pose->BoneTransforms[i + 1].Translation = m_Skeleton->GetBoneTranslations().at(i);
					pose->BoneTransforms[i + 1].Rotation = m_Skeleton->GetBoneRotations().at(i);
					pose->BoneTransforms[i + 1].Scale = m_Skeleton->GetBoneScales().at(i);
				}
				pose->AnimationDuration = 0.0f;
				pose->AnimationTimePos = 0.0f;
				pose->NumBones = static_cast<uint32_t>(m_Skeleton->GetBoneNames().size());
			}
		}

		m_IsInitialized = true;
	}


	float AnimationGraph::GetAnimationDuration() const
	{
		BEY_CORE_ASSERT(m_IsInitialized);
		return GetPose()->AnimationDuration;
	}


	float AnimationGraph::GetAnimationTimePos() const
	{
		BEY_CORE_ASSERT(m_IsInitialized);
		return GetPose()->AnimationTimePos;
	}


	void AnimationGraph::SetAnimationTimePos(float time)
	{
		for (auto& node : Nodes)
			node->SetAnimationTimePos(time);
	}


	const Pose* AnimationGraph::GetPose() const
	{
		return static_cast<const Pose*>(EndpointOutputStreams.InValue(IDs::Pose).getRawData());
	}

} // namespace Beyond::AnimationGraph
