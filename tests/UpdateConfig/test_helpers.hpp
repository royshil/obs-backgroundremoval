// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <httplib.h>

#include <chrono>
#include <memory>
#include <string>
#include <thread>

namespace TestHelpers {

class HttpServer final {
public:
	explicit HttpServer(std::string body, int statusCode = 200,
			    std::chrono::milliseconds responseDelay = std::chrono::milliseconds::zero());
	~HttpServer() noexcept;

	HttpServer(const HttpServer &) = delete;
	HttpServer &operator=(const HttpServer &) = delete;
	HttpServer(HttpServer &&) = delete;
	HttpServer &operator=(HttpServer &&) = delete;

	[[nodiscard]] std::string url() const;

private:
	std::unique_ptr<httplib::Server> server_;
	int port_ = 0;
	std::thread worker_;
};

} // namespace TestHelpers
