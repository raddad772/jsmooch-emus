//
// Created by . on 8/4/24.
//

#include "file_exists.h"
#ifdef WIN32
#include <io.h>
#define F_OK 0
#define access _access

#else
#include <unistd.h>
#endif

u32 file_exists(const char *fname)
{
    return access(fname, F_OK) == 0;
}


