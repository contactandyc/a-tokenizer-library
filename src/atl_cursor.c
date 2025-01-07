#include "a-tokenizer-library/atl_cursor.h"

#include "a-memory-library/aml_alloc.h"
#include "a-memory-library/aml_pool.h"

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static
bool advance_empty_to(atl_cursor_t *c, uint32_t id)
{
    (void)c;
    (void)id;
    return false;
}

static
bool advance_empty(atl_cursor_t *c)
{
    (void)c;
    return false;
}

atl_cursor_t *atl_cursor_init_empty(aml_pool_t *pool) {
    atl_cursor_t *r = (atl_cursor_t *)aml_pool_zalloc(pool, sizeof(atl_cursor_t));
    r->advance = advance_empty;
    r->advance_to = advance_empty_to;
    r->type = EMPTY_CURSOR;
    return r;
}


struct or_cursor_s;
typedef struct or_cursor_s or_cursor_t;

struct or_cursor_s {
    atl_cursor_t cursor;
    atl_cursor_t **cursors;
    uint32_t num_cursors;
    uint32_t cursor_size;

    atl_cursor_t **active;
    atl_cursor_t **heap;
    uint32_t num_active;
    uint32_t num_heap;
};

static
void or_add( or_cursor_t *dest, atl_cursor_t *src ) {
    // printf( "%s, %u\n", __FUNCTION__, __LINE__ );

    if(dest->num_cursors >= dest->cursor_size) {
        uint32_t num_cursors = (dest->cursor_size+1)*2;
        atl_cursor_t **cursors = 
            (atl_cursor_t **)aml_pool_alloc(dest->cursor.pool, sizeof(atl_cursor_t *) * num_cursors);
        if(dest->num_cursors)
            memcpy(cursors, dest->cursors, dest->num_cursors * sizeof(atl_cursor_t *));
        dest->cursor_size = num_cursors;
        dest->cursors = cursors;
    }
    dest->cursors[dest->num_cursors] = src;
    dest->num_cursors++;
}

static
void or_push(or_cursor_t *c, atl_cursor_t *src) {
    // printf( "%s, %u\n", __FUNCTION__, __LINE__ );
    // atl_cursor_reset(src);
    atl_cursor_t *tmp, **heap = c->heap;

    c->num_heap++;
    uint32_t i=c->num_heap;
    heap[i] = src;
    uint32_t j=i>>1;

    while (j > 0 && heap[i]->id < heap[j]->id) {
        tmp = heap[i];
        heap[i] = heap[j];
        heap[j] = tmp;
        i = j;
        j = j >> 1;
    }
}

static
atl_cursor_t *or_pop(or_cursor_t *c) {
    // printf( "%s, %u\n", __FUNCTION__, __LINE__ );

    c->num_heap--;
    ssize_t num = c->num_heap;
    atl_cursor_t **heap = c->heap;
    atl_cursor_t *r = heap[1];
    heap[1] = heap[num + 1];

    ssize_t i = 1;
    ssize_t j = i << 1;
    ssize_t k = j + 1;

    if (k <= num && heap[k]->id < heap[j]->id)
        j = k;

    while (j <= num && heap[j]->id < heap[i]->id) {
        atl_cursor_t *tmp = heap[i];
        heap[i] = heap[j];
        heap[j] = tmp;

        i = j;
        j = i << 1;
        k = j + 1;
        if (k <= num && heap[k]->id < heap[j]->id)
            j = k;
    }
    return r;
}

static
bool advance_or(or_cursor_t *c) {
    // printf( "%s, %u\n", __FUNCTION__, __LINE__ );

    for( uint32_t i=0; i<c->num_active; i++ ) {
        if(c->active[i]->advance(c->active[i]))
            or_push(c, c->active[i]);
    }
    if(!c->num_heap)
        return atl_cursor_empty(&c->cursor);

    c->active[0] = or_pop(c);
    c->num_active = 1;

    atl_cursor_t **heap = c->heap;
    uint32_t id = c->active[0]->id;
    while(c->num_heap && heap[1]->id == id) {
        c->active[c->num_active] = or_pop(c);
        c->num_active++;
    }

    c->cursor.id = id;
    return true;
}

