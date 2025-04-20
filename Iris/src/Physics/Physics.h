#pragma once

#include "Core/Base.h"
#include "MeshColliderCache.h"
#include "MeshCookingFactory.h"
#include "PhysicsScene.h"

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>

namespace Iris {

	struct PhysicsSettings
	{
		float FixedTimestep = 1.0f / 60.0f;
		glm::vec3 Gravity = { 0.0f, -9.81f, 0.0f };
		uint32_t PositionSolverIterations = 2;
		uint32_t VelocitySolverIterations = 8;

		uint32_t MaxBodies = 5700;
	};

	class PhysicsSystem
	{
	public:
		static void Init();
		static void Shutdown();

		static Ref<PhysicsScene> CreatePhysicsScene(const Ref<Scene>& scene) { return PhysicsScene::Create(scene); }
		static Ref<MeshColliderAsset> GetOrCreateMeshColliderAsset(Entity entity, MeshColliderComponent& meshColliderComponent);

		static PhysicsSettings& GetSettings() { return s_Settings; }
		static Ref<MeshColliderCache>& GetMesheColliderCache() { return s_MeshColliderCache; }

		static JPH::TempAllocator* GetTemporariesAllocator();
		static JPH::JobSystemThreadPool* GetJobSystemThreadPool();

		static const std::string_view GetLastErrorMessage();

	private:
		inline static PhysicsSettings s_Settings;
		inline static Ref<MeshColliderCache> s_MeshColliderCache;

	};

}