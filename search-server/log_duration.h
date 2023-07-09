#pragma once

#include <iostream>
#include <chrono>
#include <string>

#define PROFILE_CONCAT_INTERNAL(X, Y) X##Y
#define PROFILE_CONCAT(X, Y) PROFILE_CONCAT_INTERNAL(X, Y)
#define UNIQUE_VAR_NAME_PROFILE PROFILE_CONCAT(ProfileGuard_, __LINE__)
#define LOG_DURATION(x) LogDuration UNIQUE_VAR_NAME_PROFILE(x)

#define UNIQUE_VAR_NAME_PROFILE_STREAM PROFILE_CONCAT(porfileGuard_, __LINE__)
#define LOG_DURATION_STREAM(x, out_stream) LogDuration UNIQUE_VAR_NAME_PROFILE_STREAM(x, out_stream)

class LogDuration {
public:
	LogDuration(const std::string& id)
		: id_(id) {
	}
	
	LogDuration(const std::string& id, std::ostream& os)
		: id_(id)
		, out_(os){
	}

	using Clock = std::chrono::steady_clock;

	~LogDuration() {
		using namespace std::chrono;
		using namespace std::literals;

		const auto end_time = Clock::now();
		const auto dur = end_time - start_time_;
		out_ << "Operation time : "s
			<< duration_cast<milliseconds>(dur).count() << " ms"s << std::endl;
	}
private:
	const std::string id_;
	std::ostream& out_ = std::cerr;
	const Clock::time_point start_time_ = Clock::now();
};