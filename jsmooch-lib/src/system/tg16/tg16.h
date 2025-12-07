#pragma once
#include "helpers/enums.h"
#include "helpers/debug.h"
#include "helpers/int.h"
#include "helpers/sys_interface.h"

jsm_system *TG16_new(jsm::systems kind);
void TG16_delete(jsm_system* system);
