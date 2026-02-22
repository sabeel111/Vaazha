#pragma once
#include <iostream>
#include <string>
#include <mutex>

namespace agent::core::logging {

    // 1. Define Log Levels
    enum class LogLevel {
        DEBUG,
        INFO,
        WARN,
        ERROR
    };

    // 2. Global Logger Setup
    class Logger {
    public:
        // Singleton access so the whole app shares one logger
        static Logger& get() {
            static Logger instance;
            return instance;
        }

        void set_run_id(const std::string& id) {
            std::lock_guard<std::mutex> lock(mutex_);
            run_id_ = id;
        }

        void log(LogLevel level, const std::string& message) {
            std::lock_guard<std::mutex> lock(mutex_); // Thread safety!

            std::cout << "[" << level_to_string(level) << "] "
                      << (run_id_.empty() ? "" : "[" + run_id_ + "] ")
                      << message << std::endl;
        }

    private:
        Logger() = default;
        std::mutex mutex_;
        std::string run_id_;

        std::string level_to_string(LogLevel level) {
            switch (level) {
                case LogLevel::DEBUG: return "DEBUG";
                case LogLevel::INFO:  return "INFO ";
                case LogLevel::WARN:  return "WARN ";
                case LogLevel::ERROR: return "ERROR";
                default: return "UNKNOWN";
            }
        }
    };

    // 3. Helper macros for clean syntax everywhere else in your code
    #define LOG_DEBUG(msg) agent::core::logging::Logger::get().log(agent::core::logging::LogLevel::DEBUG, msg)
    #define LOG_INFO(msg)  agent::core::logging::Logger::get().log(agent::core::logging::LogLevel::INFO, msg)
    #define LOG_WARN(msg)  agent::core::logging::Logger::get().log(agent::core::logging::LogLevel::WARN, msg)
    #define LOG_ERROR(msg) agent::core::logging::Logger::get().log(agent::core::logging::LogLevel::ERROR, msg)

} // namespace agent::core::logging