static
bool advance_or_to(or_cursor_t *c, uint32_t id) {
    // printf( "%s, %u\n", __FUNCTION__, __LINE__ );

    if (id <= c->cursor.id)
        return true;

    bool pushed = false;
    for( uint32_t i=0; i<c->num_active; i++ ) {
        if(c->active[i]->advance_to(c->active[i], id)) {
            or_push(c, c->active[i]);
            pushed = true;
        }
    }
    atl_cursor_t *p;
    if(!pushed) {
        if(!c->num_heap)
            return atl_cursor_empty(&c->cursor);

        atl_cursor_t *p = or_pop(c);
        while(!p->advance_to(p, id)) {
            if(!c->num_heap)
                return atl_cursor_empty(&c->cursor);
            p = or_pop(c);
        }
    }
    else
        p = or_pop(c);

    c->active[0] = p;
    c->num_active = 1;

    atl_cursor_t **heap = c->heap;
    uint32_t current = p->id;
    while(c->num_heap && heap[1]->id == current) {
        c->active[c->num_active] = or_pop(c);
        c->num_active++;
    }

    c->cursor.id = current;
    return true;
}

static
bool advance_or_init(or_cursor_t *c) {
    // printf( "%s, %u\n", __FUNCTION__, __LINE__ );

    uint32_t num = c->num_cursors;

    c->heap = (atl_cursor_t **)aml_pool_alloc(c->cursor.pool, sizeof(atl_cursor_t *) * (num+1) * 2);
    c->active = c->heap + (num+1);
    c->num_heap = 0;
    c->num_active = 0;

    for( uint32_t k=0; k<c->num_cursors; k++ ) {
        if(c->cursors[k]->advance(c->cursors[k])) {
            // atl_cursor_reset(c->cursors[k]);
            or_push(c, c->cursors[k]);
        }
    }
    
    if(!c->num_heap)
        return atl_cursor_empty(&c->cursor);

    c->cursor.advance = (atl_cursor_advance_cb)advance_or;
    c->cursor.advance_to = (atl_cursor_advance_to_cb)advance_or_to;

    return advance_or(c);
}

static
bool advance_or_to_init(or_cursor_t *c, uint32_t id) {
    // printf( "%s, %u\n", __FUNCTION__, __LINE__ );
    if(!advance_or_init(c))
        return atl_cursor_empty(&c->cursor);

    return c->cursor.advance_to((atl_cursor_t*)c, id);
}

static
void init_or(aml_pool_t *pool, or_cursor_t *r) {
    // printf( "%s, %u\n", __FUNCTION__, __LINE__ );

    uint32_t num_cursors = 2;
    r->cursor.pool = pool;
    r->cursor.type = OR_CURSOR;
    r->cursor.advance = (atl_cursor_advance_cb)advance_or_init;
    r->cursor.advance_to = (atl_cursor_advance_to_cb)advance_or_to_init;
    r->cursor.add = (atl_cursor_add_cb)or_add;
    r->cursors = (atl_cursor_t **)aml_pool_alloc(pool, sizeof(atl_cursor_t *) * num_cursors);
    r->num_cursors = 0;
    r->cursor_size = num_cursors;
}

atl_cursor_t *atl_cursor_init_or(aml_pool_t *pool) {
    // printf( "%s, %u\n", __FUNCTION__, __LINE__ );

    or_cursor_t *r = (or_cursor_t *)aml_pool_zalloc(pool, sizeof(or_cursor_t));
    init_or(pool, r);
    return (atl_cursor_t *)r;
}

struct not_cursor_s;
typedef struct not_cursor_s not_cursor_t;

struct not_cursor_s {
    atl_cursor_t cursor;
    atl_cursor_t *pos;
    atl_cursor_t *neg;
};

static
bool advance_pos(not_cursor_t *c) {
    if(c->pos->advance(c->pos)) {
        c->cursor.id=c->pos->id;
        return true;
    }
    return atl_cursor_empty(&c->cursor);
}

static
bool advance_pos_to(not_cursor_t *c, uint32_t _id) {
    if (_id <= c->cursor.id)
        return true;

    if(c->pos->advance_to(c->pos, _id)) {
        c->cursor.id=c->pos->id;
        return true;
    }
    return atl_cursor_empty(&c->cursor);
}

static
bool advance_not(not_cursor_t *c)
{
    while(true) {
        if(!c->pos->advance(c->pos))
            return atl_cursor_empty(&c->cursor);
        if(!c->neg->advance_to(c->neg, c->pos->id)) {
            c->cursor.advance_to = (atl_cursor_advance_to_cb)advance_pos_to;
            c->cursor.advance = (atl_cursor_advance_cb)advance_pos;
            return true;
        }
        if(c->pos->id == c->neg->id)
            continue;
        c->cursor.id = c->pos->id;
        return true;
    }
}

