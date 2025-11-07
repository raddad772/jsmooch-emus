#include "cpu-test-helpers.h"

#include <stdio.h>

#include "helpers_c/user.h"

char *construct_cpu_test_path(char* w, const char* cpu_test_folder, const char* who)
{
    const char *homeDir = get_user_dir();
#if defined(_MSC_VER)
    char *tp = w;
    tp += sprintf(tp, "%s\\dev\\%s\\v1\\%s", homeDir, cpu_test_folder, who);
    return tp;
#else
    char *tp = w;
    tp += sprintf(tp, "%s/dev/%s/v1/%s", homeDir, cpu_test_folder, who);
    return tp;
#endif
}
