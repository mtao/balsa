#include <QtCore/QtGlobal>
#include <QByteArray>
#include <QString>
#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include "balsa/qt/spdlog_logger.hpp"

#include <string>
namespace balsa::qt {


namespace {
    static std::string logger_name = "";

    constexpr auto qtMsgType_to_spdlog_log_level(QtMsgType type) -> spdlog::level::level_enum {
        switch (type) {
        default:
        case QtDebugMsg:
            return spdlog::level::debug;
        case QtInfoMsg:
            return spdlog::level::info;
        case QtWarningMsg:
            return spdlog::level::warn;
        // case QtSystemMsg: # for some reason QtSystemMsg == QtCriticalMsg
        case QtCriticalMsg:
            return spdlog::level::critical;
        case QtFatalMsg:
            return spdlog::level::err;
        }
    }
}// namespace
void setSpdlogLoggerName(const std::string_view &logger_name) {
    qt::logger_name = logger_name;
}

void spdlogMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    QByteArray localMsg = msg.toLocal8Bit();
    spdlog::level::level_enum level = qtMsgType_to_spdlog_log_level(type);

    const static std::string none = "";
    const static std::string fatal = "[FATAL]";
    const static std::string system = "[SYSTEM]";

    std::string const *prefix = &none;


    if (type == QtFatalMsg) {
        prefix = &fatal;
    } else if (type == QtSystemMsg) {
        prefix = &system;
    }
    auto logger = spdlog::get(logger_name);
    if (context.file == nullptr) {
        logger->log(level,
                    "{}{}",
                    *prefix,
                    localMsg.constData());
    } else {
        logger->log(level,
                    "{}{} ({}:{}, {})",
                    *prefix,
                    localMsg.constData(),
                    context.file,
                    context.line,
                    context.function);
    }
}
void activateSpdlogOutput(const std::string_view &logger_name) {
    // TODO: figure out how to output categories
    qInstallMessageHandler(spdlogMessageOutput);
}
}// namespace balsa::qt