static
bool advance_not_to(not_cursor_t *c, uint32_t _id)
{
    if (_id <= c->cursor.id)
        return true;

    if(!c->pos->advance_to(c->pos, _id))
        return atl_cursor_empty(&c->cursor);
    if(!c->neg->advance_to(c->neg, c->pos->id)) {
        c->cursor.advance_to = (atl_cursor_advance_to_cb)advance_pos_to;
        c->cursor.advance = (atl_cursor_advance_cb)advance_pos;
        return true;
    }
    if(c->pos->id == c->neg->id)
        return advance_not(c);
    c->cursor.id = c->pos->id;
    return true;
}

atl_cursor_t *atl_cursor_init_not(aml_pool_t *pool,
                                atl_cursor_t *pos,
                                atl_cursor_t *neg ) {
    if(!pos)
        return atl_cursor_init_empty(pool);
    else if(!neg)
        return pos;
    not_cursor_t *r = (not_cursor_t *)aml_pool_zalloc(pool, sizeof(not_cursor_t));
    r->cursor.pool = pool;
    r->cursor.type = NOT_CURSOR;
    r->cursor.advance = (atl_cursor_advance_cb)advance_not;
    r->cursor.advance_to = (atl_cursor_advance_to_cb)advance_not_to;
    r->pos = pos;
    r->neg = neg;
    return (atl_cursor_t *)r;
}

struct and_cursor_s;
typedef struct and_cursor_s and_cursor_t;

struct and_cursor_s {
    atl_cursor_t cursor;
    atl_cursor_t **cursors;
    uint32_t num_cursors;
    uint32_t cursor_size;
};

static
void and_add( and_cursor_t *dest, atl_cursor_t *src ) {
    if(dest->num_cursors >= dest->cursor_size) {
        uint32_t num_cursors = (dest->cursor_size+1)*2;
        atl_cursor_t **cursors = 
            (atl_cursor_t **)aml_pool_alloc(dest->cursor.pool, sizeof(atl_cursor_t *) * num_cursors);
        if(dest->num_cursors)
            memcpy(cursors, dest->cursors, dest->num_cursors * sizeof(atl_cursor_t *));
        dest->cursor_size = num_cursors;
        dest->cursors = cursors;
    }
    dest->cursors[dest->num_cursors] = src;
    dest->num_cursors++;
}

static
bool advance_and(and_cursor_t *c)
{
    uint32_t i=1;
    uint32_t id = c->cursors[0]->advance(c->cursors[0]);
    while(id && i < c->num_cursors) {
        uint32_t id2 = c->cursors[i]->advance_to(c->cursors[i], id);
        if(id==id2)
            i++;
        else {
            i=0;
            id=id2;
        }
    }
    if(!id)
        return atl_cursor_empty(&c->cursor);
    c->cursor.id = id;
    return true;
}

static
bool advance_and_to(and_cursor_t *c, uint32_t id)
{
    if (id <= c->cursor.id)
        return true;

    uint32_t i=0;
    while(id && i < c->num_cursors) {
        uint32_t id2 = c->cursors[i]->advance_to(c->cursors[i], id);
        if(id==id2)
            i++;
        else {
            i=0;
            id=id2;
        }
    }
    if(!id)
        return atl_cursor_empty(&c->cursor);

    c->cursor.id = id;
    return true;
}

atl_cursor_t *atl_cursor_init_and(aml_pool_t *pool) {
    uint32_t num_cursors = 2;
    and_cursor_t *r = (and_cursor_t *)aml_pool_zalloc(pool, sizeof(and_cursor_t));
    r->cursor.pool = pool;
    r->cursor.type = AND_CURSOR;
    r->cursor.advance = (atl_cursor_advance_cb)advance_and;
    r->cursor.advance_to = (atl_cursor_advance_to_cb)advance_and_to;
    r->cursor.add = (atl_cursor_add_cb)and_add;
    r->cursors = (atl_cursor_t **)aml_pool_alloc(pool, sizeof(atl_cursor_t *) * num_cursors);
    r->num_cursors = 0;
    r->cursor_size = num_cursors;
    return (atl_cursor_t *)r;
}

bool atl_cursor_empty(atl_cursor_t *c) {
    c->advance_to = advance_empty_to;
    c->advance = advance_empty;
    c->id = 0;
    return false;
}

static
bool post_reset_advance(atl_cursor_t *c) {
    c->advance = c->_advance;
    return true;
}

void atl_cursor_reset(atl_cursor_t *c) {
    c->_advance = c->advance;
    c->advance = (atl_cursor_advance_cb)post_reset_advance;
}

