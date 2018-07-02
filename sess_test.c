/* Copyright 2018 Reuben Shaffer II. All rights reserved.
 *  released under the terms of LGPL v2.1
 */

#include "php_session.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

void print_indent(int indentlevel) {
    int i;
    for (i = 0; i < indentlevel; ++i) printf("    ");
}

void print_r_indent(php_session_var *var, int indentlevel) {
    size_t i;
    switch (var->type) {
        case PSV_NULL:
            printf("NULL\n");
            break;
        case PSV_BOOL:
            printf("%s\n", var->bval ? "TRUE" : "FALSE");
            break;
        case PSV_INT:
            printf("%ld\n", var->ival);
            break;
        case PSV_DOUBLE:
            printf("%f\n", var->dval);
            break;
        case PSV_STRING:
            printf("%s\n", var->sval.str);
            break;
        case PSV_ARRAY:
            printf("Array\n");
            print_indent(indentlevel);
            printf("(\n");
            for (i = 0; i < var->aval.len; ++i) {
                print_indent(indentlevel + 1);
                if (var->aval.vars[i].name_is_int) printf("    [%ld] => ", var->aval.vars[i].iname);
                printf("[%s] => ", var->aval.vars[i].name);
                print_r_indent(&var->aval.vars[i], indentlevel + 2);
            }
            print_indent(indentlevel);
            printf(")\n");
            break;
        case PSV_OBJECT:
            printf("%s Object\n", var->oval.name);
            print_indent(indentlevel);
            printf("{\n");
            for (i = 0; i < var->oval.len; ++i) {
                print_indent(indentlevel + 1);
                printf("%s => ", var->oval.props[i].name);
                print_r_indent(&var->oval.props[i], indentlevel + 2);
            }
            print_indent(indentlevel);
            printf("}\n");
            break;

    }
}

void print_r(php_session_array *arr) {
    size_t i;
    printf("Array\n(\n");
    for (i = 0; i < arr->len; ++i) {
        if (arr->vars[i].name_is_int) printf("    [%ld] => ", arr->vars[i].iname);
        else printf("    [%s] => ", arr->vars[i].name);
        print_r_indent(&arr->vars[i], 2);
    }
    printf(")\n");
}

// 100MB and that is it!
#define buf_size 104857600

int main(int argc, char **argv) {
    FILE *f;
    char *buf;
    size_t rb, buflen;
    php_session_array *_SESSION;
    int err;

    if (argc < 2) {
        printf("usage: %s session-file-name\n", argv[0]);
        return EXIT_FAILURE;
    }
    buf = calloc(buf_size, 1);
    if (!buf) {
        printf("memory allocation failure.\n");
        return EXIT_FAILURE;
    }
    f = fopen(argv[1], "r");
    if (!f) {
        printf("failed to open file: %s\n", argv[1]);
        free(buf);
        return EXIT_FAILURE;
    }
    buflen = 0;
    printf("reading from '%s'...", argv[1]);
    fflush(stdout);
    while (!feof(f)) {
        rb = fread(&buf[buflen], 1, buf_size - rb - 1, f);
        if (rb > 0) {
            buflen += rb;
            printf(".");
            fflush(stdout);
        }
        if (buflen > (buf_size - 10)) {
            printf("data may be truncated.");
            fflush(stdout);
            break;
        }
    }
    fclose(f);
    buf[buflen] = '\0';
    _SESSION = parse_php_session_document(buf, &err);
    if (!_SESSION) {
        printf("parsing failed: error %d\n", err);
        free(buf);
        return EXIT_FAILURE;
    }
    printf("parsing successful:\n_SESSION => ");
    print_r(_SESSION);
    free_php_session_array(_SESSION);
    return EXIT_SUCCESS;
}


