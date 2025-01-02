#include "a-tokenizer-library/atl_token.h"

int main(int argc, char *argv[]) {
    aml_pool_t *pool = aml_pool_init(16384);
    for( int i=1; i<argc; i++ ) {
        aml_pool_clear(pool);
        atl_token_t *tokens = atl_token_parse_expression(pool, argv[i], NULL, NULL);
        atl_token_dump(tokens);
    }
    aml_pool_destroy(pool);
    return 0;
}
