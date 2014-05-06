/* util.h */
#ifndef UTIL_H
#define UTIL_H

char **str_split(char *str, int len, char *sep, int seplen, int *count);
char *str_trim(char *s, const char *cset);

#endif /* UTIL_H */
