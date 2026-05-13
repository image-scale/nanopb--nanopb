#ifndef PROTOLITE_DECODE_H_INCLUDED
#define PROTOLITE_DECODE_H_INCLUDED

#include "protolite.h"

#ifdef __cplusplus
extern "C" {
#endif

struct pl_istream_s
{
    bool (*callback)(pl_istream_t *stream, pl_byte_t *buf, size_t count);
    void *state;
    size_t bytes_left;
#ifndef PL_NO_ERRMSG
    const char *errmsg;
#endif
};

#ifndef PL_NO_ERRMSG
#define PL_ISTREAM_EMPTY {0,0,0,0}
#else
#define PL_ISTREAM_EMPTY {0,0,0}
#endif

bool pl_decode_message(pl_istream_t *stream, const pl_msg_descriptor_t *fields, void *dest_struct);

#define PL_DECODE_NOINIT          0x01U
#define PL_DECODE_DELIMITED       0x02U
#define PL_DECODE_NULLTERMINATED  0x04U
bool pl_decode_message_ex(pl_istream_t *stream, const pl_msg_descriptor_t *fields, void *dest_struct, unsigned int flags);

void pl_release(const pl_msg_descriptor_t *fields, void *dest_struct);

pl_istream_t pl_istream_from_buffer(const pl_byte_t *buf, size_t msglen);

bool pl_read(pl_istream_t *stream, pl_byte_t *buf, size_t count);

bool pl_decode_tag(pl_istream_t *stream, pl_wire_type_t *wire_type, uint32_t *tag, bool *eof);

bool pl_skip_field(pl_istream_t *stream, pl_wire_type_t wire_type);

#ifndef PL_WITHOUT_64BIT
bool pl_decode_varint(pl_istream_t *stream, uint64_t *dest);
#else
#define pl_decode_varint pl_decode_varint32
#endif

bool pl_decode_varint32(pl_istream_t *stream, uint32_t *dest);

bool pl_decode_bool(pl_istream_t *stream, bool *dest);

#ifndef PL_WITHOUT_64BIT
bool pl_decode_svarint(pl_istream_t *stream, int64_t *dest);
#else
bool pl_decode_svarint(pl_istream_t *stream, int32_t *dest);
#endif

bool pl_decode_fixed32(pl_istream_t *stream, void *dest);

#ifndef PL_WITHOUT_64BIT
bool pl_decode_fixed64(pl_istream_t *stream, void *dest);
#endif

bool pl_make_string_substream(pl_istream_t *stream, pl_istream_t *substream);
bool pl_close_string_substream(pl_istream_t *stream, pl_istream_t *substream);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
