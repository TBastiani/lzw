#include <stdlib.h>
#include <assert.h>

#include "lzw.h"

void lzw_state_init(struct lzw_state *state)
{
	for(unsigned i = 0; i < SYMCOUNT; i++)
		state->dict[i] = (struct dict_entry){ i, NULL, NULL, NULL, i };
	state->next_code = SYMCOUNT;
	state->current = NULL;
}

static struct dict_entry *step(struct dict_entry *parent, unsigned char ch)
{
	struct dict_entry *child;
	for(child = parent->child; child; child = child->sibling)
		if(child->ch == ch)
			break;
	return child;
}

static void reset_dict(struct lzw_state *state)
{
	state->next_code = SYMCOUNT;
	for(unsigned i = 0; i < SYMCOUNT; i++)
		state->dict[i].child = NULL;
}

static struct dict_entry *alloc_dict_entry(
	struct lzw_state *state,
	struct dict_entry *parent,
	unsigned char ch)
{
	if(state->next_code == DICTSIZE)
		return NULL;
	struct dict_entry *entry = state->dict + state->next_code;
	entry->code = state->next_code++;
	assert(state->next_code <= DICTSIZE);
	entry->ch = ch;

	entry->sibling = parent->child;
	parent->child = entry;
	entry->parent = parent;
	entry->child = NULL;
	return entry;
}

typedef int (*emit_code_fn_t)(void *p, unsigned code);
int lzw_encode(
	struct lzw_state *state,
	emit_code_fn_t emit,
	void *p,
	unsigned char ch)
{
	struct dict_entry *child;
	if(state->current == NULL) {
		state->current = state->dict + ch;
	} else if((child = step(state->current, ch))) {
		// have transition via ch
		state->current = child;
	} else {
		// don't have transition via ch
		if(emit(p, state->current->code))
			return -1;
		child = alloc_dict_entry(state, state->current, ch);
		if(!child)
			reset_dict(state);
		
		state->current = state->dict + ch;
	}

	return 0;
}

int lzw_encode_finish(
	struct lzw_state *state,
	emit_code_fn_t emit,
	void *p)
{
	if(emit(p, state->current->code))
		return -1;
	return 0;
}

static unsigned char first(struct dict_entry *entry)
{
	if(entry->parent == NULL)
		return entry->ch;
	else return first(entry->parent);
}

typedef int (*emit_char_fn_t)(void *p, unsigned char ch);
int lzw_decode(
	struct lzw_state *state,
	emit_char_fn_t emit,
	void *p,
	unsigned code)
{
	int output_entry(struct dict_entry *ent)
	{
		if(ent->parent)
			if(output_entry(ent->parent))
				return -1;
		if(emit(p, ent->ch))
			return -1;
		return 0;
	}

	struct dict_entry *entry, *child;
	if(state->current == NULL) {
		if(emit(p, state->dict[code].ch))
			return -1;
		state->current = state->dict + state->dict[code].ch;
	} else if(code >= state->next_code) {
		if(output_entry(state->current))
			return -1;
		if(emit(p, first(state->current)))
			return -1;

		child = alloc_dict_entry(state, state->current, first(state->current));
		assert(child); // encoder didn't reset, so we better not

		state->current = step(state->current, first(state->current));
	} else {
		entry = state->dict + code;
		if(output_entry(state->dict + code))
			return -1;

		child = alloc_dict_entry(state, state->current, first(state->dict + code));
		if(!child)
			reset_dict(state);

		state->current = entry;
	}

	return 0;
}
