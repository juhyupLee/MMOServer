#include "../Network/BaseJob.h"
#include "../Network/JobDispatcher.h"
#include "../Network/JobQueue.h"

#include <boost/core/lightweight_test.hpp>

#include <memory>

namespace
{
class NoopJob final : public BaseJob
{
public:
	void Excute(std::function<void(std::int64_t, PacketHolder)>&) override
	{
	}
};
}

int main()
{
	std::shared_ptr<JobQueue> queue;
	{
		// A zero-thread dispatcher keeps jobs pending so the high-water mark can
		// be tested deterministically.
		JobDispatcher dispatcher(
			[](std::int64_t, PacketHolder) {},
			1,
			0);
		queue = std::make_shared<JobQueue>(&dispatcher, 2);

		BOOST_TEST(queue->Push(std::make_shared<NoopJob>()));
		BOOST_TEST(queue->Push(std::make_shared<NoopJob>()));
		BOOST_TEST(!queue->Push(std::make_shared<NoopJob>()));
		BOOST_TEST_EQ(queue->GetPendingJobCount(), 2);
	}

	// The queue retains only a lifetime handle, so using it after dispatcher
	// teardown is rejected instead of dereferencing a dangling raw pointer.
	BOOST_TEST(queue->GetJobDispatcher() == nullptr);
	BOOST_TEST(!queue->Push(std::make_shared<NoopJob>()));

	return boost::report_errors();
}
