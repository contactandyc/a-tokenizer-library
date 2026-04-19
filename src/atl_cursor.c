// SPDX-FileCopyrightText: 2019–2026 Andy Curtis <contactandyc@gmail.com>
// SPDX-FileCopyrightText: 2024–2025 Knode.ai
// SPDX-License-Identifier: Apache-2.0
//
// Maintainer: Andy Curtis <contactandyc@gmail.com>

#include "a-tokenizer-library/atl_cursor.h"

#include "a-memory-library/aml_alloc.h"
#include "a-memory-library/aml_pool.h"
#include "a-memory-library/aml_buffer.h"

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

static double score_empty(atl_cursor_t *c) {
    (void)c;
    return 0.0;
}

atl_cursor_t *atl_cursor_init_empty(aml_pool_t *pool) {
    atl_cursor_t *r = (atl_cursor_t *)aml_pool_zalloc(pool, sizeof(atl_cursor_t));
    r->advance = advance_empty;
    r->advance_to = advance_empty_to;
    r->score = score_empty;
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
    uint32_t num = c->num_cursors;
    c->heap = (atl_cursor_t **)aml_pool_alloc(c->cursor.pool, sizeof(atl_cursor_t *) * (num+1) * 2);
    c->active = c->heap + (num+1);
    c->num_heap = 0;
    c->num_active = 0;

    for( uint32_t k=0; k<c->num_cursors; k++ ) {
        if(c->cursors[k]->advance(c->cursors[k])) {
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
    if(!advance_or_init(c))
        return atl_cursor_empty(&c->cursor);

    return c->cursor.advance_to((atl_cursor_t*)c, id);
}

static double score_or(atl_cursor_t *c) {
    or_cursor_t *or_c = (or_cursor_t *)c;
    double s = 0.0;
    for(uint32_t i = 0; i < or_c->num_active; i++) {
        if (or_c->active[i]->score) {
            s += or_c->active[i]->score(or_c->active[i]);
        }
    }
    return s;
}

static
void init_or(aml_pool_t *pool, or_cursor_t *r) {
    uint32_t num_cursors = 2;
    r->cursor.pool = pool;
    r->cursor.type = OR_CURSOR;
    r->cursor.advance = (atl_cursor_advance_cb)advance_or_init;
    r->cursor.advance_to = (atl_cursor_advance_to_cb)advance_or_to_init;
    r->cursor.add = (atl_cursor_add_cb)or_add;
    r->cursor.score = score_or;
    r->cursors = (atl_cursor_t **)aml_pool_alloc(pool, sizeof(atl_cursor_t *) * num_cursors);
    r->num_cursors = 0;
    r->cursor_size = num_cursors;
}

atl_cursor_t *atl_cursor_init_or(aml_pool_t *pool) {
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

static double score_not(atl_cursor_t *c) {
    not_cursor_t *not_c = (not_cursor_t *)c;
    if (not_c->pos->score) {
        return not_c->pos->score(not_c->pos);
    }
    return 0.0;
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
    r->cursor.score = score_not;
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
    if(!c->cursors[0]->advance(c->cursors[0]))
        return atl_cursor_empty(&c->cursor);
    uint32_t id = c->cursors[0]->id;
    while(i < c->num_cursors) {
        if(!c->cursors[i]->advance_to(c->cursors[i], id))
            return atl_cursor_empty(&c->cursor);
        uint32_t id2 = c->cursors[i]->id;
        if(id==id2)
            i++;
        else {
            i=0;
            id=id2;
        }
    }
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
        if(!c->cursors[i]->advance_to(c->cursors[i], id))
            return atl_cursor_empty(&c->cursor);
        uint32_t id2 = c->cursors[i]->id;
        if(id==id2)
            i++;
        else {
            i=0;
            id=id2;
        }
    }
    c->cursor.id = id;
    return true;
}

static double score_and(atl_cursor_t *c) {
    and_cursor_t *and_c = (and_cursor_t *)c;
    double s = 0.0;
    for(uint32_t i = 0; i < and_c->num_cursors; i++) {
        if (and_c->cursors[i]->score) {
            s += and_c->cursors[i]->score(and_c->cursors[i]);
        }
    }
    return s;
}

atl_cursor_t *atl_cursor_init_and(aml_pool_t *pool) {
    uint32_t num_cursors = 2;
    and_cursor_t *r = (and_cursor_t *)aml_pool_zalloc(pool, sizeof(and_cursor_t));
    r->cursor.pool = pool;
    r->cursor.type = AND_CURSOR;
    r->cursor.advance = (atl_cursor_advance_cb)advance_and;
    r->cursor.advance_to = (atl_cursor_advance_to_cb)advance_and_to;
    r->cursor.add = (atl_cursor_add_cb)and_add;
    r->cursor.score = score_and;
    r->cursors = (atl_cursor_t **)aml_pool_alloc(pool, sizeof(atl_cursor_t *) * num_cursors);
    r->num_cursors = 0;
    r->cursor_size = num_cursors;
    return (atl_cursor_t *)r;
}


// ---------------------------------------------------------
// POSITION TRACKER STRUCT
// ---------------------------------------------------------
typedef struct {
    uint32_t *current;
    uint32_t *end;
} pos_tracker_t;

// ---------------------------------------------------------
// PHRASE CURSOR IMPLEMENTATION
// ---------------------------------------------------------

static bool verify_phrase_positions(atl_cursor_t **cursors, uint32_t num_cursors) {
    if (num_cursors == 0) return false;
    if (num_cursors == 1) return true;

    pos_tracker_t trackers[32];
    if (num_cursors > 32) return false;

    for (uint32_t i = 0; i < num_cursors; i++) {
        if (cursors[i]->decode_positions) {
            cursors[i]->decode_positions(cursors[i], &trackers[i].current, &trackers[i].end);
            if (trackers[i].current == trackers[i].end) return false;
        } else {
            return false;
        }
    }

    while (trackers[0].current < trackers[0].end) {
        uint32_t target_pos = *(trackers[0].current) + 1;
        bool match = true;

        for (uint32_t i = 1; i < num_cursors; i++) {
            while (trackers[i].current < trackers[i].end && *(trackers[i].current) < target_pos) {
                trackers[i].current++;
            }

            if (trackers[i].current == trackers[i].end || *(trackers[i].current) != target_pos) {
                match = false;
                break;
            }
            target_pos++;
        }

        if (match) return true;
        trackers[0].current++;
    }

    return false;
}

static bool advance_phrase(and_cursor_t *c) {
    while (advance_and(c)) {
        if (verify_phrase_positions(c->cursors, c->num_cursors)) {
            return true;
        }
    }
    return atl_cursor_empty(&c->cursor);
}

static bool advance_phrase_to(and_cursor_t *c, uint32_t id) {
    if (id <= c->cursor.id) return true;

    while (advance_and_to(c, id)) {
        if (verify_phrase_positions(c->cursors, c->num_cursors)) {
            return true;
        }
        id = c->cursor.id + 1;
    }
    return atl_cursor_empty(&c->cursor);
}

static atl_cursor_t *atl_cursor_init_phrase(aml_pool_t *pool) {
    uint32_t num_cursors = 2;
    and_cursor_t *r = (and_cursor_t *)aml_pool_zalloc(pool, sizeof(and_cursor_t));
    r->cursor.pool = pool;
    r->cursor.type = PHRASE_CURSOR;
    r->cursor.advance = (atl_cursor_advance_cb)advance_phrase;
    r->cursor.advance_to = (atl_cursor_advance_to_cb)advance_phrase_to;
    r->cursor.add = (atl_cursor_add_cb)and_add;
    r->cursor.score = score_and; // Phrasing is scored identically to sum of its underlying terms
    r->cursors = (atl_cursor_t **)aml_pool_alloc(pool, sizeof(atl_cursor_t *) * num_cursors);
    r->num_cursors = 0;
    r->cursor_size = num_cursors;
    return (atl_cursor_t *)r;
}


// ---------------------------------------------------------
// PROXIMITY CURSOR IMPLEMENTATION
// ---------------------------------------------------------

typedef struct {
    and_cursor_t and_base;
    uint32_t max_window;
    bool ordered;
    bool scored;
    uint32_t best_span;
    double score;
} proximity_cursor_t;

// Mode 1: Fast Exit - Checks if ANY valid window exists
static bool verify_proximity_positions_boolean(proximity_cursor_t *prox, atl_cursor_t **cursors, uint32_t num_cursors) {
    if (num_cursors == 0) return false;
    if (num_cursors == 1) return true;

    pos_tracker_t trackers[32];
    if (num_cursors > 32) return false;

    for (uint32_t i = 0; i < num_cursors; i++) {
        if (cursors[i]->decode_positions) {
            cursors[i]->decode_positions(cursors[i], &trackers[i].current, &trackers[i].end);
            if (trackers[i].current == trackers[i].end) return false;
        } else {
            return false;
        }
    }

    uint32_t ideal_span = num_cursors - 1;
    uint32_t target_span = ideal_span + prox->max_window;

    while (true) {
        uint32_t min_pos = 0xFFFFFFFF;
        uint32_t max_pos = 0;
        uint32_t min_index = 0;
        bool valid_order = true;
        uint32_t last_pos = 0;

        for (uint32_t i = 0; i < num_cursors; i++) {
            uint32_t pos = *(trackers[i].current);
            if (pos < min_pos) { min_pos = pos; min_index = i; }
            if (pos > max_pos) { max_pos = pos; }

            if (prox->ordered) {
                if (i > 0 && pos <= last_pos) {
                    valid_order = false;
                }
                last_pos = pos;
            }
        }

        uint32_t span = max_pos - min_pos;

        if (span <= target_span && (!prox->ordered || valid_order)) {
            prox->score = 1.0;
            return true; // Fast exit!
        }

        trackers[min_index].current++;
        if (trackers[min_index].current == trackers[min_index].end) break;
    }

    return false;
}

// Mode 2: Exhaustive Search - Finds the optimal window for scoring
static bool verify_proximity_positions_scored(proximity_cursor_t *prox, atl_cursor_t **cursors, uint32_t num_cursors) {
    if (num_cursors == 0) return false;
    if (num_cursors == 1) {
        prox->best_span = 0;
        prox->score = 1.0;
        return true;
    }

    pos_tracker_t trackers[32];
    if (num_cursors > 32) return false;

    for (uint32_t i = 0; i < num_cursors; i++) {
        if (cursors[i]->decode_positions) {
            cursors[i]->decode_positions(cursors[i], &trackers[i].current, &trackers[i].end);
            if (trackers[i].current == trackers[i].end) return false;
        } else {
            return false;
        }
    }

    uint32_t min_found_span = 0xFFFFFFFF;
    bool found_match = false;
    uint32_t ideal_span = num_cursors - 1;
    uint32_t target_span = ideal_span + prox->max_window;

    while (true) {
        uint32_t min_pos = 0xFFFFFFFF;
        uint32_t max_pos = 0;
        uint32_t min_index = 0;
        bool valid_order = true;
        uint32_t last_pos = 0;

        for (uint32_t i = 0; i < num_cursors; i++) {
            uint32_t pos = *(trackers[i].current);
            if (pos < min_pos) { min_pos = pos; min_index = i; }
            if (pos > max_pos) { max_pos = pos; }

            if (prox->ordered) {
                if (i > 0 && pos <= last_pos) {
                    valid_order = false;
                }
                last_pos = pos;
            }
        }

        uint32_t span = max_pos - min_pos;

        if (span <= target_span && (!prox->ordered || valid_order)) {
            found_match = true;
            if (span < min_found_span) {
                min_found_span = span;
            }
            if (span == ideal_span) break; // Early exit for perfect match
        }

        trackers[min_index].current++;
        if (trackers[min_index].current == trackers[min_index].end) break;
    }

    if (found_match) {
        prox->best_span = min_found_span;
        prox->score = 1.0 / (1.0 + (min_found_span - ideal_span));
        return true;
    }

    return false;
}

// Scored Advancers
static bool advance_proximity_scored(proximity_cursor_t *c) {
    while (advance_and((and_cursor_t *)c)) {
        if (verify_proximity_positions_scored(c, c->and_base.cursors, c->and_base.num_cursors)) {
            return true;
        }
    }
    return atl_cursor_empty(&c->and_base.cursor);
}

static bool advance_proximity_to_scored(proximity_cursor_t *c, uint32_t id) {
    if (id <= c->and_base.cursor.id) return true;

    while (advance_and_to((and_cursor_t *)c, id)) {
        if (verify_proximity_positions_scored(c, c->and_base.cursors, c->and_base.num_cursors)) {
            return true;
        }
        id = c->and_base.cursor.id + 1;
    }
    return atl_cursor_empty(&c->and_base.cursor);
}

// Boolean Advancers
static bool advance_proximity_boolean(proximity_cursor_t *c) {
    while (advance_and((and_cursor_t *)c)) {
        if (verify_proximity_positions_boolean(c, c->and_base.cursors, c->and_base.num_cursors)) {
            return true;
        }
    }
    return atl_cursor_empty(&c->and_base.cursor);
}

static bool advance_proximity_to_boolean(proximity_cursor_t *c, uint32_t id) {
    if (id <= c->and_base.cursor.id) return true;

    while (advance_and_to((and_cursor_t *)c, id)) {
        if (verify_proximity_positions_boolean(c, c->and_base.cursors, c->and_base.num_cursors)) {
            return true;
        }
        id = c->and_base.cursor.id + 1;
    }
    return atl_cursor_empty(&c->and_base.cursor);
}

static double score_proximity(atl_cursor_t *c) {
    proximity_cursor_t *prox_c = (proximity_cursor_t *)c;
    double base_score = score_and((atl_cursor_t *)&(prox_c->and_base));
    if (prox_c->scored) {
        return base_score * prox_c->score;
    }
    return base_score;
}

static atl_cursor_t *atl_cursor_init_proximity(aml_pool_t *pool, uint32_t max_window, char **params, uint32_t num_params) {
    uint32_t num_cursors = 2;
    proximity_cursor_t *r = (proximity_cursor_t *)aml_pool_zalloc(pool, sizeof(proximity_cursor_t));
    r->and_base.cursor.pool = pool;
    r->and_base.cursor.type = PROXIMITY_CURSOR;
    r->and_base.cursor.add = (atl_cursor_add_cb)and_add;
    r->and_base.cursor.score = score_proximity;
    r->and_base.cursors = (atl_cursor_t **)aml_pool_alloc(pool, sizeof(atl_cursor_t *) * num_cursors);
    r->and_base.num_cursors = 0;
    r->and_base.cursor_size = num_cursors;

    r->max_window = max_window;
    r->ordered = false;
    r->scored = false;

    for(uint32_t i = 0; i < num_params; i++) {
        if (params[i]) {
            if (!strcasecmp(params[i], "ordered")) {
                r->ordered = true;
            } else if (!strcasecmp(params[i], "scored")) {
                r->scored = true;
            }
        }
    }

    // Wire the appropriate advancers
    if (r->scored) {
        r->and_base.cursor.advance = (atl_cursor_advance_cb)advance_proximity_scored;
        r->and_base.cursor.advance_to = (atl_cursor_advance_to_cb)advance_proximity_to_scored;
    } else {
        r->and_base.cursor.advance = (atl_cursor_advance_cb)advance_proximity_boolean;
        r->and_base.cursor.advance_to = (atl_cursor_advance_to_cb)advance_proximity_to_boolean;
    }

    return (atl_cursor_t *)r;
}


// ---------------------------------------------------------
// CORE
// ---------------------------------------------------------


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

static double score_range(atl_cursor_t *c) {
    (void)c;
    return 1.0;
}

atl_cursor_t *atl_cursor_range(aml_pool_t *pool, uint32_t start, uint32_t end) {
    range_cursor_t *r = (range_cursor_t *)aml_pool_zalloc(pool, sizeof(range_cursor_t));
    r->id = start+1;
    r->end = end;
    r->cursor.id = start;
    r->cursor._advance = (atl_cursor_advance_cb)advance_range;
    r->cursor.advance_to = (atl_cursor_advance_to_cb)advance_range_to;
    r->cursor.advance = (atl_cursor_advance_cb)post_reset_advance;
    r->cursor.score = score_range;
    r->cursor.type = NORMAL_CURSOR;
    return &(r->cursor);
}

static double score_id(atl_cursor_t *c) {
    (void)c;
    return 1.0;
}

atl_cursor_t *atl_cursor_init_id(aml_pool_t *pool, uint32_t id) {
    atl_cursor_t *r = (atl_cursor_t *)aml_pool_zalloc(pool, sizeof(atl_cursor_t));
    r->id = id;
    r->_advance = advance_empty;       // TODO: This is probably wrong
    r->advance_to = advance_empty_to;  // TODO: This is probably wrong
    r->advance = (atl_cursor_advance_cb)post_reset_advance;
    r->score = score_id;
    r->type = NORMAL_CURSOR;
    return r;
}

atl_cursor_t ** atl_cursor_subs(atl_cursor_t *c, uint32_t *num_sub ) {
    if(c->type == AND_CURSOR || c->type == PHRASE_CURSOR || c->type == PROXIMITY_CURSOR) {
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
            atl_cursor_t *resp;
            uint32_t window_size = 0;
            bool is_proximity = false;
            char **params = NULL;
            uint32_t num_params = 0;

            if(t->type == ATL_TOKEN_DQUOTE) {
                atl_token_t *lookahead = t->next;
                if (lookahead && lookahead->token[0] == '~') {
                    is_proximity = true;
                    aml_buffer_t *param_buf = aml_buffer_pool_init(pool, 32);
                    atl_token_t *curr = lookahead;

                    if (curr->token[1] != '\0') {
                        window_size = atoi(curr->token + 1);
                    } else if (curr->next && (curr->next->type == ATL_TOKEN_NUMBER || curr->next->type == ATL_TOKEN_TOKEN)) {
                        curr = curr->next;
                        window_size = atoi(curr->token);
                    }

                    curr = curr->next;
                    while (curr) {
                        if (curr->type == ATL_TOKEN_SPACE) break;

                        if (curr->type == ATL_TOKEN_COMMA) {
                            curr = curr->next;
                        } else {
                            char *p = curr->token;
                            aml_buffer_append(param_buf, &p, sizeof(char *));
                            num_params++;
                            curr = curr->next;
                        }
                    }

                    if (num_params > 0) {
                        params = (char **)aml_buffer_data(param_buf);
                    }

                    // Disconnect the consumed modifier tokens from the parsed AST
                    t->next = curr;
                    if (curr) {
                        curr->prev = t;
                    }
                }
            }

            if (is_proximity) {
                resp = atl_cursor_init_proximity(pool, window_size, params, num_params);
            } else if (t->type == ATL_TOKEN_DQUOTE) {
                resp = atl_cursor_init_phrase(pool);
            } else {
                resp = atl_cursor_init_and(pool);
            }

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

bool atl_cursor_match(aml_pool_t *pool, atl_cursor_custom_cb cb, atl_token_t *t, void *arg) {
    atl_cursor_t *c = atl_cursor_open(pool, cb, t, arg);
    if(!c)
        return false;
    return c->advance(c);
}
