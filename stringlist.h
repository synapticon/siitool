#ifndef STRINGLIST_H
#define STRINGLIST_H

typedef struct _strings StringList;

StringList *sl_init(void);
void sl_release(StringList *sl);

/* key can be NULL */
int sl_push(StringList *sl, char *key, char *elem);
int sl_pop(StringList *sl);

char *sl_get_elem_key(StringList *sl, char *key);

/* iterate through elements */
char *sl_get_next(StringList *sl);
void sl_rewind(StringList *sl);

#endif /* STRINGLIST_H */
