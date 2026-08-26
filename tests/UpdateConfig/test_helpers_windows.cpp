// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: Apache-2.0

#include "test_helpers.hpp"

#include <winsock2.h>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <thread>
#include <utility>

namespace TestHelpers {

class HttpServer::Impl final {
public:
	explicit Impl(std::string body, int statusCode, std::chrono::milliseconds responseDelay)
	{
		WSADATA data{};
		if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
			std::cerr << "Failed to initialize Winsock\n";
			std::exit(EXIT_FAILURE);
		}

		socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (socket_ == INVALID_SOCKET) {
			std::cerr << "Failed to create test HTTP socket\n";
			std::exit(EXIT_FAILURE);
		}

		sockaddr_in address{};
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		if (bind(socket_, reinterpret_cast<const sockaddr *>(&address), sizeof(address)) == SOCKET_ERROR ||
		    listen(socket_, 1) == SOCKET_ERROR) {
			std::cerr << "Failed to bind test HTTP socket\n";
			std::exit(EXIT_FAILURE);
		}

		int addressSize = sizeof(address);
		if (getsockname(socket_, reinterpret_cast<sockaddr *>(&address), &addressSize) == SOCKET_ERROR) {
			std::cerr << "Failed to query test HTTP socket\n";
			std::exit(EXIT_FAILURE);
		}
		port_ = ntohs(address.sin_port);

		worker_ = std::thread([this, body = std::move(body), statusCode, responseDelay] {
			const SOCKET client = accept(socket_, nullptr, nullptr);
			if (client == INVALID_SOCKET) {
				return;
			}

			std::string request;
			char requestBuffer[1024];
			while (request.find("\r\n\r\n") == std::string::npos) {
				const int received = recv(client, requestBuffer, sizeof(requestBuffer), 0);
				if (received <= 0) {
					closesocket(client);
					return;
				}
				request.append(requestBuffer, static_cast<std::size_t>(received));
			}
			std::this_thread::sleep_for(responseDelay);

			const std::string response = "HTTP/1.1 " + std::to_string(statusCode) +
						     " Test\r\nContent-Length: " + std::to_string(body.size()) +
						     "\r\nConnection: close\r\n\r\n" + body;
			std::string_view remaining(response);
			while (!remaining.empty()) {
				const int sent = send(client, remaining.data(), static_cast<int>(remaining.size()), 0);
				if (sent == SOCKET_ERROR || sent == 0) {
					break;
				}
				remaining.remove_prefix(static_cast<std::size_t>(sent));
			}
			shutdown(client, SD_SEND);
			closesocket(client);
		});
	}

	~Impl()
	{
		if (socket_ != INVALID_SOCKET) {
			shutdown(socket_, SD_BOTH);
			closesocket(socket_);
			socket_ = INVALID_SOCKET;
		}
		if (worker_.joinable()) {
			worker_.join();
		}
		WSACleanup();
	}

	[[nodiscard]] std::string url() const { return "http://127.0.0.1:" + std::to_string(port_) + '/'; }

private:
	SOCKET socket_ = INVALID_SOCKET;
	unsigned short port_ = 0;
	std::thread worker_;
};

HttpServer::HttpServer(std::string body, int statusCode, std::chrono::milliseconds responseDelay)
	: impl_(std::make_unique<Impl>(std::move(body), statusCode, responseDelay))
{
}

HttpServer::~HttpServer() noexcept = default;

std::string HttpServer::url() const
{
	return impl_->url();
}

} // namespace TestHelpers
