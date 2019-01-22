/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2011 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Anthony Terrien <forp@anthonyterrien.com>                    |
  | Author: ____Shies Gukai <gukai@bilibili.com>                         |
  +----------------------------------------------------------------------+
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "zend_exceptions.h"
#include "Zend/zend_vm.h"

#include "forp_log.h"
#include "forp.h"
#include "php_forp.h"

#if HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif


/* {{{ forp_execute
 */
ZEND_API void forp_execute_ex(zend_execute_data *execute_data)
{
    forp_node_t *n;
    zend_op_array *op_array = &(execute_data->func->op_array);
    zend_execute_data *edata = EG(current_execute_data)->prev_execute_data;

    n = forp_open_node(edata, op_array);
    ori_execute_ex(execute_data TSRMLS_CC);

    if (n && n->state < 2) forp_close_node(n TSRMLS_CC);
}
/* }}} */


/* {{{ forp_execute_internal
 */
ZEND_API void forp_execute_internal(zend_execute_data *current_execute_data, zval *return_value)
{
    zend_execute_data *current_data;
    forp_node_t *n;

    current_data = EG(current_execute_data);
    n = forp_open_node(current_data, NULL);

    if (ori_execute_internal) {
        ori_execute_internal(current_execute_data, return_value);
    } else {
        execute_internal(current_execute_data, return_value);
    }

    if (n && n->state < 2) forp_close_node(n TSRMLS_CC);
}
/* }}} */


/* {{{ forp_start
 */
void forp_start(TSRMLS_D) {
    if (FORP_G(started)) {
        php_error_docref(
            NULL TSRMLS_CC,
            E_NOTICE,
            "forp is already started."
        );
    } else {
        FORP_G(started) = 1;

#if HAVE_SYS_RESOURCE_H
        if(FORP_G(flags) & FORP_FLAG_CPU) {
            struct rusage ru;
            getrusage(RUSAGE_SELF, &ru);
            FORP_G(utime) = ru.ru_utime.tv_sec * 1000000.0 + ru.ru_utime.tv_usec;
            FORP_G(stime) = ru.ru_stime.tv_sec * 1000000.0 + ru.ru_stime.tv_usec;
        }
#endif

        // Proxying zend api methods
        ori_execute_ex = zend_execute_ex;
        zend_execute_ex = forp_execute_ex;

        if (!FORP_G(no_internals)) {
            ori_execute_internal = zend_execute_internal;
            zend_execute_internal = forp_execute_internal;
        }

        FORP_G(main) = forp_open_node(NULL, NULL TSRMLS_CC);
    }
}
/* }}} */

/* {{{ forp_end
 */
void forp_end(TSRMLS_D) {

    if (FORP_G(started)) {

#if HAVE_SYS_RESOURCE_H
        if (FORP_G(flags) & FORP_FLAG_CPU) {
            struct rusage ru;
            getrusage(RUSAGE_SELF, &ru);
            FORP_G(utime) = (ru.ru_utime.tv_sec * 1000000.0 + ru.ru_utime.tv_usec) - FORP_G(utime);
            FORP_G(stime) = (ru.ru_stime.tv_sec * 1000000.0 + ru.ru_stime.tv_usec) - FORP_G(stime);
        }
#endif

        // Closing ancestors
        while(FORP_G(current_node)) {
            forp_close_node(FORP_G(current_node) TSRMLS_CC);
        }

        // Restores Zend API methods
        if (ori_execute_ex) {
            zend_execute_ex = ori_execute_ex;
            ori_execute_ex = 0;
        }

        if (!FORP_G(no_internals)) {
            zend_execute_internal = ori_execute_internal;
        }
        // Stop
        FORP_G(started) = 0;
    }
}
/* }}} */


ZEND_DECLARE_MODULE_GLOBALS(forp);


const zend_function_entry forp_functions[] = {
    PHP_FE(forp_start, NULL)
    PHP_FE(forp_end, NULL)
    PHP_FE(forp_dump, NULL)
    PHP_FE(forp_print, NULL)
    PHP_FE(forp_info, NULL)
    PHP_FE(forp_enable, NULL)
    PHP_FE(forp_inspect, NULL)
    PHP_FE(forp_json, NULL)
    PHP_FE(forp_json_google_tracer, NULL)
    PHP_FE_END  /*PHP_FE_END*/
};

zend_module_entry forp_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
    STANDARD_MODULE_HEADER,
