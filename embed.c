/* embed.c */
#include "embed.h"
#include "http.h"

#include <sapi/embed/php_embed.h>

static int embed_cb_ub_write(const char *str, uint str_length TSRMLS_DC)
{
    const char *ptr = str;
    struct server_request *request = SG(server_context);

    http_response_add_body(request->resp, ptr, (size_t) str_length);

    return str_length;
}

static void embed_cb_flush(void *server_context)
{
    (void) server_context;
}

int embed_execute(char *file, struct server_request *request)
{
    zend_file_handle script;

    /* Set up a File Handle structure */
    script.type = ZEND_HANDLE_FP;
    script.filename = file;
    script.opened_path = NULL;
    script.free_filename = 0;

    if (!(script.handle.fp = fopen(script.filename, "rb"))) {
        return 403;
    }

    php_embed_module.ub_write = embed_cb_ub_write;
    php_embed_module.flush = embed_cb_flush;

    PHP_EMBED_START_BLOCK(0, NULL)
        SG(server_context) = (void *) request;
        php_execute_script(&script TSRMLS_CC);
    PHP_EMBED_END_BLOCK()

    return 200;
}
