#include "IrisPCH.h"
#include "Ref.h"

namespace Iris {

	static std::unordered_set<void*> s_LiveReferences;
	static std::mutex s_LiveReferencesMutex;

	namespace RefUtils {

		void AddToLiveReferences(void* instance)
		{
			std::scoped_lock<std::mutex> lock(s_LiveReferencesMutex);
			IR_ASSERT(instance);
			s_LiveReferences.insert(instance);
		}

		void RemoveFromLiveReferences(void* instance)
		{
			std::scoped_lock<std::mutex> lock(s_LiveReferencesMutex);
			IR_ASSERT(instance);
			IR_ASSERT(s_LiveReferences.contains(instance));
			s_LiveReferences.erase(instance);
		}

		bool IsLive(void* instance)
		{
			IR_ASSERT(instance);
			return s_LiveReferences.contains(instance);
		}

	}

}