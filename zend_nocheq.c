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
#ifndef ZEND_NOCHEQ
#define ZEND_NOCHEQ

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "zend_nocheq.h"

#define ZEND_NOCHEQ_EXTNAME   "NoCheq"
#define ZEND_NOCHEQ_VERSION   "0.0.1-dev"
#define ZEND_NOCHEQ_AUTHOR    "krakjoe"
#define ZEND_NOCHEQ_URL       "https://github.com/krakjoe/nocheq"
#define ZEND_NOCHEQ_COPYRIGHT "Copyright (c) 2019"

#if defined(__GNUC__) && __GNUC__ >= 4
# define ZEND_NOCHEQ_EXTENSION_API __attribute__ ((visibility("default")))
#else
# define ZEND_NOCHEQ_EXTENSION_API
#endif

static int ZEND_NOCHEQ_RECV_INIT = ZEND_VM_LAST_OPCODE+1,
           ZEND_NOCHEQ_RECV_VARIADIC = ZEND_VM_LAST_OPCODE+2,
           ZEND_NOCHEQ_VERIFY_RETURN = ZEND_VM_LAST_OPCODE+3;

static int  zend_nocheq_startup(zend_extension*);
static void zend_nocheq_shutdown(zend_extension *);
static void zend_nocheq_activate(void);
static void zend_nocheq_deactivate(void);

static zend_always_inline void zend_vm_register_opcode(int *opcode, int (*handler)(zend_execute_data*)) {
    while (zend_get_user_opcode_handler(*opcode)) {
        (*opcode)++;
    }

    ZEND_ASSERT((*opcode) < 256);

    zend_set_user_opcode_handler(*opcode, handler);
}

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

#define ZEND_VM_OPLINE     EX(opline)
#define ZEND_VM_USE_OPS    const zend_op_array *ops = (zend_op_array*) EX(func)
#define ZEND_VM_USE_OPLINE const zend_op *opline = EX(opline)
#define ZEND_VM_CONTINUE() return ZEND_USER_OPCODE_CONTINUE
#define ZEND_VM_NEXT()    do { \
    ZEND_VM_OPLINE = \
        ZEND_VM_OPLINE + 1; \
    ZEND_VM_CONTINUE(); \
} while(0)

