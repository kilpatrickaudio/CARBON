/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PROBABILITY_SEED_H
#define PROBABILITY_SEED_H
#include <stdint.h>
/* Startup only: bounded hardware entropy read; timing fallback on failure. */
uint32_t probability_startup_seed(void);
#endif
