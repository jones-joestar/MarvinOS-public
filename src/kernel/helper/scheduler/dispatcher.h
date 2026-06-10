#pragma once
#include "../common.h"
#include "../process/process.h"

void switch_context(process_t* process);
void proc0_idle();