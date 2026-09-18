#ifndef UTILS_H
#define UTILS_H
int utils_string_compare(char* str1, char* str2);
unsigned long utils_HexStr2Int(char *s, int char_size);
unsigned long utils_DecStr2Int(char *s, int char_size);
unsigned long utils_align(unsigned long size, unsigned long s);
unsigned int utils_change_endian_32(unsigned int val);
unsigned long long utils_change_endian_64(unsigned long long val);
void utils_int_to_str(int value, char* str);
void utils_memcpy(char* dst, char* src, unsigned int len);
void* utils_memset(void *s, int c, unsigned long n);
unsigned long utils_strlen(char *s);
#define NULL (void*)0
#endif