// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: Apache-2.0

#include "test_helpers.hpp"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdlib>
#include <iostream>
#include <string_view>
#include <thread>
#include <utility>

namespace TestHelpers {

class HttpServer::Impl final {
public:
	explicit Impl(std::string body, int statusCode) : socket_(socket(AF_INET, SOCK_STREAM, 0))
	{
		if (socket_ == -1) {
			std::cerr << "Failed to create test HTTP socket\n";
			std::exit(EXIT_FAILURE);
		}

		sockaddr_in address{};
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		if (bind(socket_, reinterpret_cast<const sockaddr *>(&address), sizeof(address)) == -1 ||
		    listen(socket_, 1) == -1) {
			std::cerr << "Failed to bind test HTTP socket\n";
			std::exit(EXIT_FAILURE);
		}

		socklen_t addressSize = sizeof(address);
		if (getsockname(socket_, reinterpret_cast<sockaddr *>(&address), &addressSize) == -1) {
			std::cerr << "Failed to query test HTTP socket\n";
			std::exit(EXIT_FAILURE);
		}
		port_ = ntohs(address.sin_port);

		worker_ = std::thread([this, body = std::move(body), statusCode] {
			const int client = accept(socket_, nullptr, nullptr);
			if (client == -1) {
				return;
			}

			const std::string response = "HTTP/1.1 " + std::to_string(statusCode) +
						     " Test\r\nContent-Length: " + std::to_string(body.size()) +
						     "\r\nConnection: close\r\n\r\n" + body;
			std::string_view remaining(response);
			while (!remaining.empty()) {
				const ssize_t sent = send(client, remaining.data(), remaining.size(), MSG_NOSIGNAL);
				if (sent <= 0) {
					break;
				}
				remaining.remove_prefix(static_cast<std::size_t>(sent));
			}
			close(client);
		});
	}

	~Impl()
	{
		if (worker_.joinable()) {
			worker_.join();
		}
		close(socket_);
	}

	[[nodiscard]] std::string url() const { return "http://127.0.0.1:" + std::to_string(port_) + '/'; }

private:
	int socket_;
	unsigned short port_ = 0;
	std::thread worker_;
};

HttpServer::HttpServer(std::string body, int statusCode) : impl_(std::make_unique<Impl>(std::move(body), statusCode)) {}

HttpServer::~HttpServer() noexcept = default;

std::string HttpServer::url() const
{
	return impl_->url();
}

} // namespace TestHelpers
