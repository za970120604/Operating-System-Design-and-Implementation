#include "header/utils.h"

int string_compare(char* str1, char* str2) {
    while (*str1 && *str2) {  
        if (*str1 != *str2) {
            return 0; 
        }
        str1++;
        str2++;
    }
    
    // If str1 ends with a newline character ('\n') 
    // and str2 ends with a null character ('\0'), or if both strings reach the null character ('\0'), they are considered equal.
    return (*str1 == '\n' && *str2 == '\0') || (*str1 == *str2); 

}
