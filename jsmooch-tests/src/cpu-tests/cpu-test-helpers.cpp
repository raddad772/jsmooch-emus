#include "cpu-test-helpers.h"

#include <cstdio>

#include "helpers/user.h"

char *construct_cpu_test_path(char* w, const char* cpu_test_folder, const char* who, size_t sz)
{
    const char *homeDir = get_user_dir();
#if defined(_MSC_VER)
    char *tp = w;
    tp += snprintf(tp, sz, "%s\\dev\\%s\\v1\\%s", homeDir, cpu_test_folder, who);
    return tp;
#else
    char *tp = w;
    tp += snprintf(tp, sz,"%s/dev/%s/v1/%s", homeDir, cpu_test_folder, who);
    return tp;
#endif
}
