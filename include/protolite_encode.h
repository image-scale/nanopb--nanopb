#ifndef PROTOLITE_ENCODE_H_INCLUDED
#define PROTOLITE_ENCODE_H_INCLUDED

#include "protolite.h"

#ifdef __cplusplus
extern "C" {
#endif

struct pl_ostream_s
{
    bool (*callback)(pl_ostream_t *stream, const pl_byte_t *buf, size_t count);
    void *state;
    size_t max_size;
    size_t bytes_written;
#ifndef PL_NO_ERRMSG
    const char *errmsg;
#endif
};

bool pl_encode_message(pl_ostream_t *stream, const pl_msg_descriptor_t *fields, const void *src_struct);

#define PL_ENCODE_DELIMITED       0x02U
#define PL_ENCODE_NULLTERMINATED  0x04U
bool pl_encode_message_ex(pl_ostream_t *stream, const pl_msg_descriptor_t *fields, const void *src_struct, unsigned int flags);

bool pl_get_encoded_size(size_t *size, const pl_msg_descriptor_t *fields, const void *src_struct);

pl_ostream_t pl_ostream_from_buffer(pl_byte_t *buf, size_t bufsize);

#ifndef PL_NO_ERRMSG
#define PL_OSTREAM_SIZING {0,0,0,0,0}
#else
#define PL_OSTREAM_SIZING {0,0,0,0}
#endif

bool pl_write(pl_ostream_t *stream, const pl_byte_t *buf, size_t count);

bool pl_encode_tag_for_field(pl_ostream_t *stream, const pl_field_cursor_t *field);

bool pl_encode_tag(pl_ostream_t *stream, pl_wire_type_t wiretype, uint32_t field_number);

#ifndef PL_WITHOUT_64BIT
bool pl_encode_varint(pl_ostream_t *stream, uint64_t value);
#else
bool pl_encode_varint(pl_ostream_t *stream, uint32_t value);
#endif

#ifndef PL_WITHOUT_64BIT
bool pl_encode_svarint(pl_ostream_t *stream, int64_t value);
#else
bool pl_encode_svarint(pl_ostream_t *stream, int32_t value);
#endif

bool pl_encode_string(pl_ostream_t *stream, const pl_byte_t *buffer, size_t size);

bool pl_encode_fixed32(pl_ostream_t *stream, const void *value);

#ifndef PL_WITHOUT_64BIT
bool pl_encode_fixed64(pl_ostream_t *stream, const void *value);
#endif

bool pl_encode_submessage(pl_ostream_t *stream, const pl_msg_descriptor_t *fields, const void *src_struct);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
