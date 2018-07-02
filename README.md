# libphpsession

A php session file parser written in C.
Single source, single header.  Includes an example in sess_test.c

This should build with any C compiler and work fine.
The code is not thoroughly tested.

My Makefile is a simple one written by hand that works with gcc/libtool on GNU/Linux systems.  I can only say that it compiled for me on the 2 systems I tried to compile it on since I wrote it.

--------( Functions )--------

php_session_array *parse_php_session_document(char *data, int *errp)
  data: a NULL terminated string
  errp: a pointer to an int to receive an error code in
    error codes:
    1 - memory allocation error
    2 - parsing error (could possibly also be a malloc failure
    3 - unexpected end of document
    * - (*errp) will not be cleared to zero or touched unless an error occurs
  return value: if successful, a php_session_array with the full session superglobal contents
                on failure, NULL will be returned and any allocated memory will be freed

void free_php_session_array(php_session_array *arr)

--------( Types )--------

php_session_string:
    size_t len
    char *str

php_session_array:
    size_t len
    php_session_var *vars

php_session_object:
    size_t namelen
    char *name
    size_t len
    php_session_var *props

php_session_var:
    unsigned char type
    unsigned char name_is_int
    size_t namelen
    (union)
        char *name
        long iname
    (union)
        php_session_string sval
        php_session_int ival
        php_session_double dval
        php_session_bool bval
        php_session_array aval
        php_session_object oval

php_session_bool: defined as unsigned char
php_session_int: defined as long
php_session_double: defined as double

I think you can see what most of the fields are.

php_session_var.name_is_int is a boolean field (0 or 1)
if name_is_int, use iname for name (used for array indices only)
otherwise use name

all the *val members in php_session_var are part of an anonymous union, so only use the correct one.

php_session_var.type is one of: PSV_NULL, PSV_STRING, PSV_INT, PSV_BOOL, PSV_DOUBLE, PSV_ARRAY, PSV_OBJECT

