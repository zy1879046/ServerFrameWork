//
// Created by 张羽 on 25-4-9.
//

#ifndef LOG_H
#define LOG_H

#include <iostream>
#include <string>
#include <memory>
#include <sstream>
#include <cstdint>
#include <vector>
#include <fstream>
#include <map>
#include "Singleton.h"
#include "mutex.h"
#include "util.h"
#define LOG_LEVEL(logger, level) \
    if(logger->get_level() <= level) \
        sylar::LogEventWrap(sylar::LogEvent::ptr(new sylar::LogEvent(logger, level, __FILE__, __LINE__, sylar::get_thread_id(), sylar::get_cur_fiber_id(), time(0)))).get_ss()
#define LOG_DEBUG(logger) LOG_LEVEL(logger, sylar::LogLevel::DEBUG)
#define LOG_INFO(logger) LOG_LEVEL(logger, sylar::LogLevel::INFO)
#define LOG_WARN(logger) LOG_LEVEL(logger, sylar::LogLevel::WARN)
#define LOG_ERROR(logger) LOG_LEVEL(logger, sylar::LogLevel::ERROR)
#define LOG_FATAL(logger) LOG_LEVEL(logger, sylar::LogLevel::FATAL)

#define LOG_MGR() sylar::LogManager::GetInstance()
#define LOG_ROOT() sylar::LogManager::GetInstance()->get_root_logger()
#define LOG_NAME(name) sylar::LogManager::GetInstance()->get_logger(name)
#define LOG_ROOT_DEBUG() LOG_DEBUG(LOG_ROOT())
#define LOG_ROOT_INFO() LOG_INFO(LOG_ROOT())
#define LOG_ROOT_WARN() LOG_WARN(LOG_ROOT())
#define LOG_ROOT_ERROR() LOG_ERROR(LOG_ROOT())
#define LOG_ROOT_FATAL() LOG_FATAL(LOG_ROOT())

#define LOG_SYSTEM_DEBUG() LOG_DEBUG(LOG_NAME("system"))
#define LOG_SYSTEM_INFO() LOG_INFO(LOG_NAME("system"))
#define LOG_SYSTEM_WARN() LOG_WARN(LOG_NAME("system"))
#define LOG_SYSTEM_ERROR() LOG_ERROR(LOG_NAME("system"))
#define LOG_SYSTEM_FATAL() LOG_FATAL(LOG_NAME("system"))
namespace sylar
{
    class LogEvent;//该类用来声明事件，该事件中存放日志信息，后续会被输入到日志器中
    class Logger;//该类用来声明日志器，日志器中存放日志输出路径
    class LogLevel;//该类用来声明日志级别，日志级别有DEBUG、INFO、WARN、ERROR、FATAL等
    class LogAppender;//该类用来声明日志输出路径，日志输出路径存在日志输出格式
    class LogFormatter;//该类用来声明日志输出格式，日志输出格式支持log4j格式

    class LogEventWrap;//该类用来声明日志事件包装器，日志事件包装器中存放日志事件,在析构时将日志事件输出到日志器中
    class LogLevel
    {
    public:
        enum Level
        {
            UNKNOW = 0,//未知
            DEBUG,//调试
            INFO,//信息
            WARN,//警告
            ERROR,//错误
            FATAL,//致命错误
        };
        static std::string ToString(Level level);
        static Level FromString(const std::string& str);
    };
    class LogEvent {
    public:
        using ptr = std::shared_ptr<LogEvent>;
        LogEvent(std::shared_ptr<Logger>logger,LogLevel::Level level,std::string  fileName,
            uint32_t line, uint32_t threadId,
            uint32_t fiberId, uint64_t time );
        std::string get_file_name() const;
        uint64_t get_time() const;
        uint32_t get_thread_id() const;
        uint32_t get_fiber_id() const;
        uint32_t get_line() const;
        LogLevel::Level get_level() const;
        void set_level(LogLevel::Level m_level);
        std::string get_thread_name() const;
        std::stringstream& get_ss();
        std::string get_content() const;
        std::shared_ptr<Logger> get_logger() const;
    private:
        std::shared_ptr<Logger> m_logger; //日志器
        std::string m_fileName; //文件名
        uint64_t m_time; //时间戳
        uint32_t m_threadId; //线程ID
        uint32_t m_fiberId; //协程ID
        uint32_t m_line; //行号
        LogLevel::Level m_level;//日志级别
        std::string m_threadName; //线程名称
        std::stringstream m_ss; //日志内容
    };
    //日志包装器，析构时将日志写入到logger中
    class LogEventWrap
    {
    public:
        explicit LogEventWrap(LogEvent::ptr event);
        ~LogEventWrap();
        std::stringstream& get_ss();
    private:
        LogEvent::ptr m_event;//日志事件
    };
    // 格式化器
    class LogFormatter
    {
    public:
        using ptr = std::shared_ptr<LogFormatter>;
        explicit LogFormatter(std::string pattern);
        std::string format(const LogEvent::ptr& event);
        bool isError()const{return m_error;}
        std::string get_pattern()const{return m_pattern;}
        class FormatItem
        {
        public:
            using ptr = std::shared_ptr<FormatItem>;
            virtual ~FormatItem() = default;
            virtual void format(std::stringstream& os,LogEvent::ptr event) = 0;
        };
        void init();//将pattern中的格式化项解析出来到m_items中
    private:
        std::string m_pattern; //日志输出格式
        std::vector<FormatItem::ptr> m_items; //日志输出格式化项
        bool m_error = false; //日志格式是否有错误
    };
    class Logger
    {
    public:
        using ptr = std::shared_ptr<Logger>;
        Logger(std::string name = "root");
        //向appender中输出日志
        void log(LogLevel::Level level, const LogEvent::ptr& event);
        void debug(const LogEvent::ptr& event);
        void info(const LogEvent::ptr& event);
        void warn(const LogEvent::ptr& event);
        void error(const LogEvent::ptr& event);
        void fatal(const LogEvent::ptr& event);
        void add_appender(std::shared_ptr<LogAppender> appender);
        void del_appender(std::shared_ptr<LogAppender> appender);
        void clear_appender();
    public:
        std::string get_name() const;
        void set_name(const std::string& m_name);
        LogLevel::Level get_level() const;
        void set_level(LogLevel::Level m_level);
        LogFormatter::ptr get_formatter() const;
        void set_formatter(const LogFormatter::ptr& m_formatter);
        void set_formatter(const std::string pattern);
        std::string to_yaml_string() const;
    private:
        std::string m_name; //日志器名称
        LogLevel::Level m_level; //日志级别
        LogFormatter::ptr m_formatter; //日志输出格式
        std::vector<std::shared_ptr<LogAppender>> m_appenders; //日志输出器
        mutable Mutex m_mutex;
    };

