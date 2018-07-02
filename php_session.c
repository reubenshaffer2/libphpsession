/* Copyright 2018 Reuben Shaffer II. All rights reserved.
 *  released under the terms of LGPL v2.1
 */

#include <stdlib.h> // malloc, free, ...
#include <string.h> // memset, memcpy, ...
#include <ctype.h> // isalpha, isdigit, ...
#include "php_session.h" // stddef, user defined types

// this is my structure to keep track of where I am in the stream
typedef struct _php_session_token_t {
    char *dat;
    size_t len;
} php_session_token;
// I can use dat and len as a starting point for the next token

// If I do any more forward declarations, I'm just going to put them all up here
int _psp_val(php_session_token *tok, php_session_var *store);
void _psp_free_var(php_session_var *var);

// parser internal functions
// single char matches
int _psp_single_char(php_session_token *tok, char c) {
    if (tok->dat[tok->len] == c) {
        tok->dat = &(tok->dat[tok->len]);
        tok->len = 1;
        return 1;
    }
    return 0;
}

int _psp_semicolon(php_session_token *tok) {
    return _psp_single_char(tok, ';');
}

int _psp_colon(php_session_token *tok) {
    return _psp_single_char(tok, ':');
}

int _psp_bar(php_session_token *tok) {
    return _psp_single_char(tok, '|');
}

int _psp_dquot(php_session_token *tok) {
    return _psp_single_char(tok, '"');
}

int _psp_lbrace(php_session_token *tok) {
    return _psp_single_char(tok, '{');
}

int _psp_rbrace(php_session_token *tok) {
    return _psp_single_char(tok, '}');
}

int _psp_lbrack(php_session_token *tok) {
    return _psp_single_char(tok, '[');
}

int _psp_rbrack(php_session_token *tok) {
    return _psp_single_char(tok, ']');
}

int _psp_uline(php_session_token *tok) {
    return _psp_single_char(tok, '_');
}

int _psp_alpha(php_session_token *tok) {
    if (isalpha(tok->dat[tok->len])) {
        tok->dat = &(tok->dat[tok->len]);
        tok->len = 1;
        return 1;
    }
    return 0;
}

int _psp_digit(php_session_token *tok) {
    if (isdigit(tok->dat[tok->len])) {
        tok->dat = &(tok->dat[tok->len]);
        tok->len = 1;
        return 1;
    }
    return 0;
}

int _psp_alnum(php_session_token *tok) {
    return (_psp_alpha(tok) || _psp_digit(tok));
}

// matches up to count characters != '"'
int _psp_non_dquot_chars(php_session_token *tok, size_t count) {
    size_t i;
    for (i = 0; i < count; ++i) {
        if (tok->dat[tok->len + i] == '"') return 0;
    }
    tok->dat = &(tok->dat[tok->len]); 
    tok->len = count;
    return 1;
}

// matches an exact string of a specified length
int _psp_match_string(php_session_token *tok, const char *s, size_t len) {
    size_t i;
    for (i = 0; i < len; ++i) {
        if (tok->dat[tok->len + i] != s[i]) return 0;
    }
    tok->dat = &(tok->dat[tok->len]);
    tok->len = len;
    return 1;
}

// a refactored function for matching a signed/unsigned int token
// puts the result into *intptr if not NULL
int _psp_int_common(php_session_token *tok, long *intptr, int is_signed) {
    size_t i;
    char tmp[13];
    if (is_signed && tok->dat[tok->len] == '-') i = 1;
    else i = 0;
    while (tok->dat[tok->len + i] && isdigit(tok->dat[tok->len + i])) ++i;
    // this token cannot end at EOF or be zero length
    // I'm also capping the length at 12
    if (tok->dat[tok->len + i] && i > 0 && i < 12) {
        memset(tmp, 0, 13);
        memcpy(tmp, &tok->dat[tok->len], i);
        if (intptr) *intptr = atol(tmp);
        tok->dat = &tok->dat[tok->len];
        tok->len = i;
        return 1;
    }
    return 0;
}

int _psp_int(php_session_token *tok, long *intptr) {
    return _psp_int_common(tok, intptr, 1);
}

int _psp_unsigned_int(php_session_token *tok, long *intptr) {
    return _psp_int_common(tok, intptr, 0);
}

