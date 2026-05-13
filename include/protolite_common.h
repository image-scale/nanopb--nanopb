#ifndef PROTOLITE_COMMON_H_INCLUDED
#define PROTOLITE_COMMON_H_INCLUDED

#include "protolite.h"

#ifdef __cplusplus
extern "C" {
#endif

bool pl_field_cursor_begin(pl_field_cursor_t *iter, const pl_msg_descriptor_t *desc, void *message);

bool pl_field_cursor_begin_extension(pl_field_cursor_t *iter, pl_extension_t *extension);

bool pl_field_cursor_begin_const(pl_field_cursor_t *iter, const pl_msg_descriptor_t *desc, const void *message);
bool pl_field_cursor_begin_extension_const(pl_field_cursor_t *iter, const pl_extension_t *extension);

bool pl_field_cursor_next(pl_field_cursor_t *iter);

bool pl_field_cursor_find(pl_field_cursor_t *iter, uint32_t tag);

bool pl_field_cursor_find_extension(pl_field_cursor_t *iter);

#ifdef PL_VALIDATE_UTF8
bool pl_validate_utf8(const char *s);
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
