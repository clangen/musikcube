/*
 * This code implements the BASE64 algorithm.
 * This code is in the public domain; do with it what you wish.
 *
 * @file base64.c
 * @brief This code implements the BASE64 algorithm
 * @author Matthieu Speder
 */
#ifndef BASE64_H
#define BASE64_H

#include "platform.h"

char *
BASE64Decode(const char* src);

#endif /* !BASE64_H */