    class LogAppender
    {
    public:
        using ptr = std::shared_ptr<LogAppender>;
        explicit LogAppender(LogLevel::Level level,std::string pattern,bool has_formatter, bool has_level);
        virtual ~LogAppender() = default;
        virtual void log(LogLevel::Level level,const LogEvent::ptr & event) = 0;//日志输出
        virtual std::string to_yaml_string()const = 0;

    public:
        LogFormatter::ptr get_formatter() const;
        void set_formatter(const LogFormatter::ptr& m_formatter);
        void set_formatter(const std::string& pattern);
        LogLevel::Level get_level() const;
        void set_level(const LogLevel::Level m_level);
        bool has_formatter()const;
        void set_has_formatter(bool has_formatter);
        bool has_level()const;
        void set_has_level(bool has_level);
    protected:
        LogFormatter::ptr m_formatter;//日志输出格式
        LogLevel::Level m_level;//Appender日志级别
        bool m_has_formatter = false;
        bool m_has_level = false;
        mutable Mutex m_mutex;
    };



    //终端Appender
    class StdOutLogAppender : public LogAppender
    {
    public:
        using ptr = std::shared_ptr<StdOutLogAppender>;
        explicit StdOutLogAppender(LogLevel::Level level = LogLevel::DEBUG,std::string pattern = "%d{%Y-%m-%d %H:%M:%S}%T%N%T%t%T%F%T[%p]%T[%c]%T%f:%l%T%m%n",
            bool has_formatter = false,bool has_level = false);
        void log(LogLevel::Level level, const LogEvent::ptr& event) override;
        std::string to_yaml_string()const override;
    };
    //文件Appender
    class FileLogAppender : public LogAppender
    {
    public:
        using ptr = std::shared_ptr<FileLogAppender>;
        explicit FileLogAppender(std::string fileName, LogLevel::Level level = LogLevel::DEBUG,
            std::string pattern = "%d{%Y-%m-%d %H:%M:%S}%T%N%T%t%T%F%T[%p]%T[%c]%T%f:%l%T%m%n",
            bool has_formatter = false,bool has_level = false);
        ~FileLogAppender() override;
        void log(LogLevel::Level level, const LogEvent::ptr& event) override;
        std::string to_yaml_string()const override;
        bool reopen();

    private:
        std::string m_fileName;
        std::ofstream m_ofstream;
        uint64_t m_lastTime = 0;
    };
    class LogManager :public Singleton<LogManager>
    {
        friend class Singleton<LogManager>;
    public:
        LogManager();
        ~LogManager();
        Logger::ptr get_logger(const std::string& name,LogLevel::Level level = LogLevel::DEBUG);
        void add_logger(const std::string& name, Logger::ptr logger);
        void remove_logger(const std::string& name);
        Logger::ptr get_root_logger() const;

        std::string to_yaml_string() const;
    private:
        std::map<std::string, Logger::ptr> m_loggers;
        Logger::ptr m_rootLogger;
        mutable Mutex m_mutex;
    };
}
#endif //LOG_H
