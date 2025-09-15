#include "UIDGenerator.h"


UIDGenerator::UIDGenerator()
{
    
}

UIDGenerator::~UIDGenerator()
{
    
}

int64_t UIDGenerator::GenerateSessionID()
{
	return m_lastSessionID.fetch_add(1);
}
