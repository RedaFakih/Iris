#include "IrisPCH.h"
#include "Physics.h"

namespace Iris {

	struct PhysicsSystemData
	{
		JPH::TempAllocator* TemporariesAllocator = nullptr;
		Scope<JPH::JobSystemThreadPool> JobSystemThreadPool;

		std::string LastErrorMessage;
	};

	static PhysicsSystemData* s_Data = nullptr;

	void PhysicsSystem::Init()
	{
		IR_VERIFY(!s_Data, "Can't initialize Physics Engine multiple times!");
		IR_CORE_INFO_TAG("Physics", "Initializing physics engine...");

		s_Data = new PhysicsSystemData();

		// Register allocation hook. This needs to be done before any other Jolt function is called.
		JPH::RegisterDefaultAllocator();

		// Install trace and assert callbacks
		JPH::Trace = [](const char* format, ...)
		{
			va_list list;
			va_start(list, format);

			char buffer[1024]{};
			vsnprintf(buffer, sizeof(buffer), format, list);

			if (s_Data)
				s_Data->LastErrorMessage = buffer;

			va_end(list);
		};

#ifdef JPH_ENABLE_ASSERTS
		JPH::AssertFailed = [](const char* expression, const char* message, const char* file, uint32_t line)
		{
			IR_CORE_FATAL_TAG("Physics", "{}:{}: ({}) {}", file, line, expression, message != nullptr ? message : "");

			// Breakpoint
			return true;
		};
#endif // JPH_ENABLE_ASSERTS

		// Create a factory, this class is responsible for creating instances of classes based on their name or hash and is mainly used for deserialization of saved data.
		// It is not directly used in this example but still required.
		JPH::Factory::sInstance = new JPH::Factory();

		// Register all physics types with the factory and install their collision handlers with the CollisionDispatch class.
		// If you have your own custom shape types you probably need to register their handlers with the CollisionDispatch before calling this function.
		// If you implement your own default material (PhysicsMaterial::sDefault) make sure to initialize it before this function or else this function will create one for you.
		JPH::RegisterTypes();

		// We need a temp allocator for temporary allocations during the physics update. We're
		// pre-allocating 10 MB to avoid having to do allocations during the physics update.
		// TODO: What is this 300 here we should change it or lower the numbers that we create the JPH::PhysicsSystem with
		s_Data->TemporariesAllocator = new JPH::TempAllocatorImpl(300 * 1024 * 1024);

		// We need a job system that will execute physics jobs on multiple threads.
		s_Data->JobSystemThreadPool = CreateScope<JPH::JobSystemThreadPool>(2048, 8, 6);

		// TODO: Mesh Cooking Factory
	}

	void PhysicsSystem::Shutdown()
	{
		IR_CORE_INFO_TAG("Physics", "Shutting down physics engine...");
		
		// TODO: Clean up Mesh Cooking Factory
		
		delete s_Data->TemporariesAllocator;

		delete s_Data;
		s_Data = nullptr;

		delete JPH::Factory::sInstance;
	}

	JPH::TempAllocator* PhysicsSystem::GetTemporariesAllocator()
	{
		return s_Data->TemporariesAllocator;
	}

	JPH::JobSystemThreadPool* PhysicsSystem::GetJobSystemThreadPool()
	{
		return s_Data->JobSystemThreadPool.get();
	}

	const std::string_view PhysicsSystem::GetLastErrorMessage()
	{
		return std::string_view(s_Data->LastErrorMessage);
	}

}