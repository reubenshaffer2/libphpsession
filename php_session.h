#ifndef PHP_SESSION_H
#define PHP_SESSION_H 1

#include <stddef.h>

#define PSV_NULL        0
#define PSV_STRING      1
#define PSV_INT         2
#define PSV_BOOL        3
#define PSV_DOUBLE      4
#define PSV_ARRAY       5
#define PSV_OBJECT      6

typedef struct _php_session_string_t {
    size_t len;
    char *str;
} php_session_string;

typedef long php_session_int;
typedef double php_session_double;
typedef unsigned char php_session_bool;

struct _php_session_var_t;

typedef struct _php_session_array_t {
    size_t len;
    struct _php_session_var_t *vars;
} php_session_array;

typedef struct _php_session_object_t {
    size_t namelen;
    char *name;
    size_t len;
    struct _php_session_var_t *props;
} php_session_object;

typedef struct _php_session_var_t {
    unsigned char type, name_is_int;
    size_t namelen;
    union {
        char *name;
        long iname;
    };
    union {
        php_session_string sval;
        php_session_int ival;
        php_session_double dval;
        php_session_bool bval;
        php_session_array aval;
        php_session_object oval;
    };
} php_session_var;


/* parse_php_session_document:
 * data: a NULL terminated string
 * errp: a pointer to an int to receive an error code in
    error codes:
    1 - memory allocation error
    2 - parsing error (could possibly also be a malloc failure
    3 - unexpected end of document
    * - (*errp) will not be cleared to zero or touched unless an error occurs
 * return value: if successful, a php_session_array with the full session superglobal contents
 *               on failure, NULL will be returned and any allocated memory will be freed
*/
php_session_array *parse_php_session_document(char *data, int *errp);

/* free_php_session_array: frees the entire array and any children */
void free_php_session_array(php_session_array *arr);

#endif