#endif
    "forp",
    forp_functions,
    PHP_MINIT(forp), // Main init
    PHP_MSHUTDOWN(forp), // Main shutdown
    PHP_RINIT(forp), // Request init
    PHP_RSHUTDOWN(forp), // Request shutdown
    PHP_MINFO(forp),
#if ZEND_MODULE_API_NO >= 20010901
    FORP_VERSION,
#endif
#if (PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION >= 2) || PHP_MAJOR_VERSION >= 6
    NO_MODULE_GLOBALS,
#endif
    /*PHP_GINIT(forp), PHP_GSHUTDOWN(forp),*/
    ZEND_MODULE_POST_ZEND_DEACTIVATE_N(forp),
    STANDARD_MODULE_PROPERTIES_EX
};

#ifdef COMPILE_DL_FORP
ZEND_GET_MODULE(forp)
#endif


PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("forp.max_nesting_level", "50", PHP_INI_ALL, OnUpdateLong, max_nesting_level, zend_forp_globals, forp_globals)
    STD_PHP_INI_BOOLEAN("forp.no_internals", "0", PHP_INI_ALL, OnUpdateBool, no_internals, zend_forp_globals, forp_globals)
    STD_PHP_INI_ENTRY("forp.inspect_depth_object", "2", PHP_INI_ALL, OnUpdateLong, inspect_depth_object, zend_forp_globals, forp_globals)
    STD_PHP_INI_ENTRY("forp.inspect_depth_array", "2", PHP_INI_ALL, OnUpdateLong, inspect_depth_object, zend_forp_globals, forp_globals)
PHP_INI_END()


static void php_forp_init_globals(zend_forp_globals *forp_globals)
{
    zval tmp;
    ZVAL_NULL(&tmp);

    forp_globals->started = 0;
    forp_globals->flags = FORP_FLAG_ALL;
    forp_globals->max_nesting_level = 50;
    forp_globals->no_internals = 0;
    forp_globals->stack_len = 0;
    forp_globals->nesting_level = 0;
    forp_globals->dump = tmp;
    forp_globals->stack = NULL;
    forp_globals->main = NULL;
    forp_globals->current_node = NULL;
    forp_globals->utime = 0;
    forp_globals->stime = 0;
    forp_globals->inspect = NULL;
    forp_globals->inspect_len = 0;
    forp_globals->inspect_depth_array = 2;
    forp_globals->inspect_depth_object = 2;
}

PHP_MSHUTDOWN_FUNCTION(forp) {
    return SUCCESS;
}

PHP_MINFO_FUNCTION(forp) {
    php_info_print_table_start();
    php_info_print_table_row(2, "Version", FORP_VERSION);
    php_info_print_table_end();
    DISPLAY_INI_ENTRIES();
}

ZEND_FUNCTION(forp_info) {
    php_info_print_style(TSRMLS_C);
    php_info_print_table_start();
    php_info_print_table_row(2, "Version", FORP_VERSION);
    php_info_print_table_end();
}

