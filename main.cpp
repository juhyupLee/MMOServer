

//#include "ServerCore/PlayServer.h"
//#include "ServerCore/Test/Test_LockFreeStack.h"
#include "PlayServer.h"

int main()
{
	PlayServer server;
	server.Initialize();
	server.Start();
	server.Run();

	while(true)
	{
		
	}
}
