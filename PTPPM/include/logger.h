#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <memory>
#include <string>

class Logger {
private:
    static std::shared_ptr<spdlog::logger> logger_;
    static bool initialized_;

    Logger() = delete;
    ~Logger() = delete;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

public:
    static void init(const std::string& loggerName = "ptppm_logger", 
                    const std::string& logFile = "logs/ptppm.log",
                    spdlog::level::level_enum level = spdlog::level::info) {
        if (initialized_) return;

        try {
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_level(level);
            
            auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                logFile, 10 * 1024 * 1024, 5);
            file_sink->set_level(level);
            
            std::vector<spdlog::sink_ptr> sinks {console_sink, file_sink};
            logger_ = std::make_shared<spdlog::logger>(loggerName, sinks.begin(), sinks.end());
            logger_->set_level(level);
            
            logger_->set_pattern("[PTPPM] [%H:%M:%S] [%^%l%$] %v");
            
            spdlog::set_default_logger(logger_);
            
            initialized_ = true;
        } catch (const spdlog::spdlog_ex& ex) {
            std::cerr << "Erreur d'initialisation du logger: " << ex.what() << std::endl;
        }
    }

    static void shutdown() {
        spdlog::shutdown();
        initialized_ = false;
    }

    template<typename... Args>
    static void trace(const char* fmt, const Args&... args) {
        if (!initialized_) init();
        logger_->trace(fmt, args...);
    }

    template<typename... Args>
    static void debug(const char* fmt, const Args&... args) {
        if (!initialized_) init();
        logger_->debug(fmt, args...);
    }

    template<typename... Args>
    static void info(const char* fmt, const Args&... args) {
        if (!initialized_) init();
        logger_->info(fmt, args...);
    }

    template<typename... Args>
    static void warn(const char* fmt, const Args&... args) {
        if (!initialized_) init();
        logger_->warn(fmt, args...);
    }

    template<typename... Args>
    static void error(const char* fmt, const Args&... args) {
        if (!initialized_) init();
        logger_->error(fmt, args...);
    }

    template<typename... Args>
    static void critical(const char* fmt, const Args&... args) {
        if (!initialized_) init();
        logger_->critical(fmt, args...);
    }

    static void setLevel(spdlog::level::level_enum level) {
        if (!initialized_) init();
        logger_->set_level(level);
    }
};

std::shared_ptr<spdlog::logger> Logger::logger_ = nullptr;
bool Logger::initialized_ = false;
