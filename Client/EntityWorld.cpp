#include "EntityWorld.h"

namespace Client {

	void EntityWorld::OnSpawn(const Common::EntitySpawnPayload& payload)
	{
		std::lock_guard<std::mutex> lock(mutex_);

		if (!hasMyEntityId_) {
			hasMyEntityId_ = true;
			myEntityId_ = payload.EntityId;
		}

		EntityInfo& info = entities_[payload.EntityId];
		info.EntityId = payload.EntityId;
		info.Type = payload.Type;
		info.PositionX = payload.PositionX;
		info.PositionY = payload.PositionY;
		info.Heading = payload.Heading;
	}

	void EntityWorld::OnDespawn(const Common::EntityDespawnPayload& payload)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		entities_.erase(payload.EntityId);
	}

	void EntityWorld::OnState(const Common::EntityStatePayload& payload)
	{
		std::lock_guard<std::mutex> lock(mutex_);

		// 없으면 새로 생성 (Spawn을 놓친 경우에도 최소한 State는 보이게)
		EntityInfo& info = entities_[payload.EntityId];
		info.EntityId = payload.EntityId;
		info.PositionX = payload.PositionX;
		info.PositionY = payload.PositionY;
		info.Heading = payload.Heading;
		info.VelocityX = payload.VelocityX;
		info.VelocityY = payload.VelocityY;
	}

	bool EntityWorld::TryGetMyEntityId(uint32_t& entityId) const
	{
		std::lock_guard<std::mutex> lock(mutex_);

		if (!hasMyEntityId_) {
			return false;
		}

		entityId = myEntityId_;
		return true;
	}

	std::vector<EntityInfo> EntityWorld::GetSnapshot() const
	{
		std::lock_guard<std::mutex> lock(mutex_);

		std::vector<EntityInfo> result;
		result.reserve(entities_.size());

		for (const auto& entry : entities_) {
			result.push_back(entry.second);
		}

		return result;
	}

} // namespace Client
