#include "receive.hpp"

#include <mutex>
#include <chrono>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <unistd.h>
#include <stdio.h>
#include <math.h>

const int port = 3000;

std::mutex mutex;
bool buf_swap;
std::vector<radar_data_t> data[2];

std::vector<radar_data_t> give_data() {
	std::unique_lock<std::mutex> lock{mutex};
	return data[buf_swap];
}

void receive_task(std::stop_token stop) {
	// Create socket
	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if(sockfd < 0) {
		perror("socket");
		return;
	}

	// Close fd on exit
	struct closefd {
		int fd;
		~closefd() {
			close(fd);
		};
	};

	closefd cleanup(sockfd);

	// Bind to address and port
	sockaddr_in saddr;
	memset(&saddr, 0, sizeof(sockaddr_in));
	saddr.sin_family = AF_INET;
	saddr.sin_addr.s_addr = INADDR_ANY;
	saddr.sin_port = htons(port);
	if(bind(sockfd, (sockaddr*)&saddr, sizeof(sockaddr_in)) < 0) {
		perror("bind");
		return;
	}

	std::vector<char> recv_buf;
	recv_buf.resize(2048);

	while(!stop.stop_requested()) {
		auto *buf = &data[!buf_swap];

		// Check if data is available
		pollfd fds{
			.fd = sockfd,
			.events = POLLIN,
			.revents = 0
		};

		int pollret = poll(&fds, 1, 100);
		if(pollret < 0) {
			perror("poll");
			break;
		}

		// Socket has become invalid for some reason
		if(fds.revents & (POLLERR | POLLHUP | POLLNVAL)) {
			fprintf(stderr, "socket died: %X\n", fds.revents);
			break;
		}

		// Wait for data to be ready
		if(!pollret || !(fds.revents & POLLIN))
			continue;

		// Data is ready for reading
		auto len = recvfrom(sockfd, recv_buf.data(), recv_buf.size(), 0, nullptr, 0);
		if(len < 0) {
			perror("recvfrom");
			break;
		}

		len /= sizeof(float);
		float *recv_buf_f = (float*)recv_buf.data();

		if(len == 0)
			continue;

		// NaN at first position indicates start of measurement
		if(std::isnan(*recv_buf_f)) {
			{
				std::unique_lock<std::mutex> lock{mutex};
				buf_swap ^= 1;
			}

			buf = &data[!buf_swap];
			buf->clear();
			len--;
			recv_buf_f++;
		}

		while(len--)
			buf->push_back(*recv_buf_f++);
	}
}
