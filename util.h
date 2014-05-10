/* util.h */
#ifndef UTIL_H
#define UTIL_H

struct str_array {
    char **array;
    int total;
    int len;
};

char **str_split(char *str, int len, char *sep, int seplen, int *count);
char *str_trim(char *s, const char *cset);
int in_int_array(int *array, int needle, int length);

#endif /* UTIL_H */
