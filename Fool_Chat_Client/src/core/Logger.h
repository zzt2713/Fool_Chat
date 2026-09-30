#ifndef LOGGER_H
#define LOGGER_H
/*
 *  包含头文件:
 *  #include "Logger.h"
 *
 *  LOG_DEBUG("调试信息");
 *  LOG_INFO("用户登录成功");
 *  LOG_WARN("密码格式不正确");
 *  LOG_ERROR("数据库连接失败");
 *
 *  Logger::getInstance().clear();           // 清空日志文件内容
 *  Logger::getInstance().deleteLogFile();   // 删除日志文件
 *
 *  日志级别:
 *  DEBUG   - 调试信息，开发阶段使用
 *  INFO    - 一般信息，正常流程记录
 *  WARNING - 警告信息，不影响运行但需要注意
 *  ERROR   - 错误信息，需要处理的异常情况
 *
 *  日志文件:
 *  写入当前目录 log/ 文件夹，按天滚动: log/foolchat_YYYYMMDD.log
 *  格式: [级别] [时间戳] 日志内容
 *  示例: [INFO] [2026-05-20 15:30:45] 用户登录成功
 */
#define LOG Logger::getInstance()
#define LOG_DEBUG(msg) Logger::getInstance() << DEBUG  << msg << Logger::endl
#define LOG_INFO(msg)  Logger::getInstance() << INFO << msg << Logger::endl
#define LOG_WARN(msg)  Logger::getInstance() << WARNING << msg << Logger::endl
#define LOG_ERROR(msg) Logger::getInstance() << ERROR << msg << Logger::endl
#define ADDLOG(msg,level) Logger::getInstance() << level << msg << Logger::endl

#include <iostream>
#include <fstream>
#include <string>
#include <mutex>
#include <filesystem>

enum level {
    DEBUG,
    INFO,
    WARNING,
    ERROR
};

class Logger {
public:
    static Logger& getInstance();
    ~Logger();
    class EndLog {};

    Logger(const Logger& ) = delete;
    Logger& operator=(const Logger& ) = delete;

    Logger& operator<<(level lev);
    Logger& operator<<(const std::string& msg);
    Logger& operator<<(const EndLog&);
    Logger& operator<<(int value);

    void log(const std::string& msg,level lev = DEBUG);
    friend Logger operator<<(level levconst ,std::string& msg);

    void clear();
    bool deleteLogFile();
    // 崩溃捕获：Qt 消息、terminate、致命信号统一落盘（main 启动时调用一次）
    static void installCrashHook();

    static EndLog endl;

private:
    Logger();
    std::string currentLogPath();
    void reopenIfRollover();
    std::ofstream logFile_;
    std::mutex mutex_;
    std::string logDate_; // 当前日志文件归属日期（YYYYMMDD），跨天滚动
};



#endif //LOGGER_H