// unsure of whether PHP will serialize these in scientific notation or anything
// so I'm going to use a basic pattern that does not support it
// This is really a best guess without checking it out...
int _psp_double(php_session_token *tok, double *doubleptr) {
    size_t i;
    char tmp[43], *end; // no clue how long these could be
    double d;
    if (tok->dat[tok->len] == '-') i = 1;
    else i = 0;
    while (tok->dat[tok->len + i] && isdigit(tok->dat[tok->len + i])) ++i;
    if (tok->dat[tok->len + i] && tok->dat[tok->len + i] == '.') {
        ++i;
        while (tok->dat[tok->len + i] && isdigit(tok->dat[tok->len + i])) ++i;
    }
    if (tok->dat[tok->len + i] && i > 0 && i < 42) {
        memset(tmp, 0, 43);
        memcpy(tmp, &tok->dat[tok->len], i);
        // I'm going to use strtod to confirm that the pattern match was correct
        d = strtod(tmp, &end);
        if (&tmp[i] == end) {
            if (doubleptr) *doubleptr = strtod(tmp, &end);
            tok->dat = &tok->dat[tok->len];
            tok->len = i;
            return 1;
        }
    }
    return 0;
}

// PHP serialized "values"
int _psp_val_null(php_session_token *tok, php_session_var *store) {
    if (_psp_single_char(tok, 'N')) {
        store->type = PSV_NULL;
        return 1;
    }
    return 0;
}

int _psp_val_bool(php_session_token *tok, php_session_var *store) {
    if (_psp_match_string(tok, "b:0", 3) || _psp_match_string(tok, "b:1", 3)) {
        store->type = PSV_BOOL;
        store->bval = (tok->dat[2] == '1');
        return 1;
    }
    return 0;
}

int _psp_val_int(php_session_token *tok, php_session_var *store) {
    php_session_token mtok;
    memcpy(&mtok, tok, sizeof(php_session_token));
    if (_psp_single_char(tok, 'i') && _psp_colon(tok) && _psp_int(tok, &store->ival)) {
        store->type = PSV_INT;
        tok->dat = &mtok.dat[mtok.len];
        tok->len += 2;
        return 1;
    }
    memcpy(tok, &mtok, sizeof(php_session_token));
    return 0;
}

int _psp_val_double(php_session_token *tok, php_session_var *store) {
    php_session_token mtok;
    memcpy(&mtok, tok, sizeof(php_session_token));
    if (_psp_single_char(tok, 'd') && _psp_colon(tok) && _psp_double(tok, &store->dval)) {
        store->type = PSV_DOUBLE;
        tok->dat = &mtok.dat[mtok.len];
        tok->len += 2;
        return 1;
    }
    memcpy(tok, &mtok, sizeof(php_session_token));
    return 0;
}

int _psp_val_string(php_session_token *tok, php_session_var *store) {
    php_session_token mtok;
    memcpy(&mtok, tok, sizeof(php_session_token));
    if (_psp_single_char(tok, 's') && _psp_colon(tok) && _psp_unsigned_int(tok, &store->sval.len) && _psp_colon(tok) && _psp_dquot(tok)) {
        store->type = PSV_STRING;
        if (store->sval.str = malloc(store->sval.len + 1)) {
            if (_psp_non_dquot_chars(tok, store->sval.len)) {
                memcpy(store->sval.str, tok->dat, tok->len);
                store->sval.str[store->sval.len] = '\0';
                if (_psp_dquot(tok)) {
                    tok->len += (tok->dat - &mtok.dat[mtok.len]);
                    tok->dat = &mtok.dat[mtok.len];
                    return 1;
                }
            }
            free(store->sval.str);
        }
    }
    memcpy(tok, &mtok, sizeof(php_session_token));
    return 0;
}

// probably shouldn't have used the _psp_val functions like this, but it works
// match a "key" value of type "string" or, if allow_int_keys is true, type "int"
// can we have "bool" or "double" keys?  if so, this needs to be redone.
int _psp_element_key(php_session_token *tok, php_session_var *element, int allow_int_keys) {
    php_session_token mtok;
    php_session_var tmp;
    memcpy(&mtok, tok, sizeof(php_session_token));
    if (_psp_val_string(tok, &tmp) || (allow_int_keys && _psp_val_int(tok, &tmp))) {
        if (tmp.type == PSV_STRING) {
            element->name = tmp.sval.str;
            element->namelen = tmp.sval.len;
            element->name_is_int = 1;
        } else if (tmp.type == PSV_INT) {
            element->iname = tmp.ival;
            element->namelen = 0;
            element->name_is_int = 0;
        }
        return 1;
    }
    memcpy(tok, &mtok, sizeof(php_session_token));
    return 0;
}

// array element lists can contain int or string keys, followed by any type of value,
// separated by semicolon, kind of like a strided list with stride length of 2, but
// we go ahead and use the keys as php_session_value.{name,iname} rather than adding
// extra elements like that (should make lookups easier)
int _psp_array_element_list(php_session_token *tok, php_session_var *elements, size_t count) {
    size_t real_count;
    php_session_token mtok;
    memcpy(&mtok, tok, sizeof(php_session_token));
    if (_psp_lbrace(tok)) {
        real_count = 0;
        do {
            if (_psp_element_key(tok, &elements[real_count], 1) && _psp_semicolon(tok) && _psp_val(tok, &elements[real_count]) && _psp_semicolon(tok)) ++real_count;
        } while (real_count < count);
        if (real_count == count && _psp_rbrace(tok)) {
            tok->len += (tok->dat - &mtok.dat[mtok.len]);
            tok->dat = &mtok.dat[mtok.len];
            return 1;
        }
        while (real_count) _psp_free_var(&elements[--real_count]);
    }
    memcpy(tok, &mtok, sizeof(php_session_token));
    return 0;
}

