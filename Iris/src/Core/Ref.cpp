#include "IrisPCH.h"
#include "Ref.h"

namespace Iris {

//#if IR_CONFIG_DEBUG

	namespace RefUtils {

		inline std::unordered_set<void*>& GetLiveReferences()
		{
			static std::unordered_set<void*> s_LiveReferences;
			return s_LiveReferences;
		}

		inline std::mutex& GetLiveReferencesMutex()
		{
			static std::mutex s_LiveReferencesMutex;
			return s_LiveReferencesMutex;
		}

		void AddToLiveReferences(void* instance)
		{
			std::scoped_lock<std::mutex> lock(GetLiveReferencesMutex());
			IR_ASSERT(instance);
			GetLiveReferences().insert(instance);
		}

		void RemoveFromLiveReferences(void* instance)
		{
			std::scoped_lock<std::mutex> lock(GetLiveReferencesMutex());
			IR_ASSERT(instance);

			auto& liveRefs = GetLiveReferences();
			IR_ASSERT(liveRefs.contains(instance));
			liveRefs.erase(instance);
		}

		bool IsLive(void* instance)
		{
			std::scoped_lock<std::mutex> lock(GetLiveReferencesMutex());
			IR_ASSERT(instance);
			return GetLiveReferences().contains(instance);
		}

		void DumpLiveReferences()
		{
			IR_CORE_INFO("====================== LIVE REFERENCES DUMP ======================");
			std::scoped_lock<std::mutex> lock(GetLiveReferencesMutex());

			auto& liveRefs = GetLiveReferences();

			if (liveRefs.empty())
			{
				IR_CORE_INFO("All references deleted successfully");

				return;
			}

			IR_CORE_ERROR("=== LEAKED REF COUNTED OBJECTS DETECTED ({}) ===", liveRefs.size());
			for (void* ref : liveRefs)
			{
				RefCountedObject* obj = static_cast<RefCountedObject*>(ref);

#if IR_CONFIG_DEBUG
				IR_CORE_ERROR("\t-> Name: {} | RefCount: {} | Address: {}", obj->GetName().empty() ? "Unnamed" : obj->GetName(), obj->GetRefCount(), (void*)obj);
#endif
			}
		}

	}

//#endif

}