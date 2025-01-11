#pragma once

#include <optional>
#include <map>

//#include "OrderedVector.h"

namespace Beyond
{
	//? NOT USED YET
	class Selectable
	{
		virtual void Select() = 0;
		virtual void Deselect() = 0;
		virtual void IsSelected() = 0;

	};

	template<class ValueType>
	struct TreeModel
	{
		struct Node : Beyond::RefCounted
		{
			Node() = default;
			Node(const Beyond::TreeModel<ValueType>::Node& other)
				: Tree(other.Tree)
				, Name(other.Name)
				, Value(other.Value)
				, Parent(other.Parent)
				, Children(other.Children)
			{
			}

			Node(TreeModel* tree) : Tree(tree)
			{}
			Node(TreeModel* tree, ValueType value) : Tree(tree), Value(value)
			{}
			Node(TreeModel* tree, ValueType value, Node* parent) : Tree(tree), Value(value), Parent(parent)
			{}
			Node(TreeModel* tree, std::optional<ValueType> optValue, Node* parent, const std::string& name) : Tree(tree), Name(name), Value(optValue), Parent(parent)
			{}


			TreeModel* Tree = nullptr;
			std::string Name;

			std::optional<ValueType> Value;

			Node* Parent = nullptr;
			//OrderedVector<Beyond::Ref<Node>> Children;
			std::map<std::string, Beyond::Ref<Node>> Children;
		};

		Node RootNode{ this, std::nullopt, nullptr, "RootNode" };
	};

	// TODO: serialize / deserialize Tree

} // namespace Beyond
