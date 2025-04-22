//
// Created by 张羽 on 25-4-9.
//
#include "log.h"
#include <ctime>
#include <iomanip>
#include <utility>
#include <vector>
#include <tuple>
#include <map>
#include <functional>
#include <sstream>
#include <ostream>
#include <algorithm>
#include <filesystem>
#include "config.h"
#include "mutex.h"
#include "Thread.h"
namespace sylar
{
std::string LogLevel::ToString(Level level)
{
    switch(level)
    {
    #define XX(item) \
        case item: \
            return #item; \
            break;
        XX(DEBUG)
        XX(INFO)
        XX(WARN)
        XX(ERROR)
        XX(FATAL)
    #undef XX
        default:
            return "UNKNOW";
            break;
    }
}

LogLevel::Level LogLevel::FromString(const std::string& str)
{
#define XX(name,value) \
    if(str == #value) \
        return LogLevel::name
    XX(DEBUG,debug);
    XX(INFO,info);
    XX(WARN,warn);
    XX(ERROR,error);
    XX(FATAL,fatal);
    XX(DEBUG,DEBUG);
    XX(INFO,INFO);
    XX(WARN,WARN);
    XX(ERROR,ERROR);
    XX(FATAL,FATAL);
    return LogLevel::UNKNOW;
#undef XX
}

LogEvent::LogEvent(std::shared_ptr<Logger> logger,LogLevel::Level level, std::string  fileName,
                       uint32_t line, uint32_t threadId, uint32_t fiberId, uint64_t time)
    :m_logger(std::move(logger)),m_fileName(std::move(fileName)),m_time(time),
    m_threadId(threadId),m_fiberId(fiberId),m_line(line),m_level(level),m_threadName(Thread::get_thread_name())
{
}

std::string LogEvent::get_file_name() const
{
    return m_fileName;
}

uint64_t LogEvent::get_time() const
{
    return m_time;
}

uint32_t LogEvent::get_thread_id() const
{
    return m_threadId;
}

uint32_t LogEvent::get_fiber_id() const
{
    return m_fiberId;
}

uint32_t LogEvent::get_line() const
{
    return m_line;
}

LogLevel::Level LogEvent::get_level() const
{
    return m_level;
}

void LogEvent::set_level(LogLevel::Level level)
{
    m_level = level;
}

std::string LogEvent::get_thread_name() const
{
    return m_threadName;
}

std::stringstream& LogEvent::get_ss()
{
    return m_ss;
}

std::string LogEvent::get_content() const
{
    return m_ss.str();
}

std::shared_ptr<Logger> LogEvent::get_logger() const
{
    return m_logger;
}

LogEventWrap::LogEventWrap(LogEvent::ptr event)
    :m_event(std::move(event))
{
}

LogEventWrap::~LogEventWrap()
{
    m_event->get_logger()->log(m_event->get_level(),m_event);
}

std::stringstream& LogEventWrap::get_ss()
{
    return m_event->get_ss();
}

LogFormatter::LogFormatter(std::string pattern)
    :m_pattern(std::move(pattern)),m_error(false)
{
    init();
}

std::string LogFormatter::format(const LogEvent::ptr& event)
{
    std::stringstream ss;
    for (auto& item : m_items)
    {
        item->format(ss, event);
    }
    return ss.str();
}

/*
 * 将日志事件中的ss内容写入到日志器中
 */
class MessageFormatItem final : public LogFormatter::FormatItem
{
public:
    explicit MessageFormatItem(const std::string& fmt = "") {};
    ~MessageFormatItem() override = default;
    void format(std::stringstream& os, const LogEvent::ptr event) override
    {
        os << event->get_content();
    }
};

class LevelFormatItem final : public LogFormatter::FormatItem
{
public:
    explicit LevelFormatItem(const std::string& fmt = "") {}
    ~LevelFormatItem() override = default;
    void format(std::stringstream& os, const LogEvent::ptr event) override
    {
        os << LogLevel::ToString(event->get_level());
    }
};

class LoggerNameFormatItem final : public LogFormatter::FormatItem
{
public:
    explicit LoggerNameFormatItem(const std::string& fmt = "") {}
    ~LoggerNameFormatItem() override = default;
    void format(std::stringstream& os, const LogEvent::ptr event) override
    {
        os << event->get_logger()->get_name();
    }
};
class ThreadIdFormatItem final : public LogFormatter::FormatItem
{
public:
    explicit ThreadIdFormatItem(const std::string& fmt = "") {}
    ~ThreadIdFormatItem() override = default;
    void format(std::stringstream& os, const LogEvent::ptr event) override
    {
        os << event->get_thread_id();
    }
};

class CLineFormatItem final : public LogFormatter::FormatItem
{
public:
    explicit CLineFormatItem(const std::string& fmt = ""){}
    ~CLineFormatItem() override = default;
    void format(std::stringstream& os, const LogEvent::ptr event) override
    {
        os << "\n";
    }
};

class LineFormatItem final : public LogFormatter::FormatItem
{
public:
    explicit LineFormatItem(const std::string& fmt = "") {}
    ~LineFormatItem() override = default;
    void format(std::stringstream& os, const LogEvent::ptr event) override
    {
        os << event->get_line();
    }
};

class FiberIdFormatItem final : public LogFormatter::FormatItem
{
public:
    explicit FiberIdFormatItem(const std::string& fmt = ""){}
    ~FiberIdFormatItem() override = default;
    void format(std::stringstream& os, const LogEvent::ptr event) override
    {
        os << event->get_fiber_id();
    }
};

class TabFormatItem final : public LogFormatter::FormatItem
{
public:
    explicit TabFormatItem(const std::string& fmt = "") {}
    ~TabFormatItem() override = default;
    void format(std::stringstream& os, const LogEvent::ptr event) override
    {
        os << "\t";
    }
};

class FileNameFormatItem final : public LogFormatter::FormatItem
{
public:
    explicit FileNameFormatItem(const std::string& fmt = ""){}
    ~FileNameFormatItem() override = default;
    void format(std::stringstream& os, const LogEvent::ptr event) override
    {
        os << event->get_file_name();
    }
};

class ThreadNameFormatItem final : public LogFormatter::FormatItem
{
public:
    explicit ThreadNameFormatItem(const std::string& fmt = ""){}
    ~ThreadNameFormatItem() override = default;
    void format(std::stringstream& os, const LogEvent::ptr event) override
    {
        os << event->get_thread_name();
    }
};

class StringFormatItem final : public LogFormatter::FormatItem
{
public:
    explicit StringFormatItem(const std::string& fmt):m_str(fmt){}
    ~StringFormatItem() override = default;
    void format(std::stringstream& os, const LogEvent::ptr event) override
    {
        os << m_str;
    }
private:
    std::string m_str;
};

class DateTimeFormatItem final : public LogFormatter::FormatItem
{
public:
    explicit DateTimeFormatItem(const std::string& fmt = "%Y-%m-%d %H:%M:%S"):m_format(fmt)
    {
        if (fmt.empty())m_format = "%Y-%m-%d %H:%M:%S";
    }
    ~DateTimeFormatItem() override = default;
    void format(std::stringstream& os, const LogEvent::ptr event) override
    {

        auto time = static_cast<std::time_t>(event->get_time());
        std::tm* localTime = std::localtime(&time);
        if (localTime)
        {
            auto str = std::put_time(localTime, m_format.c_str());
            os << str;
        }
    }
private:
    std::string m_format;
};
/*
 * %m: 消息内容
 * %p: 日志级别
 * %c: 日志器名称
 * %f: 文件名
 * %l: 行号
 * %t: 线程ID
 * %F: 协程ID
 * %T: Tab
 * %n: 换行
 * %d: 时间
 * %N: 线程名称ß
 */
void LogFormatter::init()
{
    //创建一个数组存储格式化项，类似 <m,fmt,type>, type = 0 ;表示 str类型 ，type = 1 表示格式化项 %% 表示 一个 %
    std::vector<std::tuple<std::string,std::string,int>> vec;
    std::string nstr;//记录当前str
    std::size_t index = 0;//记录当前位置
    while (index < m_pattern.size())
    {
        //从index开始找到的第一个%的位置
        auto pos = m_pattern.find_first_of("%", index);
        //当前位置大于index，则表示前面是普通字符，将其存在nstr中，并存在vec中
        if (pos > index)
        {
            nstr += m_pattern.substr(index, pos - index);
            vec.emplace_back(nstr, std::string(), 0);
            nstr.clear();
        }
        //如果没有找到%，则表示结束
        if (pos == std::string::npos)
        {
            break;
        }
        //如果找到了%，则判断下一个字符
        if (pos + 1 < m_pattern.size())
        {
            //如果下一个字符是%，则表示是一个%，将其存入nstr中
            if (m_pattern[pos + 1] == '%')
            {
                nstr += '%';
                vec.emplace_back(nstr, std::string(), 0);
                index = pos + 2;
                nstr.clear();
                continue;
            }
            //如果下一个字符不是%，则表示是格式化项
            else
            {
                //记录下一个格式字符，以及判断下下一个字符是不是 {，是则代表有格式化信息，不是则表示没有
                std::string fmt = std::string();
                nstr += m_pattern[pos + 1];
                if (pos + 2 < m_pattern.size())
                {
                    //如果下下一个字符不是{，则表示没有格式化信息
                    if (m_pattern[pos + 2] == '{')
                    {
                        //从 pos + 3开始找到下一个}的位置
                        auto endPos = m_pattern.find_first_of("}", pos + 3);
                        fmt = m_pattern.substr(pos + 3, endPos - pos - 3);
                        //如果没有找到}，则表示格式化信息不完整，存储type = 0
                        if (endPos == std::string::npos)
                        {
                            vec.emplace_back("<parse_error>", fmt, 0);
                            nstr.clear();
                            index = endPos;
                            m_error = true;
                            continue;
                        }else//完整则将格式化信息存进去
                        {
                            vec.emplace_back(nstr, fmt, 1);
                            nstr.clear();
                            //更新index为 } 后面的位置
                            index = endPos + 1;
                            continue;;
                        }
                    }
                    //没有格式化信息
                    vec.emplace_back(nstr, fmt, 2);
                    nstr.clear();
                    index = pos + 2;
                }else
                {
                    index += 2;
                    vec.emplace_back(nstr, fmt, 2);
                    nstr.clear();
                }
            }
        }
        else
        {
            //如果下一个字符是结束,有 % 却没有格式表示parse_error
            vec.emplace_back("<parse_error>", std::string("%"), 0);
            m_error = true;
            break;
        }
    }
    std::map<std::string,std::function<FormatItem::ptr(const std::string&)>> fmtMap={
#define XX(str,C)\
    {#str,[](const std::string& fmt) { return FormatItem::ptr(new C(fmt));}}
      XX(m,MessageFormatItem),
        XX(p,LevelFormatItem),
        XX(c,LoggerNameFormatItem),
        XX(f,FileNameFormatItem),
        XX(l,LineFormatItem),
        XX(t,ThreadIdFormatItem),
        XX(F,FiberIdFormatItem),
        XX(T,TabFormatItem),
        XX(n,CLineFormatItem),
        XX(d,DateTimeFormatItem),
        XX(N,ThreadNameFormatItem),
#undef XX
    };
    //从vec中解析出格式化项
    for (auto& item : vec)
    {
        //根据type判断格式化项
        if (std::get<2>(item) == 0)
        {
            //普通字符
            m_items.emplace_back(FormatItem::ptr(new StringFormatItem(std::get<0>(item))));
        }
        else
        {
            //格式化项
            auto it = fmtMap.find(std::get<0>(item));
            if (it != fmtMap.end())
            {
                m_items.emplace_back(it->second(std::get<1>(item)));
            }
            else
            {
                m_items.emplace_back(FormatItem::ptr(new StringFormatItem("<<error_format %" + std::get<0>(item) + ">>")));
                m_error = true;
            }
        }
    }
}

Logger::Logger(std::string name):m_name(std::move(name)),m_level(LogLevel::DEBUG)
{
    m_formatter.reset(new LogFormatter("%d{%Y-%m-%d %H:%M:%S}%T%N%T%t%T%F%T[%p]%T[%c]%T%f:%l%T%m%n"));
}

void Logger::log(LogLevel::Level level, const LogEvent::ptr& event)
{

    //判断日志级别是否大于等于当前日志器的级别
    if (level >= m_level)
    {
        //遍历所有的appender，输出日志
        for (auto& appender : m_appenders)
        {
            appender->log(level, event);
        }
    }
}

void Logger::debug(const LogEvent::ptr& event)
{
    log(LogLevel::DEBUG, event);
}

void Logger::info(const LogEvent::ptr& event)
{
    log(LogLevel::INFO, event);
}

void Logger::warn(const LogEvent::ptr& event)
{
    log(LogLevel::WARN, event);
}

void Logger::error(const LogEvent::ptr& event)
{
    log(LogLevel::ERROR, event);
}

void Logger::fatal(const LogEvent::ptr& event)
{
    log(LogLevel::FATAL, event);
}

void Logger::add_appender(std::shared_ptr<LogAppender> appender)
{
    LockGuard lock(m_mutex);
    if (!appender)return;
    if (!appender->has_formatter())
    {
        appender->set_formatter(m_formatter);
    }
    if (!appender->has_level())
    {
        appender->set_level(m_level);
    }
    m_appenders.emplace_back(appender);
}

void Logger::del_appender(std::shared_ptr<LogAppender> appender)
{
    LockGuard lock(m_mutex);
    if (!appender)return;
    auto it = std::find(m_appenders.begin(), m_appenders.end(), appender);
    if (it != m_appenders.end())
    {
        m_appenders.erase(it);
    }
}

void Logger::clear_appender()
{
    LockGuard lock(m_mutex);
    m_appenders.clear();
}

std::string Logger::get_name() const
{
    LockGuard lock(m_mutex);
    return m_name;
}

void Logger::set_name(const std::string& name)
{
    LockGuard lock(m_mutex);
    m_name = name;
}

LogLevel::Level Logger::get_level() const
{
    LockGuard lock(m_mutex);
    return m_level;
}

void Logger::set_level(const LogLevel::Level level)
{
    LockGuard lock(m_mutex);
    this->m_level = level;
    //对所有没有默认level的appender设置level
    for (auto& appender : m_appenders)
    {
        if (!appender->has_level())
        {
            appender->set_level(level);
        }
    }
}

LogFormatter::ptr Logger::get_formatter() const
{
    LockGuard lock(m_mutex);
    return m_formatter;
}

void Logger::set_formatter(const LogFormatter::ptr& formatter)
{
    LockGuard lock(m_mutex);
    m_formatter = formatter;
    //对所有没有formatter的appedner进行更改formatter
    for (auto& appender : m_appenders)
    {
        if (!appender->has_formatter())
        {
            appender->set_formatter(formatter);
        }
    }
}

void Logger::set_formatter(const std::string pattern)
{
    set_formatter(std::make_shared<LogFormatter>(pattern));
}

std::string Logger::to_yaml_string() const
{
    LockGuard lock(m_mutex);
    YAML::Node node;
    node["name"] = m_name;
    node["level"] = LogLevel::ToString(m_level);
    node["formatter"] = m_formatter->get_pattern();
    for (auto& appender : m_appenders)
    {
        YAML::Node app = YAML::Node(appender->to_yaml_string());
        node["appenders"].push_back(app);
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
}

LogAppender::LogAppender(const LogLevel::Level level, std::string pattern,bool has_formatter ,bool has_level)
    :m_formatter(nullptr),m_level(level),m_has_formatter(has_formatter),m_has_level(has_level)
{
    if (!pattern.empty())
        m_formatter.reset(new LogFormatter(pattern));
}

LogFormatter::ptr LogAppender::get_formatter() const
{
    LockGuard lock(m_mutex);
    return m_formatter;
}

void LogAppender::set_formatter(const LogFormatter::ptr& formatter)
{
    LockGuard lock(m_mutex);
    m_formatter = formatter;
}

void LogAppender::set_formatter(const std::string& pattern)
{
    set_formatter(std::make_shared<LogFormatter>(pattern));
}

LogLevel::Level LogAppender::get_level() const
{
    LockGuard lock(m_mutex);
    return m_level;
}

void LogAppender::set_level(const LogLevel::Level level)
{
    LockGuard lock(m_mutex);
    m_level = level;
}

bool LogAppender::has_formatter() const
{
    LockGuard lock(m_mutex);
    return m_has_formatter;
}

void LogAppender::set_has_formatter(bool has_formatter)
{
    LockGuard lock(m_mutex);
    m_has_formatter = has_formatter;
}
bool LogAppender::has_level() const
{
    LockGuard lock(m_mutex);
    return m_has_level;
}

void LogAppender::set_has_level(bool has_level)
{
    LockGuard lock(m_mutex);
    m_has_level = has_level;
}

StdOutLogAppender::StdOutLogAppender(LogLevel::Level level, std::string pattern,bool has_formatter,bool has_level)
    :LogAppender(level, std::move(pattern),has_formatter,has_level)
{
}

void StdOutLogAppender::log(LogLevel::Level level, const LogEvent::ptr& event)
{
    LockGuard lock(m_mutex);
    if (level >= m_level)
    {
        std::cout << m_formatter->format(event);
    }
}

std::string StdOutLogAppender::to_yaml_string() const
{
    LockGuard lock(m_mutex);
    YAML::Node node;
    node["type"] = "StdoutLogAppender";
    if (m_has_formatter)
    {
        node["formatter"] = m_formatter->get_pattern();
    }
    if (m_has_level)
        node["level"] = LogLevel::ToString(m_level);
    std::stringstream ss;
    ss << node;
    return ss.str();
}

FileLogAppender::FileLogAppender(std::string fileName, LogLevel::Level level, std::string pattern,bool has_formatter,bool has_level)
    : LogAppender(level, std::move(pattern),has_formatter,has_level), m_fileName(std::move(fileName)), m_ofstream()
{
}

FileLogAppender::~FileLogAppender()
{
    //关闭文件
    if (m_ofstream.is_open())
    {
        m_ofstream.close();
    }
}

void FileLogAppender::log(LogLevel::Level level, const LogEvent::ptr& event)
{
    LockGuard lock(m_mutex);
    if (level >= m_level)
    {
        //打开文件
        uint64_t now = event->get_time();
        if (now > m_lastTime + 3)
        {
            reopen();
        }
        m_lastTime = now;
        if (m_ofstream.is_open())
        {
            m_ofstream << m_formatter->format(event);
        }
        else
        {
            std::cerr << "FileLogAppender::log() failed to open file: " << m_fileName << std::endl;
        }
    }
}

std::string FileLogAppender::to_yaml_string() const
{
    LockGuard lock(m_mutex);
    YAML::Node node;
    node["type"] = "FileLogAppender";
    node["file"] = m_fileName;
    if (m_has_formatter)
    {
        node["formatter"] = m_formatter->get_pattern();
    }
    if (m_has_level) node["level"] = LogLevel::ToString(m_level);
    std::stringstream ss;
    ss << node;
    return ss.str();
}

bool FileLogAppender::reopen()
{
    if (m_ofstream.is_open())
    {
        m_ofstream.close();
    }
    //没有则创建
    std::filesystem::path dir = std::filesystem::path(m_fileName).parent_path();
    if (!std::filesystem::exists(dir)) {
        if (!std::filesystem::create_directories(dir)) {
            std::cerr << "无法创建目录: " << dir << std::endl;
            return 1;
        }
    }
    m_ofstream.open(m_fileName, std::ios::app);
    return m_ofstream.is_open();
}

LogManager::LogManager()
{
    m_rootLogger.reset(new Logger());
    m_rootLogger->add_appender(std::make_shared<StdOutLogAppender>());
    m_rootLogger->set_level(LogLevel::DEBUG);
    m_loggers[m_rootLogger->get_name()] = m_rootLogger;
}

LogManager::~LogManager() = default;
Logger::ptr LogManager::get_logger(const std::string& name,LogLevel::Level level)
{
    LockGuard lock(m_mutex);
    if (m_loggers.count(name) > 0)return m_loggers[name];
    //没有则创建
    Logger::ptr logger(new Logger(name));
    logger->set_level(level);
    // logger->add_appender(std::make_shared<StdOutLogAppender>());
    m_loggers[name] = logger;
    return logger;
}

void LogManager::add_logger(const std::string& name, Logger::ptr logger)
{
    LockGuard lock(m_mutex);
    //先判断logger存不存在
    if (m_loggers.count(name) > 0)
    {
        std::cout << "logger " << name << " already exists" << std::endl;
        return;
    }
    m_loggers[name] = std::move(logger);
}

void LogManager::remove_logger(const std::string& name)
{
    LockGuard lock(m_mutex);
    m_loggers.erase(name);
}

Logger::ptr LogManager::get_root_logger() const
{
    return m_rootLogger;
}

std::string LogManager::to_yaml_string() const
{
    LockGuard lock(m_mutex);
    YAML::Node node;
    for (auto& logger : m_loggers)
    {
        YAML::Node logger_node = YAML::Load(logger.second->to_yaml_string());
        node["logs"].push_back(logger_node);
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
}

//定义yaml文件读取的Appender
struct LogAppenderDefine
{
    int type = 0;//appender类型 ，STDOUT = 1, FILE = 2
    std::string file;//文件名
    std::string formatter;//格式化信息
    LogLevel::Level level = LogLevel::UNKNOW;//日志级别
    bool operator==(const LogAppenderDefine& other) const
    {
        return type == other.type &&
            file == other.file &&
            formatter == other.formatter &&
            level == other.level;
    }
};
    //定义yaml文件中的logger结构体
struct LoggerDefine
{
    LogLevel::Level level = LogLevel::UNKNOW;//日志级别
    std::string name;//logger名称
    std::vector<LogAppenderDefine> appenders;//appender列表
    std::string formatter;//格式化信息
    bool operator==(const LoggerDefine& other) const
    {
        return level == other.level &&
            name == other.name &&
            appenders == other.appenders &&
            formatter == other.formatter;
    }
    //为了在set中进行排序，重载
    bool operator<(const LoggerDefine& other) const
    {
        return name < other.name;
    }
    bool operator!=(const LoggerDefine& other) const
    {
        return !(*this == other);
    }
};

ConfigVar<std::set<LoggerDefine>>::ptr g_log_defines =
    CONFIG_ENV()->lookup("logs", std::set<LoggerDefine>(), "loggers config");



//从 std::string转换为 LoggerDefine
template<>
class LexicalCast<std::string, LoggerDefine>
{
public:
    LoggerDefine operator()(const std::string& str) const
    {
        //加载yaml文件
        YAML::Node node = YAML::Load(str);
        LoggerDefine ld;
        //获取日志名字
        if (!node["name"].IsDefined())
        {
            LOG_ROOT_ERROR() << "logger name is not defined" << node ;
            throw std::logic_error("logger name is not defined");
        }
        ld.name = node["name"].as<std::string>();
        //获取日志级别
        ld.level = LogLevel::FromString(node["level"].IsDefined() ? node["level"].as<std::string>() : "");
        ld.formatter = node["formatter"].IsDefined() ? node["formatter"].as<std::string>() : "";
        //获取appenders
        if (node["appenders"].IsDefined())
        {
            //读取appender数组
            for (auto appender : node["appenders"])
            {
                LogAppenderDefine lad;
                //根据tpye来判断appender类型
                if (!appender["type"].IsDefined())
                {
                    LOG_ROOT_ERROR() << "appender type is not defined" << appender;
                    throw std::logic_error("appender type is not defined");
                }
                auto type = appender["type"].as<std::string>();
                if (type == "FileLogAppender")
                {
                    lad.type = 2;
                    //需要file名字
                    if (!appender["file"].IsDefined())
                    {
                        LOG_ROOT_ERROR() << "file appender file is node defined" << appender["file"].as<std::string>();
                        continue;
                    }
                    lad.file = appender["file"].as<std::string>();
                    lad.level = LogLevel::FromString(appender["level"].IsDefined() ? appender["level"].as<std::string>()
                        : "");
                    lad.formatter = appender["formatter"].IsDefined() ? appender["formatter"].as<std::string>() : "";

                }
                else if (type == "StdoutLogAppender")
                {
                    lad.type = 1;
                    lad.level = LogLevel::FromString(appender["level"].IsDefined() ? appender["level"].as<std::string>()
                        : "");
                    lad.formatter = appender["formatter"].IsDefined() ? appender["formatter"].as<std::string>() : "";
                }
                else
                {
                    LOG_ROOT_ERROR() << "unknown appender type" << appender;
                    continue;
                }
                ld.appenders.push_back(lad);
            }
        }
        return ld;
    }
};

template<>
class LexicalCast<LoggerDefine,std::string>
{
public:
    std::string operator()(const LoggerDefine& ld) const
    {
        std::stringstream ss;
        YAML::Node node;
        if (!ld.name.empty())
        {
            node["name"] = ld.name;
        }
        if (ld.level != LogLevel::UNKNOW)
        {
            node["level"] = LogLevel::ToString(ld.level);
        }
        if (!ld.formatter.empty())
        {
            node["formatter"] = ld.formatter;
        }
        for (auto& lad : ld.appenders)
        {
            YAML::Node appender;
            if (lad.type == 1)
            {
                appender["type"] = "StdoutLogAppender";
                if (!lad.formatter.empty())
                {
                    appender["formatter"] = lad.formatter;
                }
                appender["level"] = LogLevel::ToString(lad.level);
            }
            if (lad.type == 2)
            {
                appender["type"] = "FileLogAppender";
                if (lad.formatter.empty())
                {
                    appender["formatter"] = lad.formatter;
                }
                appender["level"] = LogLevel::ToString(lad.level);
                appender["file"] = lad.file;
            }
            node["appenders"].push_back(appender);
        }
        ss << node;
        return ss.str();
    }
};

// 要求编译阶段初始化，处理配置文件发生变化时需要的操作
struct LogInit
{
    constexpr LogInit()
    {

        g_log_defines->add_listener([](const std::set<LoggerDefine>& old_value,const  std::set<LoggerDefine>& new_value)
        {
            LOG_ROOT_INFO() << "log config has changed";
            // 将old里面没有的 new log 加入到LogMGr中,有的但变化了则改变
            for (auto& ld : new_value)
            {
                Logger::ptr logger;
                auto it = old_value.find(ld);
                //没有则新增
                if (it == old_value.end())
                {
                    logger = LOG_NAME(ld.name);
                }else//有则判断是否改变
                {
                    if (*it == ld)
                    {
                        continue;
                    }
                    else
                    {
                        logger = LOG_NAME(ld.name);
                    }
                }
                //若改变或者新增则对logger进行操作
                logger->set_level(ld.level);
                if (!ld.formatter.empty())
                {
                    logger->set_formatter(ld.formatter);
                }
                logger->clear_appender();
                //添加appender
                for (auto& appender : ld.appenders)
                {
                    LogAppender::ptr logAppender;
                    if (appender.type == 1)
                    {
                        logAppender.reset(new StdOutLogAppender);
                    }else if (appender.type == 2)
                    {
                        logAppender.reset(new FileLogAppender(appender.file));
                    }
                    logAppender->set_level(appender.level);
                    if (appender.level != LogLevel::UNKNOW)
                    {
                        logAppender->set_has_level(true);
                    }else
                    {
                        logAppender->set_has_level(false);
                    }
                    //根据formatter格式是否正确来设置
                    if (!appender.formatter.empty())
                    {
                        LogFormatter::ptr formatter(new LogFormatter(appender.formatter));
                        if (formatter->isError())
                        {
                            LOG_ROOT_ERROR() << "log.name" << ld.name << "appender type" << appender.type
                                <<" formatter= " << appender.formatter << " is invalid ";
                        }else
                        {
                            logAppender->set_formatter(formatter);
                            logAppender->set_has_formatter(true);
                        }
                    }else
                    {
                        logAppender->set_has_formatter(false);
                    }
                    logger->add_appender(logAppender);
                }
            }
            //旧的有而新的没有的话
            for (auto& ld : old_value)
            {
                auto it = new_value.find(ld);
                if (it != new_value.end())
                {
                    auto logger = LOG_NAME(ld.name);
                    logger->clear_appender();
                    LOG_MGR()->remove_logger(ld.name);
                }
            }
        });
    }

};
LogInit _logInit = LogInit();

}
