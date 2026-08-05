#pragma once

#include "../Common/Protocol.h"

#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace Client {

	struct EntityInfo {
		uint32_t EntityId = 0;
		Common::EntityType Type = Common::EntityType::PlayerHelicopter;
		float PositionX = 0.0f;
		float PositionY = 0.0f;
		float Heading = 0.0f;
		float VelocityX = 0.0f;
		float VelocityY = 0.0f;
	};

	// 서버가 브로드캐스트하는 엔티티 Spawn/Despawn/State를 반영해 현재 알려진 월드 상태를 유지
	class EntityWorld {
		public:
			void OnSpawn(const Common::EntitySpawnPayload& payload);
			void OnDespawn(const Common::EntityDespawnPayload& payload);
			void OnState(const Common::EntityStateEntry& entry);

			// 접속 후 가장 먼저 받은 Spawn의 EntityId = 내 엔티티 (콘솔 표시용)
			bool TryGetMyEntityId(uint32_t& entityId) const;

			std::vector<EntityInfo> GetSnapshot() const;

			// 재접속 시 이전 세션의 엔티티/내 EntityId 흔적을 지움 - 재접속은 서버 쪽에서
			// 완전히 새 세션(새 EntityId)으로 취급되므로, 이전 값을 들고 있으면 안 됨
			void Clear();

		private:
			mutable std::mutex mutex_;
			std::unordered_map<uint32_t, EntityInfo> entities_;
			bool hasMyEntityId_ = false;
			uint32_t myEntityId_ = 0;
	};

} // namespace Client
