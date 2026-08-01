#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "main.h"
#include <stdbool.h>

/* Scheduler Event Interface */
bool scheduler_check_start(void);
bool scheduler_check_end(void);

#endif /* SCHEDULER_H */
