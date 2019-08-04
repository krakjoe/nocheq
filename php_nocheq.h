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

#ifndef PHP_NOCHEQ_H
# define PHP_NOCHEQ_H

extern zend_module_entry nocheq_module_entry;
# define phpext_nocheq_ptr &nocheq_module_entry

# define PHP_NOCHEQ_VERSION "0.1.0"

# if defined(ZTS) && defined(COMPILE_DL_NOCHEQ)
ZEND_TSRMLS_CACHE_EXTERN()
# endif

#endif	/* PHP_NOCHEQ_H */
