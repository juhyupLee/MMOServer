#include "Global.h"

//GMemoryPoolTLSIndex = TlsAll
void CRASH()
{
	int *p = nullptr;
	*p = 10;
}
