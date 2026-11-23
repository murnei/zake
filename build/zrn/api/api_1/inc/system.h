#ifndef SYSTEM_H
#define SYSTEM_H

#include "stdint.h"

void make_directory(const char* name);
void remove_directory(const char* name);
void create_file(const char* name);
void write_file(const char* name, const char* data, uint32_t size);
int32_t read_file(const char* name, char* buffer, uint32_t size);

#endif
