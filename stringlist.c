#include "stringlist.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum eType {
	ELEM
	,HEAD
	,TAIL
};

struct _strings {
	size_t count;
	struct _str_elem *head;
	struct _str_elem *current;
};

struct _str_elem {
	int id;
	char *key;
	char *value;
	enum eType type;
	struct _str_elem *next;
	struct _str_elem *prev;
};

static struct _str_elem *elem_new(int id, char *key, char *value, enum eType type)
{
	struct _str_elem *e = malloc(sizeof(struct _str_elem));

	e->id = id;
	e->key = key;
	e->value = value;
	e->type = type;
	e->next = NULL;
	e->prev = NULL;

	return e;
}

static void elem_free(struct _str_elem *e)
{
	if (e->prev != NULL && e->type != TAIL)
		e->prev->next = e->next;

	if (e->next != NULL && e->type != HEAD)
		e->next->prev = e->prev;

	e->next = NULL;
	e->prev = NULL;
	free(e->key);
	free(e->value);
	free(e);
}


/* api functions */

StringList *sl_init(void)
{
	struct _strings *str = malloc(sizeof(struct _strings));

	str->count = 0;
	str->head = elem_new(0, NULL, NULL, HEAD);
	str->current = str->head;
	str->head->next = elem_new(-1, NULL, NULL, TAIL);
	str->head->next->prev = str->head;
	str->head->next->next = NULL;

	return str;
}

void sl_release(StringList *sl)
{
	while (sl_pop(sl))
		;

	free(sl);
}

/* key can be NULL */
int sl_push(StringList *sl, char *key, char *elem)
{
	struct _str_elem *e = sl->head;
	struct _str_elem *new = elem_new(0, key, elem, ELEM);

	while (e->next->type != TAIL)
		e = e->next;

	new->id = e->id+1;
	new->prev = e;
	new->next = e->next; /* = TAIL */
	e->next = new;

	sl->count++;
	return new->id; // or sl->count???
}

int sl_pop(StringList *sl)
{
	struct _str_elem *elem = sl->head;
	while (elem->next->type != TAIL)
		elem = elem->next;

	elem_free(elem);
	sl->count--;
	return (sl->count);
}

char *sl_get_elem_key(StringList *sl, char *key)
{
	struct _str_elem *e = sl->head;
	while (e->type != TAIL) {
		if (e->type == HEAD)
			continue;

		if (strncmp(key, e->key, strlen(key)) == 0)
			return e->value;

		e = e->next;
	}

	return NULL;
}

/* iterate through elements */
char *sl_get_next(StringList *sl)
{
	if (sl->current->type == TAIL)
		return NULL;

	sl->current = sl->current->next;

	return (sl->current->value);
}

void sl_rewind(StringList *sl)
{
	sl->current = sl->head;
}
