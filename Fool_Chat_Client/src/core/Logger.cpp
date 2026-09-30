//
// Created by yasalzzt on 2025/12/17.
//
#include "Logger.h"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <mutex>
#include <chrono>
#include <ctime>
#include <csignal>
#include <exception>
#include <QMessageLogContext>
#include <QString>

Logger::EndLog Logger::endl;
static thread_local std::string t_curMsg;
static thread_local level t_curLev = DEBUG;

Logger& Logger::getInstance() {
    static Logger log;
    return log;
}

static std::string TodayStr()
{
    auto now = std::chrono::system_clock::now();
    std::time_t time = std::chrono::system_clock::to_time_t(now);
    char buffer[16];
    std::strftime(buffer, sizeof(buffer), "%Y%m%d", std::localtime(&time));
    return buffer;
}

std::string Logger::currentLogPath()
{
    return "log/foolchat_" + TodayStr() + ".log";
}

// 文件名按日期字典序即时间序，只清理 foolchat_YYYYMMDD.log，超 30 个删最旧
static void CleanupOldLogs(const std::string& keepFileName)
{
    const int kMaxLogFiles = 30;
    std::error_code ec;
    std::vector<std::string> logs;
    for (const auto& entry : std::filesystem::directory_iterator("log", ec)) {
        if (!entry.is_regular_file(ec)) {
            continue;
        }
        const std::string name = entry.path().filename().string();
        if (name.rfind("foolchat_", 0) == 0 && name.size() > 4
            && name.compare(name.size() - 4, 4, ".log") == 0 && name != keepFileName) {
            logs.push_back(name);
        }
    }
    if (static_cast<int>(logs.size()) < kMaxLogFiles) {
        return;
    }
    std::sort(logs.begin(), logs.end());
    // 保留最新 kMaxLogFiles-1 个 + 当天正在写的，其余删除
    const int removeCount = static_cast<int>(logs.size()) - (kMaxLogFiles - 1);
    for (int i = 0; i < removeCount; ++i) {
        std::filesystem::remove("log/" + logs[i], ec);
    }
}

Logger::Logger() {
    std::error_code ec;
    std::filesystem::create_directories("log", ec);
    logDate_ = TodayStr();
    CleanupOldLogs("foolchat_" + logDate_ + ".log");
    logFile_.open(currentLogPath(), std::ios::app);
    if (!logFile_.is_open()) {
        throw std::runtime_error("Logger could not open log file");
    }
}

void Logger::reopenIfRollover()
{
    const std::string today = TodayStr();
    if (today == logDate_) {
        return;
    }
    if (logFile_.is_open()) {
        logFile_.close();
    }
    logDate_ = today;
    CleanupOldLogs("foolchat_" + logDate_ + ".log");
    logFile_.open(currentLogPath(), std::ios::app);
}

// Qt/控件库内部警告与业务无关，落盘只会刷屏
static bool isFrameworkNoise(const QString& msg)
{
    static const char* const kPrefixes[] = {
        "QFont::setPixelSize",
        "Could not parse stylesheet of object",
    };
    for (const char* prefix : kPrefixes) {
        if (msg.startsWith(QLatin1String(prefix))) {
            return true;
        }
    }
    return false;
}

void Logger::installCrashHook()
{
    // Qt 消息统一落盘（qDebug/qWarning/qCritical 全走 Logger）
    qInstallMessageHandler([](QtMsgType type, const QMessageLogContext&, const QString& msg) {
        if (isFrameworkNoise(msg)) {
            return;
        }
        level lev = DEBUG;
        switch (type) {
        case QtInfoMsg:    lev = INFO; break;
        case QtWarningMsg: lev = WARNING; break;
        case QtCriticalMsg:
        case QtFatalMsg:   lev = ERROR; break;
        default:           lev = DEBUG; break;
        }
        Logger::getInstance().log(msg.toStdString(), lev);
    });

    // 未捕获异常
    std::set_terminate([]() {
        Logger::getInstance().log("terminate called: unhandled exception", ERROR);
        std::abort();
    });

    // 致命信号：先落盘再按默认处理
    for (int sig : {SIGSEGV, SIGABRT, SIGFPE, SIGILL}) {
        std::signal(sig, [](int s) {
            Logger::getInstance().log("fatal signal " + std::to_string(s), ERROR);
            std::signal(s, SIG_DFL);
            std::raise(s);
        });
    }
}

void Logger::clear()
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (logFile_.is_open()) {
        logFile_.close();
    }

    logFile_.open(currentLogPath(), std::ios::out | std::ios::trunc);
}

Logger::~Logger() {
    if (logFile_.is_open()) {
        logFile_.close();
    }
}

Logger &Logger::operator <<(level lev){
    t_curLev = lev;
    return *this;
}

Logger &Logger::operator<<(const std::string& msg){
    t_curMsg += msg;
    return *this;
}

Logger& Logger::operator<<(const EndLog&) {
    log(t_curMsg, t_curLev);
    t_curMsg.clear();
    t_curLev = DEBUG;
    return *this;
}

Logger& Logger::operator<<(int value) {
    t_curMsg += std::to_string(value);
    return *this;
}

void Logger::log(const std::string &msg, level lev)
{
    std::lock_guard<std::mutex> lock(mutex_);
    reopenIfRollover();
    std::string level_str;
    switch (lev) {
    case DEBUG:
        level_str = "[DEBUG] ";
        break;
    case INFO:
        level_str = "[INFO] ";
        break;
    case WARNING:
        level_str = "[WARNING] ";
        break;
    case ERROR:
        level_str = "[ERROR] ";
        break;
    }
    if (logFile_.is_open()) {
        auto now = std::chrono::system_clock::now();
        std::time_t time = std::chrono::system_clock::to_time_t(now);
        char buffer[80];
        std::strftime(buffer, sizeof(buffer), "[%Y-%m-%d %H:%M:%S] ", std::localtime(&time));

        logFile_ << level_str << buffer << msg << std::endl;
    }
}

bool Logger::deleteLogFile() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (logFile_.is_open()) {
        logFile_.close();
    }

    bool result = std::filesystem::remove(currentLogPath());

    logFile_.open(currentLogPath(), std::ios::app);
    return result;
}

