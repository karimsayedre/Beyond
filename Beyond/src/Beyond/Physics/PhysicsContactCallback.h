#pragma once

#include "Beyond/Scene/Entity.h"

#include <functional>

namespace Beyond {

	enum class ContactType : int8_t { None = -1, CollisionBegin, CollisionEnd, TriggerBegin, TriggerEnd };
	using ContactCallbackFn = std::function<void(ContactType, Entity, Entity)>;

}
