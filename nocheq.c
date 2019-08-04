/*
  +----------------------------------------------------------------------+
  | nocheq                                                               |
  +----------------------------------------------------------------------+
  | Copyright (c) The PHP Group                                          |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: krakjoe                                                      |
  +----------------------------------------------------------------------+
 */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "php.h"
#include "ext/standard/info.h"
#include "php_nocheq.h"

static user_opcode_handler_t zend_vm_recv_handler;
static user_opcode_handler_t zend_vm_recv_init_handler;
static user_opcode_handler_t zend_vm_recv_variadic_handler;

static user_opcode_handler_t zend_vm_verify_return_handler;

static zend_string* zend_nocheq_namespace = NULL;

ZEND_TLS HashTable zend_nocheq_namespace_cache;

static zend_always_inline zval* zend_vm_get_zval(
        const zend_op *opline,
        int op_type,
        const znode_op *node,
        const zend_execute_data *execute_data,
#if PHP_VERSION_ID < 80000
        zend_free_op *should_free,
#endif
        int type) {
#if PHP_VERSION_ID >= 80000
	return zend_get_zval_ptr(opline, op_type, node, execute_data, type);
#elif PHP_VERSION_ID >= 70300
	return zend_get_zval_ptr(opline, op_type, node, execute_data, should_free, type);
#else
	return zend_get_zval_ptr(op_type, node, execute_data, should_free, type);
#endif
}

static zend_always_inline zend_bool zend_nocheq_namespace_check(zend_op_array *ops) {
    zend_bool *cache, result;

    if (NULL == zend_nocheq_namespace) {
        return 1;
    }

    if (NULL == ops->function_name) {
        return 1;
    }

    if ((cache = zend_hash_find_ptr(&zend_nocheq_namespace_cache, ops->function_name))) {
        return *cache;
    }

    result = zend_binary_strncasecmp(
                ZSTR_VAL(ops->function_name),
                ZSTR_LEN(ops->function_name),
                ZSTR_VAL(zend_nocheq_namespace),
                ZSTR_LEN(zend_nocheq_namespace),
                ZSTR_LEN(zend_nocheq_namespace)) == SUCCESS;

    cache = (zend_bool*)
        zend_hash_add_mem(
            &zend_nocheq_namespace_cache,
            ops->function_name,
            &result, sizeof(zend_bool));

    return *cache;
}

ZEND_INI_MH(OnUpdateNamespace)
{
    if (UNEXPECTED(zend_nocheq_namespace != NULL)) {
        return FAILURE;
    }

    if (0 == ZSTR_LEN(new_value) ||
        0 == ZSTR_VAL(new_value)[0]) {
        return FAILURE;
    }

    zend_nocheq_namespace =
        zend_new_interned_string(new_value);

    return SUCCESS;
}

ZEND_INI_BEGIN()
    ZEND_INI_ENTRY("nocheq.namespace", "", ZEND_INI_SYSTEM, OnUpdateNamespace)
ZEND_INI_END()

#define ZEND_VM_OPLINE     EX(opline)
#define ZEND_VM_USE_OPLINE const zend_op *opline = EX(opline)
#define ZEND_VM_CONTINUE() return ZEND_USER_OPCODE_CONTINUE
#define ZEND_VM_NEXT()    do { \
	ZEND_VM_OPLINE = \
        ZEND_VM_OPLINE + 1; \
	ZEND_VM_CONTINUE(); \
} while(0)

int zend_nocheq_recv_handler(zend_execute_data *execute_data) {
    ZEND_VM_USE_OPLINE;

    if (!zend_nocheq_namespace_check((zend_op_array*) EX(func))) {
        if (UNEXPECTED(zend_vm_recv_handler)) {
            return zend_vm_recv_handler(execute_data);
        }

        return ZEND_USER_OPCODE_DISPATCH;
    }

    if (UNEXPECTED(opline->op1.num > EX_NUM_ARGS())) {
        if (UNEXPECTED(zend_vm_recv_handler)) {
            return zend_vm_recv_handler(execute_data);
        }

        return ZEND_USER_OPCODE_DISPATCH;
    }

    ZEND_VM_NEXT();
}

