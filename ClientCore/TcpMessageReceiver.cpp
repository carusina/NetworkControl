#include "TcpMessageReceiver.h"

#include "../Common/BinarySerializer.h"
#include "../Common/Protocol.h"

namespace Client {

	TcpMessageReceiver::TcpMessageReceiver(Common::TcpSocket& controlSocket, EntityWorld& entityWorld)
		: controlSocket_(controlSocket), entityWorld_(entityWorld) { }

	TcpMessageReceiver::~TcpMessageReceiver() {
		Stop();
	}

	void TcpMessageReceiver::Start()
	{
		isRunning_ = true;
		workerThread_ = std::thread(&TcpMessageReceiver::ReceiveWorker, this);
	}

	void TcpMessageReceiver::Stop()
	{
		isRunning_ = false;

		// ReceiveAll 블로킹을 풀기 위해 소켓을 닫음 (제어 명령 송신도 이 시점 이후로는 끝났다고 가정)
		controlSocket_.Close();

		if (workerThread_.joinable()) {
			workerThread_.join();
		}
	}

	void TcpMessageReceiver::ReceiveWorker()
	{
		while (isRunning_)
		{
			uint8_t headerByte = 0;
			if (!controlSocket_.ReceiveAll(&headerByte, sizeof(headerByte))) {
				break;
			}

			Common::MessageType type{};
			{
				Common::BinaryReader headerReader(&headerByte, sizeof(headerByte));
				Common::TryDeserializeHeader(headerReader, type);
			}

			if (type == Common::MessageType::EntitySpawn)
			{
				uint8_t payloadBytes[Common::EntitySpawnPayloadSize]{};
				if (!controlSocket_.ReceiveAll(payloadBytes, sizeof(payloadBytes))) {
					break;
				}

				Common::EntitySpawnPayload payload{};
				Common::BinaryReader reader(payloadBytes, sizeof(payloadBytes));
				if (Common::TryDeserializeEntitySpawn(reader, payload)) {
					entityWorld_.OnSpawn(payload);
				}
			}
			else if (type == Common::MessageType::EntityDespawn)
			{
				uint8_t payloadBytes[Common::EntityDespawnPayloadSize]{};
				if (!controlSocket_.ReceiveAll(payloadBytes, sizeof(payloadBytes))) {
					break;
				}

				Common::EntityDespawnPayload payload{};
				Common::BinaryReader reader(payloadBytes, sizeof(payloadBytes));
				if (Common::TryDeserializeEntityDespawn(reader, payload)) {
					entityWorld_.OnDespawn(payload);
				}
			}
			else
			{
				// 이 채널에서 예상하지 못한 타입 - 스트림 정렬이 깨졌다고 보고 종료
				break;
			}
		}
	}

} // namespace Client
