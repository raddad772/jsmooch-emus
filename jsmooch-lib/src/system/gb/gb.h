#pragma once

#include "helpers/enums.h"
#include "helpers/debug.h"
#include "helpers/int.h"
#include "helpers/sys_interface.h"
#include "gb_enums.h"

jsm_system *GB_new(jsm::systems kind, GB_variants variant);
void GB_delete(jsm_system* system);
