#pragma once

class PlayServer : public BaseServerApp
{
public:
	PlayServer();
	void ON_CLGS_AUTHEN_REQ(int64_t sessionID, std::shared_ptr<CLGS_AUTHEN_REQT>& msg);

	JobDispatcher m_main;
	JobDispatcher m_sub;
	bool Initialize() override;
	bool Start() override;
	void Run() override;
	void Release() override;
	void MainDispatch(int64_t key, PacketHolder& messageHolder);
private:
	std::unordered_map<MessageID, std::function<void(int64_t, const PacketHolder&)>> m_handlers{ };

	template<MessageConcept T, auto messageID = MessageIDUnionTraits<T>::enum_value>
	void RegisterPacket(void(PlayServer::* handler)(int64_t, std::shared_ptr<T>&))
	{
		auto handlerFunc = [this, handler](int64_t sessionID, const PacketHolder& messageholder)
			{
				auto msg = std::shared_ptr<T>(static_cast<T*>(messageholder->message.value));
				messageholder->message.type = MessageID::NONE;
				(this->*handler)(sessionID, msg);
			};

		m_handlers.insert({ messageID, handlerFunc });
	}
};