PHP_MINIT_FUNCTION(forp) {

    ZEND_INIT_MODULE_GLOBALS(forp, php_forp_init_globals, NULL);

    REGISTER_INI_ENTRIES();

    REGISTER_LONG_CONSTANT("FORP_FLAG_MEMORY", FORP_FLAG_MEMORY,
            CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("FORP_FLAG_TIME", FORP_FLAG_TIME,
            CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("FORP_FLAG_CPU", FORP_FLAG_CPU,
            CONST_CS | CONST_PERSISTENT);

    REGISTER_LONG_CONSTANT("FORP_FLAG_ALIAS", FORP_FLAG_ALIAS,
            CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("FORP_FLAG_CAPTION", FORP_FLAG_CAPTION,
            CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("FORP_FLAG_GROUPS", FORP_FLAG_GROUPS,
            CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("FORP_FLAG_HIGHLIGHT", FORP_FLAG_HIGHLIGHT,
            CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("FORP_FLAG_ANNOTATIONS", FORP_FLAG_ANNOTATIONS,
            CONST_CS | CONST_PERSISTENT);

    REGISTER_LONG_CONSTANT("FORP_FLAG_ALL", FORP_FLAG_ALL,
            CONST_CS | CONST_PERSISTENT);


    return SUCCESS;
}

PHP_RINIT_FUNCTION(forp) {
    zval tmp;
    ZVAL_NULL(&tmp);

    FORP_G(started) = 0;
    FORP_G(flags) = FORP_FLAG_ALL;
    FORP_G(stack_len) = 0;
    FORP_G(nesting_level) = 0;
    FORP_G(dump) = tmp;
    FORP_G(stack) = NULL;
    FORP_G(main) = NULL;
    FORP_G(current_node) = NULL;
    FORP_G(utime) = 0;
    FORP_G(stime) = 0;
    FORP_G(inspect) = NULL;
    FORP_G(inspect_len) = 0;

    // globals
    // FORP_G(max_nesting_level) = 50;
    // FORP_G(no_internals) = 0;
    // FORP_G(inspect_depth_array) = 2;
    // FORP_G(inspect_depth_object) = 2;

    return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(forp) {
    if (FORP_G(started)) {
        // Restores zend api methods
        if (ori_execute_ex) {
            zend_execute_ex = ori_execute_ex;
        }

        if (!FORP_G(no_internals)) {
            zend_execute_internal = ori_execute_internal;
        }
    }

    return SUCCESS;
}

ZEND_MODULE_POST_ZEND_DEACTIVATE_D(forp) {
    int i, j;

    TSRMLS_FETCH();

    // TODO track not terminated node

    FORP_G(started) = 0;
    FORP_G(nesting_level) = 0;
    FORP_G(current_node) = NULL;
    FORP_G(utime) = 0;
    FORP_G(stime) = 0;

    // stack dtor
    if (FORP_G(stack) != NULL) {
        for (i = 0; i < FORP_G(stack_len); ++i) {
            if(FORP_G(stack)[i]->function.groups) {
                free(FORP_G(stack)[i]->function.groups);
            }
            free(FORP_G(stack)[i]);
        }
        if (i) free(FORP_G(stack));
    }
    FORP_G(stack_len) = 0;
    FORP_G(stack) = NULL;

    // dump dtor
    if (Z_TYPE(FORP_G(dump)) != IS_NULL) zval_ptr_dtor(&FORP_G(dump));
    zval tmp;
    ZVAL_NULL(&tmp);
    FORP_G(dump) = tmp;

    // inspect
    if (FORP_G(inspect) != NULL) free(FORP_G(inspect));
    FORP_G(inspect) = NULL;

    return SUCCESS;
}

ZEND_FUNCTION(forp_stats) {
    int i;
    zval zvar;
    array_init(&zvar);
    if(FORP_G(inspect_len) > 0) {
        php_printf("\n\x1B[37m-\x1B[36mdebug\x1B[37m--------------------------------------------------------------------------%s", PHP_EOL);
        for (i = 0; i < FORP_G(inspect_len); ++i) {
            forp_stack_dump_cli_var(FORP_G(inspect)[i], 0 TSRMLS_CC);
        }
    }
    RETURN_ZVAL(&zvar, 1, 0);

}

ZEND_FUNCTION(forp_enable) {

    long opt = -1;
    php_error_docref(
                NULL TSRMLS_CC,
                E_USER_DEPRECATED,
                "forp_enable() is deprecated, use forp_start()."
                );

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|l", &opt) == FAILURE) {
        return;
    }
    if(opt >= 0) FORP_G(flags) = opt;
    forp_start(TSRMLS_C);
}

ZEND_FUNCTION(forp_start) {
    long opt = -1;
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|l", &opt) == FAILURE) {
        return;
    }
    if(opt >= 0) FORP_G(flags) = opt;

    forp_start(TSRMLS_C);
}

ZEND_FUNCTION(forp_end) {
    forp_end(TSRMLS_C);
}

ZEND_FUNCTION(forp_dump) {
    if (FORP_G(started)) {
        forp_end(TSRMLS_C);
    }
    if (Z_TYPE(FORP_G(dump)) == IS_NULL) {
        forp_stack_dump(TSRMLS_C);
    }
    RETURN_ZVAL(&FORP_G(dump), 1, 0);
}

ZEND_FUNCTION(forp_print) {
    if (FORP_G(started)) {
        forp_end(TSRMLS_C);
    }
    forp_stack_dump_cli(TSRMLS_C);
}

ZEND_FUNCTION(forp_inspect) {
    zval *expr;
    zend_string *name;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "Sz", &name, &expr) == FAILURE) {
        return;
    }

    forp_inspect_zval(name, expr TSRMLS_CC);

    RETURN_TRUE;
}

ZEND_FUNCTION(forp_json) {
    forp_json(TSRMLS_C);
}

ZEND_FUNCTION(forp_json_google_tracer) {
    zend_string *file_path;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "S", &file_path) == FAILURE) {
        return;
    }
    if (ZSTR_LEN(file_path) != strlen(ZSTR_VAL(file_path))) {
        return;
    }
    forp_json_google_tracer(ZSTR_VAL(file_path) TSRMLS_CC);
}