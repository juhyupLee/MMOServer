#pragma once
#include "../ServerCore/Network/JobDispatcher.h"

class PlayServer : public BaseServerApp
{
public:
	PlayServer();
	void OnFAuthenticationReq(int64_t sessionID, std::shared_ptr<FAuthenticationReqT>& msg);

	JobDispatcher m_main;
	bool Initialize() override;
	bool Start() override;
	void Run() override;
	void Release() override;
	void Dispatch(int64_t key, MessageHolderPtr& messageHolder);
private:
	std::unordered_map<MessageID, std::function<void(int64_t, const MessageHolderPtr&)>> m_handlers{ };

	template<MessageConcept T, auto messageID = MessageIDUnionTraits<T>::enum_value>
	void RegisterPacket(void(PlayServer::* handler)(int64_t, std::shared_ptr<T>&))
	{
		auto handlerFunc = [this, handler](int64_t sessionID, const MessageHolderPtr& messageholder)
			{
				auto msg = std::shared_ptr<T>(static_cast<T*>(messageholder->message.value));
				messageholder->message.type = MessageID::NONE;
				(this->*handler)(sessionID, msg);
			};

		m_handlers.insert({ messageID, handlerFunc });
	}
};
