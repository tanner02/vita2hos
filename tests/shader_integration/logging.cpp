#include <cstdio>
#include <string>
#include <vector>
#include "../../include/vita3k/util/log.h"

static std::vector<std::string> messages;
static unsigned evaluations;

void log_print(const char *message) { messages.emplace_back(message); }
static unsigned diagnostic_argument() { return ++evaluations; }

int main()
{
    LOG_TRACE("trace {}", diagnostic_argument());
    LOG_DEBUG("debug {}", diagnostic_argument());
#ifdef NDEBUG
    if (evaluations || !messages.empty()) return 1;
#else
    if (evaluations!=2 || messages!=std::vector<std::string>{"trace 1","debug 2"}) return 2;
#endif
    evaluations=0;messages.clear();
    LOG_INFO("info {}", diagnostic_argument());
    LOG_WARN("warn {}", diagnostic_argument());
    LOG_ERROR("error {}", diagnostic_argument());
    LOG_CRITICAL("critical {}", diagnostic_argument());
    if (evaluations!=4 || messages!=std::vector<std::string>{
            "info 1","warn 2","error 3","critical 4"}) return 3;
    std::puts("Shader log levels and argument evaluation checks passed.");
}
