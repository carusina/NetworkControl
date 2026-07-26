#pragma once

#include "ClientSession.h"

#include <memory>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

namespace Server {

	// 활성 ClientSession의 등록과 수명 관리를 담당
	class SessionManager {
		public:
			SessionManager() = default;
			~SessionManager();

			void AddSession(std::shared_ptr<ClientSession> session);
			void RemoveSession(uint64_t sessionId);
			std::shared_ptr<ClientSession> GetSession(uint64_t sessionId);
			void CloseAll();
			size_t GetSessionCount() const;

			// UDP 송신 스레드가 안전하게 순회할 세션 목록을 반환
			std::vector<std::shared_ptr<ClientSession>> GetSessionsSnapshot() const;

		private:
			std::unordered_map<uint64_t, std::shared_ptr<ClientSession>> sessions_;

			// 조회는 병렬을 허용, 추가/삭제만 배타적으로 처리
			mutable std::shared_timed_mutex managerMutex_;
	};

} // namespace Server