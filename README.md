# libphpsession

A php session file parser written in C.
Single source, single header.  Includes an example in sess_test.c

This should build with any C compiler and work fine.
The code is not thoroughly tested.

My Makefile is a simple one written by hand that works with gcc/libtool on GNU/Linux systems.  I
can only say that it compiled for me on the 2 systems I tried to compile it on since I wrote it.
If you use a different toolchain you probably will need to create a new Makefile.  It just didn't
seem like using autoconf for a single source/header was necessary.

Be advised, the parser is rather picky about the format.  It will not tolerate any leading or
trailing whitespace.  When testing, I copied and pasted a session from another system to test
with, and the trailing newline before the EOF that my editor added triggered a parsing error 2
and I had to manually delete it from the file before it would parse.  It also does not keep any
information about where a parsing error occurred or what it was.  It would be relatively simple
to keep track of the last successfully parsed token and add it to the struct that the parsing
functions use, but I had no need for it.  There is no function for serializing the data back to
its original string, but it would be pretty trivial to add it.

Possible portability issue: I used the size_t typedef quite a few times and this probably is
not portable to Windows.  Replacing these with long or adding a #define or typedef may work
just fine.  I meant to stick with the standard types/functions and just overlooked that it seems.

----------
## Functions

``php_session_array *parse_php_session_document(char *data, int *errp)``
  - data: a NULL terminated string
  - errp: a pointer to an int to receive an error code in
    - error codes:    
    - 1 - memory allocation error
    - 2 - parsing error (could possibly also be a malloc failure
    - 3 - unexpected end of document
    -\* - (\*errp) will not be cleared to zero or touched unless an error occurs
  - return value: if successful, a php_session_array with the full session superglobal contents
                on failure, NULL will be returned and any allocated memory will be freed

``
void free_php_session_array(php_session_array *arr)
``

----------
## Types

### The following are struct typedefs

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

### The following are basic typedefs
php_session_bool: ``unsigned char``

php_session_int: ``long``

php_session_double: ``double``


**php_session_var.name_is_int** is a *boolean* field (0 or 1)
if name_is_int, use *iname* for name (used for array indices only)
otherwise use *name*

all the *val* members in php_session_var are part of an anonymous union, so only use the correct one.

**php_session_var.type** is one of: PSV_NULL, PSV_STRING, PSV_INT, PSV_BOOL, PSV_DOUBLE, PSV_ARRAY, PSV_OBJECT

----------
## New Functions 

All of the following return a pointer to a php_session_var, or NULL if not found

Retrieve array element at index (type specific/no conversion):

``
get_php_session_array_index_str(php_session_array *arr, const char *index)
get_php_session_array_index_int(php_session_array *arr, long index)
``

Retrieve a property from an object:
``
get_php_session_object_property(php_session_object *obj, const char *prop)
``