int zend_nocheq_recv_init_handler(zend_execute_data *execute_data) {
    ZEND_VM_USE_OPLINE;
    uint32_t args;
    zval     *param;

    if (!zend_nocheq_namespace_check((zend_op_array*) EX(func))) {
        if (UNEXPECTED(zend_vm_recv_init_handler)) {
            return zend_vm_recv_init_handler(execute_data);
        }

        return ZEND_USER_OPCODE_DISPATCH;
    }

    if (UNEXPECTED(!(EX(func)->common.fn_flags & ZEND_ACC_HAS_TYPE_HINTS))) {
        if (UNEXPECTED(zend_vm_recv_init_handler)) {
            return zend_vm_recv_init_handler(execute_data);
        }
        return ZEND_USER_OPCODE_DISPATCH;
    }

    args  = opline->op1.num;
    param = EX_VAR(opline->result.var);

    if (args > EX_NUM_ARGS()) {
#if PHP_VERSION_ID < 70300
        ZVAL_COPY(param, EX_CONSTANT(opline->op2));

        if (Z_OPT_CONSTANT_P(param)) {
            if (UNEXPECTED(zval_update_constant_ex(param, EX(func)->op_array.scope) != SUCCESS)) {
                zval_ptr_dtor_nogc(param);
                ZVAL_UNDEF(param);
                ZEND_VM_CONTINUE();
            }
        }
#else
        zval *def = RT_CONSTANT(opline, opline->op2);

        if (Z_OPT_TYPE_P(def) == IS_CONSTANT_AST) {
            zval *cache = (zval*) CACHE_ADDR(Z_CACHE_SLOT_P(def));

            if (Z_TYPE_P(cache) != IS_UNDEF) {
                ZVAL_COPY_VALUE(param, cache);
            } else {
                ZVAL_COPY(param, def);

                if (UNEXPECTED(zval_update_constant_ex(param, EX(func)->op_array.scope) != SUCCESS)) {
                    zval_ptr_dtor_nogc(param);
                    ZVAL_UNDEF(param);
                    ZEND_VM_CONTINUE();
                }

                if (!Z_REFCOUNTED_P(param)) {
                    ZVAL_COPY_VALUE(cache, param);
                }
            }
        } else {
            ZVAL_COPY(param, def);
        }
#endif
    }

    ZEND_VM_NEXT();
}

int zend_nocheq_recv_variadic_handler(zend_execute_data *execute_data) {
    ZEND_VM_USE_OPLINE;
    uint32_t args, count;
    zval     *params;
    zend_op_array *ops = (zend_op_array*) EX(func);

    if (!zend_nocheq_namespace_check((zend_op_array*) EX(func))) {
        if (UNEXPECTED(zend_vm_recv_variadic_handler)) {
            return zend_vm_recv_variadic_handler(execute_data);
        }

        return ZEND_USER_OPCODE_DISPATCH;
    }

    if (UNEXPECTED(!(ops->fn_flags & ZEND_ACC_HAS_TYPE_HINTS))) {
        if (UNEXPECTED(zend_vm_recv_variadic_handler)) {
            return zend_vm_recv_variadic_handler(execute_data);
        }
        return ZEND_USER_OPCODE_DISPATCH;
    }

    args   = opline->op1.num;
    count  = EX_NUM_ARGS();
    params = EX_VAR(opline->result.var);

    if (args <= count) {
        zval *param;

        array_init_size(params, count - args + 1);
#if PHP_VERSION_ID >= 70300
        zend_hash_real_init_packed(Z_ARRVAL_P(params));
#else
        zend_hash_real_init(Z_ARRVAL_P(params), 1);
#endif
		ZEND_HASH_FILL_PACKED(Z_ARRVAL_P(params)) {
			param = EX_VAR_NUM(ops->last_var + ops->T);
			do {
				if (Z_OPT_REFCOUNTED_P(param)) Z_ADDREF_P(param);
				ZEND_HASH_FILL_ADD(param);
				param++;
			} while (++args <= count);
		} ZEND_HASH_FILL_END();
    } else {
        ZVAL_EMPTY_ARRAY(params);
    }

    ZEND_VM_NEXT();
}