int zend_nocheq_recv_init_handler(zend_execute_data *execute_data) {
    ZEND_VM_USE_OPS;
    ZEND_VM_USE_OPLINE;

    if (UNEXPECTED(opline->op1.num > EX_NUM_ARGS())) {
        zval     *param = EX_VAR(opline->result.var);

#if PHP_VERSION_ID < 70300
        ZVAL_COPY(param, EX_CONSTANT(opline->op2));

        if (Z_OPT_CONSTANT_P(param)) {
            if (UNEXPECTED(zval_update_constant_ex(param, ops->scope) != SUCCESS)) {
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

                if (UNEXPECTED(zval_update_constant_ex(param, ops->scope) != SUCCESS)) {
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
    ZEND_VM_USE_OPS;
    ZEND_VM_USE_OPLINE;
    uint32_t args   = opline->op1.num,
             count  = EX_NUM_ARGS();
    zval    *params = EX_VAR(opline->result.var);

    if (UNEXPECTED(args <= count)) {
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
    ZEND_VM_USE_OPS;
    ZEND_VM_USE_OPLINE;
    zval *ref, *val;
    zend_arg_info *info;
#if PHP_VERSION_ID < 80000
    zend_free_op free_op1;
#endif

    if (UNEXPECTED((opline->op1_type == IS_UNUSED))) {
        return ZEND_USER_OPCODE_DISPATCH_TO | ZEND_VERIFY_RETURN_TYPE;
    }

    info = ops->arg_info - 1;
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
        && !(ops->fn_flags & ZEND_ACC_RETURN_REFERENCE)
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

    ZEND_VM_NEXT();
}

int zend_nocheq_startup(zend_extension *ze)
{
    zend_vm_register_opcode(
        &ZEND_NOCHEQ_RECV_INIT,     zend_nocheq_recv_init_handler);
    zend_vm_register_opcode(
        &ZEND_NOCHEQ_RECV_VARIADIC, zend_nocheq_recv_variadic_handler);
    zend_vm_register_opcode(
        &ZEND_NOCHEQ_VERIFY_RETURN, zend_nocheq_verify_return_handler);

    ze->handle = 0;

    return SUCCESS;
}

void zend_nocheq_shutdown(zend_extension *ze)
{
    zend_set_user_opcode_handler(ZEND_NOCHEQ_RECV_INIT,     NULL);
    zend_set_user_opcode_handler(ZEND_NOCHEQ_RECV_VARIADIC, NULL);
    zend_set_user_opcode_handler(ZEND_NOCHEQ_VERIFY_RETURN, NULL);
}

void zend_nocheq_activate(void)
{
#if defined(ZTS) && defined(COMPILE_DL_NOCHEQ)
    ZEND_TSRMLS_CACHE_UPDATE();
#endif

    if (INI_INT("opcache.optimization_level")) {
        zend_string *optimizer = zend_string_init(
	        ZEND_STRL("opcache.optimization_level"), 1);
        zend_long level = INI_INT("opcache.optimization_level");
        zend_string *value;

        /* disable incompatible passes */
        level &= ~(1<<0);
        level &= ~(1<<3);
        level &= ~(1<<5);

        value = zend_strpprintf(0, "0x%08X", (unsigned int) level);

        zend_alter_ini_entry(optimizer, value,
	        ZEND_INI_SYSTEM, ZEND_INI_STAGE_ACTIVATE);

        zend_string_release(optimizer);
        zend_string_release(value);
    }
}

void zend_nocheq_deactivate(void)
{

}

void zend_nocheq_optimize(zend_op_array *ops) {
    if (!(ops->fn_flags & ZEND_ACC_HAS_TYPE_HINTS) &&
        !(ops->fn_flags & ZEND_ACC_HAS_RETURN_TYPE)) {
        /* no type information */
        return;
    }

    if (!(ops->fn_flags & ZEND_ACC_STRICT_TYPES)) {
        /* not strict code */
        return;
    }

    if (!ops->function_name) {
        /* a file */
        return;
    }

    {
        zend_op *opline = ops->opcodes,
                *end    = opline + ops->last;

        while (opline < end) {
            zend_arg_info *ai;

            switch (opline->opcode) {
                /* RECV is the best case, it can be optimized away completely if strict and not double */
                case ZEND_RECV:
                    if ((ops->fn_flags & ZEND_ACC_HAS_TYPE_HINTS)) {
                        ai = &ops->arg_info[opline->op1.num-1];

                        if (ZEND_TYPE_IS_SET(ai->type) &&
                            ZEND_TYPE_CODE(ai->type) != IS_DOUBLE) {
                            opline->opcode  = ZEND_NOP;
                        }
                    }
                break;

                /* Other cases will use user opcodes if strict and not double */
                case ZEND_RECV_INIT:
                    if ((ops->fn_flags & ZEND_ACC_HAS_TYPE_HINTS)) {
                        ai = &ops->arg_info[opline->op1.num-1];

                        if (ZEND_TYPE_IS_SET(ai->type) &&
                            ZEND_TYPE_CODE(ai->type) != IS_DOUBLE) {
                            opline->opcode = ZEND_NOCHEQ_RECV_INIT;
                            opline->handler = zend_nocheq_recv_init_handler;
                        }
                    }
                break;

                case ZEND_RECV_VARIADIC:
                    if ((ops->fn_flags & ZEND_ACC_HAS_TYPE_HINTS)) {
                        ai = &ops->arg_info[opline->op1.num-1];

                        if (ZEND_TYPE_IS_SET(ai->type) &&
                            ZEND_TYPE_CODE(ai->type) != IS_DOUBLE) {
                            opline->opcode = ZEND_NOCHEQ_RECV_VARIADIC;
                            opline->handler = zend_nocheq_recv_variadic_handler;
                        }
                    }
                break;

                case ZEND_VERIFY_RETURN_TYPE:
                    ai = ops->arg_info - 1;

                    if (ZEND_TYPE_IS_SET(ai->type) &&
                        ZEND_TYPE_CODE(ai->type) != IS_DOUBLE) {
                        opline->opcode = ZEND_NOCHEQ_VERIFY_RETURN;
                        opline->handler = zend_nocheq_verify_return_handler;
                    }
                break;
            }

            opline++;
        }
    }
}

ZEND_NOCHEQ_EXTENSION_API zend_extension_version_info extension_version_info = {
    ZEND_EXTENSION_API_NO,
    ZEND_EXTENSION_BUILD_ID
};

ZEND_NOCHEQ_EXTENSION_API zend_extension zend_extension_entry = {
    ZEND_NOCHEQ_EXTNAME,
    ZEND_NOCHEQ_VERSION,
    ZEND_NOCHEQ_AUTHOR,
    ZEND_NOCHEQ_URL,
    ZEND_NOCHEQ_COPYRIGHT,
    zend_nocheq_startup,
    zend_nocheq_shutdown,
    zend_nocheq_activate,
    zend_nocheq_deactivate,
    NULL,
    zend_nocheq_optimize,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    STANDARD_ZEND_EXTENSION_PROPERTIES
};

#if defined(ZTS) && defined(COMPILE_DL_NOCHEQ)
    ZEND_TSRMLS_CACHE_DEFINE()
#endif

#endif
