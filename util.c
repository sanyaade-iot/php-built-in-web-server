/* util.c */
#include "util.h"

#include <string.h>
#include <stdlib.h>

/**
 * Steal from redis
 */
char **str_split(char *s, int len, char *sep, int seplen, int *count) {
    int elements = 0, slots = 5, start = 0, j;
    char **tokens;

    if (seplen < 1 || len < 0) return NULL;

    tokens = (char **)malloc(sizeof(char *) * slots);
    if (tokens == NULL) return NULL;

    if (len == 0) {
        *count = 0;
        return tokens;
    }

    for (j = 0; j < (len-(seplen-1)); j++) {
        /* make sure there is room for the next element and the final one */
        if (slots < elements+2) {
            char **newtokens;

            slots *= 2;
            newtokens = realloc(tokens,sizeof(char *) * slots);
            if (newtokens == NULL) goto cleanup;
            tokens = newtokens;
        }
        /* search the separator */
        if ((seplen == 1 && *(s+j) == sep[0]) || (memcmp(s+j,sep,seplen) == 0)) {
            tokens[elements] = malloc(sizeof(char) * (j-start+1));
            if (tokens[elements] == NULL) goto cleanup;

            memcpy(tokens[elements], s+start, j-start);
            tokens[elements][j-start] = '\0';

            elements++;
            start = j+seplen;
            j = j+seplen-1; /* skip the separator */
        }
    }
    /* Add the final element. We are sure there is room in the tokens array. */
    tokens[elements] = malloc(sizeof(char) * (len-start+1));
    if (tokens[elements] == NULL) goto cleanup;
    memcpy(tokens[elements], s+start, len-start);
    tokens[elements][len-start] = '\0';

    elements++;
    *count = elements;
    return tokens;

cleanup:
    {
        int i;
        for (i = 0; i < elements; i++) free(tokens[i]);
        free(tokens);
        *count = 0;
        return NULL;
    }
}

char *str_trim(char *s, const char *cset) {
    char *start, *end, *sp, *ep;
    size_t len;

    sp = start = s;
    ep = end = s+strlen(s)-1;
    while(sp <= end && strchr(cset, *sp)) sp++;
    while(ep > start && strchr(cset, *ep)) ep--;
    len = (sp > ep) ? 0 : ((ep-sp)+1);
    if (sp != start || ep != end) {
        char *ns = malloc(sizeof(char) * (len + 1));
        memcpy(ns, sp, len);
        ns[len] = '\0';
        free(s);
        s = ns;
    }

    return s;
}
