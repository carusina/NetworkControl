#include "SessionManager.h"

namespace Server {

	SessionManager::~SessionManager() {
		CloseAll();
	}

	void SessionManager::AddSession(std::shared_ptr<ClientSession> session)
	{
		if (!session) {
			return;
		}

		// 세션 컨테이너 변경은 배타적으로 처리
		std::unique_lock<std::shared_timed_mutex> lock(managerMutex_);
		sessions_[session->GetSessionId()] = session;
	}

	void SessionManager::RemoveSession(uint64_t sessionId)
	{
		std::shared_ptr<ClientSession> sessionToRemove;

		{
			std::unique_lock<std::shared_timed_mutex> lock(managerMutex_);
			
			const auto it = sessions_.find(sessionId);
			if (it != sessions_.end())
			{
				sessionToRemove = it->second;
				sessions_.erase(it);
			}
		}

		// 컨테이너 락을 잡지 않은 상태에서 워커를 종료
		if (sessionToRemove) {
			sessionToRemove->StopSession();
		}
	}

	std::shared_ptr<ClientSession> SessionManager::GetSession(uint64_t sessionId)
	{
		// 조회는 여러 스레드가 동시에 수행할 수 있음
		std::shared_lock<std::shared_timed_mutex> lock(managerMutex_);

		const auto it = sessions_.find(sessionId);
		return it != sessions_.end() ? it->second : nullptr;
	}

	void SessionManager::CloseAll()
	{
		std::unordered_map<uint64_t, std::shared_ptr<ClientSession>> sessionsToClose;

		{
			std::unique_lock<std::shared_timed_mutex> lock(managerMutex_);
			sessionsToClose = std::move(sessions_);
		}

		// 종료 대기 중에는 SessionManager 락을 점유하지 않음
		for (const auto& entry : sessionsToClose)
		{
			if (entry.second) {
				entry.second->StopSession();
			}
		}
	}

	size_t SessionManager::GetSessionCount() const
	{
		std::shared_lock<std::shared_timed_mutex> lock(managerMutex_);
		return sessions_.size();
	}

	std::vector<std::shared_ptr<ClientSession>> SessionManager::GetSessionsSnapshot() const
	{
		std::shared_lock<std::shared_timed_mutex> lock(managerMutex_);

		std::vector<std::shared_ptr<ClientSession>> sessions;
		sessions.reserve(sessions_.size());
		
		for (const auto& entry : sessions_) {
			sessions.push_back(entry.second);
		}

		return sessions;
	}

} // namespace Server