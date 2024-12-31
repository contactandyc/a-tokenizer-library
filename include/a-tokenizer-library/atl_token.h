#ifndef _atl_token_h
#define _atl_token_h

#include "a-memory-library/aml_pool.h"

struct atl_token_s;
typedef struct atl_token_s atl_token_t;

typedef enum { 
    ATL_TOKEN_TOKEN=0,
    ATL_TOKEN_NUMBER=10,
    ATL_TOKEN_OPERATOR=20,
    ATL_TOKEN_COMPARISON=30,
    ATL_TOKEN_MODIFIER=40,
    ATL_TOKEN_AND=50,
    ATL_TOKEN_OR=60,
    ATL_TOKEN_NOT=65,
    ATL_TOKEN_DASH=70,
    ATL_TOKEN_OPEN_PAREN=90,
    ATL_TOKEN_OPEN_BRACE=91,
    ATL_TOKEN_OPEN_BRACKET=92,
    ATL_TOKEN_QUOTE=93,
    ATL_TOKEN_DQUOTE=94,
    ATL_TOKEN_CLOSE_PAREN=100,
    ATL_TOKEN_CLOSE_BRACE=101,
    ATL_TOKEN_CLOSE_BRACKET=102,
    ATL_TOKEN_NULL=110,
    ATL_TOKEN_COLON=111,
    ATL_TOKEN_QUESTION=112,
    ATL_TOKEN_COMMA=130,
    ATL_TOKEN_SPACE=140,
    ATL_TOKEN_OTHER=150 } atl_token_type_t;

typedef enum {
    NORMAL=0, SKIP=1, NEXT=2, GLOBAL=3, NUMBER=4
} atl_token_cb_t;

struct atl_token_s {
    atl_token_type_t type;
    char *token;

    atl_token_cb_t attr_type;
    bool no_params;

    char **attrs;
    uint32_t num_attrs;

    atl_token_t *attr;

    atl_token_t *next;
    atl_token_t *prev;
    atl_token_t *child;
    atl_token_t *parent;
};

atl_token_t *atl_token_clone(aml_pool_t *pool, atl_token_t *t);

void atl_token_dump(atl_token_t *t);
atl_token_t *atl_token_parse(aml_pool_t *pool, const char *s);

typedef struct {
    aml_pool_t *pool;
    char *param;
    char **attrs;
    uint32_t num_attrs;

    bool no_params;
    bool strip;
    atl_token_t *alt_token;
} atl_token_cb_data_t;

/* this should return SKIP if a parameter is set */
typedef atl_token_cb_t (*atl_token_set_var_cb)(void *arg, atl_token_cb_data_t *d);

atl_token_t *atl_token_parse_expression(aml_pool_t *pool, const char *s,
                                        atl_token_set_var_cb cb, void *arg);

struct atl_token_dict_s;
typedef struct atl_token_dict_s atl_token_dict_t;
atl_token_dict_t *atl_token_dict_init();
atl_token_dict_t *atl_token_dict_load(const char *filename);
void atl_token_dict_destroy(atl_token_dict_t *h);

/* name: global|next|skip|normal|number [no_params] [strip] string */
bool atl_token_dict_add(atl_token_dict_t *h, const char *config_line);
const char *atl_token_dict_value(atl_token_dict_t *h, const char *param);
char **atl_token_dict_values(atl_token_dict_t *h, uint32_t *num_values, const char *param);

atl_token_cb_t atl_token_dict_cb(void *arg, atl_token_cb_data_t *d);

#endif