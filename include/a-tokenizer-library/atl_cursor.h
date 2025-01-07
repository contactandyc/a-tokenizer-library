#ifndef _atl_cursor_h
#define _atl_cursor_h

#include <inttypes.h>
#include "a-memory-library/aml_pool.h"
#include "a-tokenizer-library/atl_token.h"

struct atl_cursor_s;
typedef struct atl_cursor_s atl_cursor_t;
/*
    Callback function for custom cursors.  This function is called for each token.
*/
typedef atl_cursor_t *(*atl_cursor_custom_cb)(aml_pool_t *pool, atl_token_t *token, void *arg);

/* The atl_token object has a atl_token_parse_expression function which parses a query
   into the atl_token_t object.  Once this cursor is constructed, call cursor->advance()
   to get each id which matches the given query. */
atl_cursor_t *atl_cursor_open(aml_pool_t *pool, atl_cursor_custom_cb cb, atl_token_t *t, void *arg);

/*
    Create a cursor which will return all ids in the range [start, end)
*/
atl_cursor_t *atl_cursor_range(aml_pool_t *pool, uint32_t start, uint32_t end);

typedef bool (*atl_cursor_advance_cb)( atl_cursor_t * c );
typedef bool (*atl_cursor_advance_to_cb)( atl_cursor_t * c, uint32_t id );
typedef void (*atl_cursor_add_cb)( atl_cursor_t *dest, atl_cursor_t *src );

enum atl_cursor_type { EMPTY_CURSOR = 0, AND_CURSOR = 1, PHRASE_CURSOR = 2, OR_CURSOR = 3, NOT_CURSOR = 4,
                       NORMAL_CURSOR = 5, TERM_CURSOR = 6 };

struct atl_cursor_s {
    aml_pool_t *pool;
    atl_cursor_advance_cb advance;
    atl_cursor_advance_to_cb advance_to;
    atl_cursor_advance_cb _advance;

    atl_cursor_add_cb add;

    enum atl_cursor_type type;

    uint32_t id;
};

/* convert a query to an empty one */
bool atl_cursor_empty(atl_cursor_t *c);

atl_cursor_t *atl_cursor_init_empty(aml_pool_t *pool);
atl_cursor_t *atl_cursor_init_id(aml_pool_t *pool, uint32_t id);
atl_cursor_t *atl_cursor_init_or(aml_pool_t *pool);
atl_cursor_t *atl_cursor_init_and(aml_pool_t *pool);
atl_cursor_t *atl_cursor_init_not(aml_pool_t *pool, atl_cursor_t *pos, atl_cursor_t *neg);

atl_cursor_t ** atl_cursor_subs(atl_cursor_t *c, uint32_t *num_sub );

void atl_cursor_reset(atl_cursor_t *c);

#endif