int zend_nocheq_verify_return_handler(zend_execute_data *execute_data) {
    ZEND_VM_USE_OPLINE;
	zval *ref, *val;
    zend_arg_info *info;
#if PHP_VERSION_ID < 80000
    zend_free_op free_op1;
#endif

    if (!zend_nocheq_namespace_check((zend_op_array*) EX(func))) {
        if (UNEXPECTED(zend_vm_verify_return_handler)) {
            return zend_vm_verify_return_handler(execute_data);
        }

        return ZEND_USER_OPCODE_DISPATCH;
    }

    if (UNEXPECTED(!(EX(func)->common.fn_flags & ZEND_ACC_HAS_RETURN_TYPE))) {
        if (UNEXPECTED(zend_vm_verify_return_handler)) {
            return zend_vm_verify_return_handler(execute_data);
        }
        return ZEND_USER_OPCODE_DISPATCH;
    }

    if (UNEXPECTED(opline->op1_type == IS_UNUSED)) {
        if (UNEXPECTED(zend_vm_verify_return_handler)) {
            return zend_vm_verify_return_handler(execute_data);
        }
        return ZEND_USER_OPCODE_DISPATCH;
    }

	info = EX(func)->common.arg_info - 1;
	ref =
        val =
            zend_vm_get_zval(
                opline,
				opline->op1_type,
				&opline->op1,
				execute_data,
#if PHP_VERSION_ID < 80000
				&free_op1,
#endif
                BP_VAR_R);

	if (opline->op1_type == IS_CONST) {
		ZVAL_COPY(
            EX_VAR(opline->result.var), val);
		ref =
            val =
                EX_VAR(opline->result.var);
	} else if (opline->op1_type == IS_VAR) {
		if (UNEXPECTED(Z_TYPE_P(val) == IS_INDIRECT)) {
			val = Z_INDIRECT_P(val);
		}
		ZVAL_DEREF(val);
	} else if (opline->op1_type == IS_CV) {
		ZVAL_DEREF(val);
	}

	if (UNEXPECTED(!ZEND_TYPE_IS_CLASS(info->type)
		&& ZEND_TYPE_CODE(info->type) != IS_CALLABLE
		&& ZEND_TYPE_CODE(info->type) != IS_ITERABLE
		&& !ZEND_SAME_FAKE_TYPE(ZEND_TYPE_CODE(info->type), Z_TYPE_P(val))
		&& !(EX(func)->op_array.fn_flags & ZEND_ACC_RETURN_REFERENCE)
		&& ref != val)
	) {
		if (Z_REFCOUNT_P(ref) == 1) {
			ZVAL_UNREF(ref);
		} else {
			Z_DELREF_P(ref);
			ZVAL_COPY(ref, val);
		}
		val = ref;
	}

#if PHP_VERSION_ID < 80000
    if (free_op1) {
        zval_ptr_dtor_nogc(free_op1);
    }
#endif

    ZEND_VM_NEXT();
}

PHP_MINIT_FUNCTION(nocheq)
{
    zend_vm_recv_handler =
        zend_get_user_opcode_handler(ZEND_RECV);
    zend_set_user_opcode_handler(
        ZEND_RECV, zend_nocheq_recv_handler);

    zend_vm_recv_init_handler =
        zend_get_user_opcode_handler(ZEND_RECV_INIT);
    zend_set_user_opcode_handler(
        ZEND_RECV_INIT, zend_nocheq_recv_init_handler);

    zend_vm_recv_variadic_handler =
        zend_get_user_opcode_handler(ZEND_RECV_VARIADIC);
    zend_set_user_opcode_handler(
        ZEND_RECV_VARIADIC, zend_nocheq_recv_variadic_handler);

    zend_vm_verify_return_handler =
        zend_get_user_opcode_handler(ZEND_VERIFY_RETURN_TYPE);
    zend_set_user_opcode_handler(
        ZEND_VERIFY_RETURN_TYPE, zend_nocheq_verify_return_handler);

    REGISTER_INI_ENTRIES();

    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(nocheq)
{
    UNREGISTER_INI_ENTRIES();

    zend_set_user_opcode_handler(
        ZEND_RECV, zend_vm_recv_handler);

    zend_set_user_opcode_handler(
        ZEND_RECV_INIT, zend_vm_recv_init_handler);

    zend_set_user_opcode_handler(
        ZEND_RECV_VARIADIC, zend_vm_recv_variadic_handler);

    zend_set_user_opcode_handler(
        ZEND_VERIFY_RETURN_TYPE, zend_vm_verify_return_handler);

    return SUCCESS;
}

static void zend_nocheq_namespace_cache_free(zval *zv) {
    efree(Z_PTR_P(zv));
}

/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(nocheq)
{
#if defined(ZTS) && defined(COMPILE_DL_NOCHEQ)
	ZEND_TSRMLS_CACHE_UPDATE();
#endif

    zend_hash_init(&zend_nocheq_namespace_cache, 32, NULL, zend_nocheq_namespace_cache_free, 0);

	return SUCCESS;
}
/* }}} */

PHP_RSHUTDOWN_FUNCTION(nocheq)
{
    zend_hash_destroy(&zend_nocheq_namespace_cache);

    return SUCCESS;
}

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(nocheq)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "nocheq support", "enabled");
	php_info_print_table_end();
}
/* }}} */

/* {{{ nocheq_module_entry
 */
zend_module_entry nocheq_module_entry = {
	STANDARD_MODULE_HEADER,
	"nocheq",
	NULL,
	PHP_MINIT(nocheq),
	PHP_MSHUTDOWN(nocheq),
	PHP_RINIT(nocheq),
	PHP_RSHUTDOWN(nocheq),
	PHP_MINFO(nocheq),
	PHP_NOCHEQ_VERSION,
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_NOCHEQ
# ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
# endif
ZEND_GET_MODULE(nocheq)
#endif
