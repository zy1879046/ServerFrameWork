//
// Created by 张羽 on 25-4-10.
//

#include "log.h"
#include <iostream>
#include <filesystem>

int main(){
  // sylar::Logger::ptr logger(new sylar::Logger("test"));
  // logger->set_level(sylar::LogLevel::DEBUG);
  // sylar::StdOutLogAppender::ptr appender(new sylar::StdOutLogAppender());
  // logger->add_appender(appender);
  // sylar::LogEvent::ptr event = std::make_shared<sylar::LogEvent>(logger, sylar::LogLevel::DEBUG, __FILE__, __LINE__, 0, 0, time(0));
  // event->get_ss() << "Hello World";
  // sylar::LogEventWrap wrap(event);
  std::cout << "当前工作目录: " << std::filesystem::current_path() << std::endl;
  auto logger = LOG_ROOT();
  LOG_DEBUG(logger) << "Hello World";
  LOG_INFO(logger) << "Hello World";
  auto appender = std::make_shared<sylar::FileLogAppender>("../log/test.log", sylar::LogLevel::WARN);
  auto logger2 = LOG_NAME("test");
  logger2->add_appender(appender);
  LOG_INFO(logger2) << "Hello World INFO";
  LOG_DEBUG(logger2) << "Hello World DEBUG";
  LOG_WARN(logger2) << "Hello World WARN";
  LOG_ERROR(logger2) << "Hello World ERROR";
  LOG_FATAL(logger2) << "Hello World FATAL";
}