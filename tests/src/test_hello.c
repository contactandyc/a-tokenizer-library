#include "a-tokenizer-library/atl_token.h"
#include "the-io-library/io.h"

int main(int argc, char *argv[]) {
    aml_pool_t *pool = aml_pool_init(16384);
    for( int i=1; i<argc; i++ ) {
        aml_pool_clear(pool);
        size_t len;
        char *buffer = io_read_file(&len, argv[1]);
        if(!buffer)
            continue;
        atl_token_t *tokens = atl_token_parse(pool, buffer);
        atl_token_dump(tokens);
        aml_free(buffer);
    }
    return 0;
}
