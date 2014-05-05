/* util.h */
#ifndef UTIL_H
#define UTIL_H

char **strsplit(char *str, int len, char *sep, int seplen, int *count);
char *sdstrim(char *s, const char *cset);
void strfree(char **str, int count);

#endif /* UTIL_H */