// parses a serialized array
int _psp_val_array(php_session_token *tok, php_session_var *store) {
    php_session_token mtok;
    size_t i;
    memcpy(&mtok, tok, sizeof(php_session_token));
    if (_psp_single_char(tok, 'a') && _psp_colon(tok) && _psp_unsigned_int(tok, &store->aval.len) && _psp_colon(tok)) {
        store->type = PSV_ARRAY;
        if (store->aval.len) {
            if (store->aval.vars = calloc(store->aval.len, sizeof(php_session_var))) {
                if (_psp_array_element_list(tok, store->aval.vars, store->aval.len)) {
                    tok->len += (tok->dat - &mtok.dat[mtok.len]);
                    tok->dat = &mtok.dat[mtok.len];
                    return 1;
                }
                free(store->aval.vars);
            }
        } else {
            // empty array should be okay
            if (_psp_lbrack(tok) && _psp_rbrack(tok)) {
                tok->len += (tok->dat - &mtok.dat[mtok.len]);
                tok->dat = &mtok.dat[mtok.len];
                return 1;
            }
        }
    }
    memcpy(tok, &mtok, sizeof(php_session_token));
    return 0;
}

// generic "identifier" matching function requiring that an identifier
// (variable) name begins with a letter or underline character followed
// by any number of letters, numbers, or underline characters
int _psp_identifier(php_session_token *tok) {
    char *dat;
    size_t len;
    if (_psp_uline(tok) || _psp_alpha(tok)) {
        dat = tok->dat;
        len = 1;
        while (_psp_uline(tok) || _psp_alnum(tok)) ++len;
        tok->dat = dat;
        tok->len = len;
        return 1;
    }
    return 0;
}

// just barely different enough from arrays to need this
// could refactor into one that looks like this:
// int _psp_element_list(php_session_token *tok, php_session_var *elements, size_t count, int allow_int_keys, int (*nameval_sep)(php_session_token *));
// and call like this:
// _psp_element_list(tok, props, count, 0, _psp_colon);
// but it's kind of ugly to save 10 lines or so of code
int _psp_object_prop_list(php_session_token *tok, php_session_var *props, size_t count) {
    size_t real_count;
    php_session_token mtok;
    memcpy(&mtok, tok, sizeof(php_session_token));
    if (_psp_lbrace(tok)) {
        real_count = 0;
        do {
            if (_psp_element_key(tok, &props[real_count], 0) && _psp_colon(tok) && _psp_val(tok, &props[real_count])) ++real_count;
        } while (real_count < count && _psp_semicolon(tok));
        if (real_count == count && _psp_rbrace(tok)) {
            tok->len += (tok->dat - &mtok.dat[mtok.len]);
            tok->dat = &mtok.dat[mtok.len];
            return 1;
        }
        while (real_count) _psp_free_var(&props[--real_count]);
    }
    memcpy(tok, &mtok, sizeof(php_session_token));
    return 0;
}

// parses a serialized object
int _psp_val_object(php_session_token *tok, php_session_var *store) {
    php_session_token mtok;
    size_t i;
    memcpy(&mtok, tok, sizeof(php_session_token));
    if (_psp_single_char(tok, 'O') && _psp_colon(tok) && _psp_unsigned_int(tok, &store->oval.namelen) && _psp_colon(tok)) {
        // this "name" is, I think, the class name and, I think, has the same
        // naming requirements as a variable identifier.
        store->type = PSV_OBJECT;
        if (store->oval.name = malloc(store->oval.namelen + 1)) {
            if (_psp_identifier(tok)) {
                if (tok->len == store->oval.namelen) {
                    memcpy(store->name, tok->dat, tok->len);
                    store->oval.name[store->oval.namelen] = '\0';
                    if (_psp_unsigned_int(tok, &store->oval.len) && _psp_colon(tok)) {
                        if (store->oval.len) {
                            if (store->oval.props = calloc(store->oval.len, sizeof(php_session_var))) {
                                if (_psp_object_prop_list(tok, store->oval.props, store->oval.len)) {
                                    tok->len += (tok->dat - &mtok.dat[mtok.len]);
                                    tok->dat = &mtok.dat[mtok.len];
                                    return 1;
                                }
                                free(store->oval.props);
                            }
                        } else {
                            // no properties seems like it would be valid
                            if (_psp_lbrace(tok) && _psp_rbrace(tok)) {
                                tok->len += (tok->dat - &mtok.dat[mtok.len]);
                                tok->dat = &mtok.dat[mtok.len];
                                return 1;
                            }
                        }
                    }
                }
            }
            free(store->oval.name);
        }
    }
    memcpy(tok, &mtok, sizeof(php_session_token));
    return 0;
}

