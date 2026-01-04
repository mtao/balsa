#include <string_view>

namespace balsa::qt {

void activateSpdlogOutput(const std::string_view &logger_name = "");
void setSpdlogLoggerName(const std::string_view &logger_name);

}// namespace balsa::qt
