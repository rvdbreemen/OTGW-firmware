// TASK-1736 host fake: no HAL locks, so cbuf.cpp needs no FreeRTOS mutex.
#pragma once
#define CONFIG_DISABLE_HAL_LOCKS 1
