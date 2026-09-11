#include "receive.hpp"

#include <mutex>
#include <chrono>

std::mutex mutex;
bool buf_swap;
std::vector<radar_data_t> data[2];

std::vector<radar_data_t> give_data() {
	std::unique_lock<std::mutex> lock{mutex};
	return data[buf_swap];
}

void receive_task(std::stop_token stop) {
	using clock = std::chrono::steady_clock;
	auto start = clock::now();

	while(!stop.stop_requested()) {
		auto &buf = data[!buf_swap];

		buf.resize(1024);
		for(size_t i = 0; i < buf.size(); i++)
			buf[i] = std::sin(3.0*i/buf.size() + std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - start).count()/1000.0)+1;

		std::this_thread::sleep_for(std::chrono::milliseconds(10));

		{
			std::unique_lock<std::mutex> lock{mutex};
			buf_swap ^= 1;
		}
	}
}