struct range_cursor_s;
typedef struct range_cursor_s range_cursor_t;

struct range_cursor_s {
    atl_cursor_t cursor;
    uint32_t id;
    uint32_t end;
};

static
bool advance_range(range_cursor_t *c)
{
    uint32_t id = c->id;
    if(id >= c->end)
        return atl_cursor_empty(&c->cursor);
    c->cursor.id = id;
    c->id++;
    return true;
}

static
bool advance_range_to(range_cursor_t *c, uint32_t id)
{
    if(id <= c->cursor.id)
        return true;

    if(id >= c->end)
        return atl_cursor_empty(&c->cursor);
    c->cursor.id = id;
    c->id = id+1;
    return true;
}

atl_cursor_t *atl_cursor_range(aml_pool_t *pool, uint32_t start, uint32_t end) {
    range_cursor_t *r = (range_cursor_t *)aml_pool_zalloc(pool, sizeof(range_cursor_t));
    r->id = start+1;
    r->end = end;
    r->cursor.id = start;
    r->cursor._advance = (atl_cursor_advance_cb)advance_range;
    r->cursor.advance_to = (atl_cursor_advance_to_cb)advance_range_to;
    r->cursor.advance = (atl_cursor_advance_cb)post_reset_advance;
    r->cursor.type = NORMAL_CURSOR;
    return &(r->cursor);
}

atl_cursor_t *atl_cursor_init_id(aml_pool_t *pool, uint32_t id) {
    atl_cursor_t *r = (atl_cursor_t *)aml_pool_zalloc(pool, sizeof(atl_cursor_t));
    r->id = id;
    r->_advance = advance_empty;       // TODO: This is probably wrong
    r->advance_to = advance_empty_to;  // TODO: This is probably wrong
    r->advance = (atl_cursor_advance_cb)post_reset_advance;
    r->type = NORMAL_CURSOR;
    return r;
}

atl_cursor_t ** atl_cursor_subs(atl_cursor_t *c, uint32_t *num_sub ) {
    if(c->type == AND_CURSOR || c->type == PHRASE_CURSOR) {
        and_cursor_t *r = (and_cursor_t *)c;
        *num_sub = r->num_cursors;
        return r->cursors;
    }
    else if(c->type == OR_CURSOR) {
        or_cursor_t *r = (or_cursor_t *)c;
        *num_sub = r->num_active;
        return r->active;
    }
    else if(c->type == NOT_CURSOR) {
        not_cursor_t *r = (not_cursor_t *)c;
        *num_sub = 1;
        return &(r->pos);
    }
    *num_sub = 0;
    return NULL;
}

static
atl_cursor_t *_atl_cursor_open(aml_pool_t *pool, atl_cursor_custom_cb cb, atl_token_t *t, void *arg) {
    if(t->child) {
        if(t->type == ATL_TOKEN_OPEN_PAREN || t->type == ATL_TOKEN_DQUOTE) {
            atl_cursor_t *resp = atl_cursor_init_and(pool);
            if(t->type == ATL_TOKEN_DQUOTE)
                resp->type = PHRASE_CURSOR;
            atl_token_t *n = t->child;
            while(n) {
                atl_cursor_t *c = _atl_cursor_open(pool, cb, n, arg);
                if(c && c->type != EMPTY_CURSOR)
                    resp->add(resp, c);
                else
                    return NULL;
                n = n->next;
            }
            return resp;
        }
        else if(t->type == ATL_TOKEN_OR) {
            atl_cursor_t *resp = atl_cursor_init_or(pool);
            atl_token_t *n = t->child;
            while(n) {
                atl_cursor_t *c = _atl_cursor_open(pool, cb, n, arg);
                if(c)
                    resp->add(resp, c);
                n = n->next;
            }
            return resp;
        }
        else if(t->type == ATL_TOKEN_NOT) {
            if(t->child && t->child->next) {
                return atl_cursor_init_not(pool,
                                          _atl_cursor_open(pool, cb, t->child->next, arg),
                                          _atl_cursor_open(pool, cb, t->child, arg));
            }
        }
        return NULL;
    }
    else
        return cb(pool, t, arg);
}

atl_cursor_t *atl_cursor_open(aml_pool_t *pool, atl_cursor_custom_cb cb, atl_token_t *t, void *arg) {
    if(!t)
        return atl_cursor_init_empty(pool);
        
    atl_cursor_t *c = _atl_cursor_open(pool, cb, t, arg);
    if(!c)
        return atl_cursor_init_empty(pool);
    return c;
}
