#include "Global.h"

void CRASH()
{
	int *p = nullptr;
	*p = 10;
}
