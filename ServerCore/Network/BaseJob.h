#pragma once

#include "../Memory/Global.h"

#include <cstdint>
#include <functional>

class BaseJob
{
private:

public:
	virtual ~BaseJob() = default;
	virtual void Excute(std::function<void(int64_t, PacketHolder)>& dispatch) = 0;
};