// parses any serialized "value" (only the value, not the name)
int _psp_val(php_session_token *tok, php_session_var *store) {
    return (_psp_val_null(tok, store) || _psp_val_int(tok, store) || _psp_val_bool(tok, store) || _psp_val_double(tok, store) || _psp_val_string(tok, store) || _psp_val_array(tok, store) || _psp_val_object(tok, store));
}

// parses only the serialized variable name and copies it to store->name
int _psp_varname(php_session_token *tok, php_session_var *store) {
    php_session_token mtok;
    memcpy(&mtok, tok, sizeof(php_session_token));
    if (_psp_identifier(tok)) {
        if (store->name = malloc(tok->len + 1)) {
            store->namelen = tok->len;
            store->name_is_int = 0;
            memcpy(store->name, tok->dat, tok->len);
            store->name[tok->len] = '\0';
            return 1;
        }
    }
    memcpy(tok, &mtok, sizeof(php_session_token));
    return 0;
}

// rootitem for document.  they are different: "identifier|value"
int _psp_rootitem(php_session_token *tok, php_session_var *store) {
    return (_psp_varname(tok, store) && _psp_bar(tok) && _psp_val(tok, store));
}

// the document is a little bit special.  it uses name|value;name|value...
// where regular arrays are more like name;value;name;value...
// and name is pretty much just like value
// also, the document has no clues about the length of this array
// so we must malloc/realloc on the heap for it.
php_session_array *parse_php_session_document(char *data, int *errp) {
    // I don't know the length of this one so I will have to malloc and realloc
    int cap, count;
    php_session_token tok;
    php_session_var *vars;
    php_session_array *ret;
    void *tmp;
    ret = NULL;
    vars = NULL;
    cap = 0;
    count = 0;
    tok.dat = data;
    tok.len = 0;
    do {
        if (cap == count) {
            tmp = realloc(vars, (cap + 4) * sizeof(php_session_var));
            if (tmp) {
                vars = tmp;
                memset(&vars[cap], 0, sizeof(php_session_var) * 4);
                cap += 4;
            } else {
                // failed realloc
                if (errp) *errp = 1;
                while (count) _psp_free_var(&vars[--count]);
                if (vars) free(vars);
                return NULL;
            }
        }
        if (_psp_rootitem(&tok, &vars[count])) {
            ++count;
        } else {
            if (errp) *errp = 2;
            while (count) _psp_free_var(&vars[--count]);
            if (vars) free(vars);
            return NULL;
        }
    } while (_psp_semicolon(&tok));
    if (tok.dat[tok.len] != '\0') {
        if (errp) *errp = 3;
        while (count) _psp_free_var(&vars[--count]);
        if (vars) free(vars);
        return NULL;
    }
    if (ret = calloc(1, sizeof(php_session_array))) {
        ret->len = count;
        ret->vars = vars;
    } else {
        if (errp) *errp = 1;
        while (count) _psp_free_var(&vars[--count]);
        if (vars) free(vars);
        ret = NULL;
    }
    return ret;
}

// does not free "var" but rather its contents
void _psp_free_string(php_session_string *var) {
    if (var->str && var->len) free(var->str);
}

// does not free "var" but rather its contents
void _psp_free_array(php_session_array *var) {
    size_t i;
    if (var->vars && var->len) {
        for (i = 0; i < var->len; ++i) _psp_free_var(&var->vars[i]);
        free(var->vars);
    }
}

// does not free "var" but rather its contents
void _psp_free_object(php_session_object *var) {
    size_t i;
    if (var->name && var->namelen) free(var->name);
    if (var->props && var->len) {
        for (i = 0; i < var->len; ++i) _psp_free_var(&var->props[i]);
        free(var->props);
    }
}

// does not free "var" but rather its contents
void _psp_free_var(php_session_var *var) {
    if ((!var->name_is_int) && var->name && var->namelen) free(var->name);
    switch (var->type) {
        case PSV_NULL:
        case PSV_INT:
        case PSV_DOUBLE:
        case PSV_BOOL:
            break;
        case PSV_STRING:
            _psp_free_string(&var->sval);
            break;
        case PSV_ARRAY:
            _psp_free_array(&var->aval);
            break;
        case PSV_OBJECT:
            _psp_free_object(&var->oval);
            break;
    }
}

// frees the entire array completely
void free_php_session_array(php_session_array *arr) {
    _psp_free_array(arr);
    free(arr);
}


