// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "test_helpers.hpp"

#include <httplib.h>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <utility>

namespace TestHelpers {

HttpServer::HttpServer(std::string body, int statusCode, std::chrono::milliseconds responseDelay)
	: server_(std::make_unique<httplib::Server>()),
	  port_(server_->bind_to_any_port("127.0.0.1"))
{
	if (port_ <= 0) {
		std::cerr << "Failed to bind test HTTP server\n";
		std::exit(EXIT_FAILURE);
	}

	server_->Get("/", [body = std::move(body), statusCode, responseDelay](const httplib::Request &,
									      httplib::Response &response) {
		std::this_thread::sleep_for(responseDelay);
		response.status = statusCode;
		response.set_content(body, "text/plain");
	});

	worker_ = std::thread([this] {
		if (!server_->listen_after_bind()) {
			std::cerr << "Failed to start test HTTP server\n";
		}
	});
	server_->wait_until_ready();
}

HttpServer::~HttpServer() noexcept
{
	server_->stop();
	if (worker_.joinable()) {
		worker_.join();
	}
}

std::string HttpServer::url() const
{
	return "http://127.0.0.1:" + std::to_string(port_) + '/';
}

} // namespace TestHelpers
