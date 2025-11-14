#include "user.h"

#if defined(_MSC_VER)
#include <windows.h>
#else
#include <unistd.h>
#include <pwd.h>
#include <cstdlib>
#endif

const char* get_user_dir()
{
#if defined(_MSC_VER)
    const char* homeDir = getenv("USERPROFILE");
#else
    const char *homeDir = getenv("HOME");

    if (!homeDir) {
        struct passwd* pwd = getpwuid(getuid());
        if (pwd)
            homeDir = pwd->pw_dir;
    }
#endif
    return homeDir ? homeDir : "";
}
