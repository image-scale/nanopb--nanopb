#include "protolite.h"
#include "protolite_decode.h"

#ifdef PL_WITHOUT_64BIT
#define pl_int64_t int32_t
#define pl_uint64_t uint32_t
#else
#define pl_int64_t int64_t
#define pl_uint64_t uint64_t
#endif

static bool buf_read(pl_istream_t *stream, pl_byte_t *buf, size_t count)
{
    const pl_byte_t *source = (const pl_byte_t*)stream->state;
    stream->state = (pl_byte_t*)stream->state + count;

    if (buf != NULL)
    {
        memcpy(buf, source, count * sizeof(pl_byte_t));
    }

    return true;
}

bool pl_read(pl_istream_t *stream, pl_byte_t *buf, size_t count)
{
    if (count == 0)
        return true;

    if (buf == NULL && stream->callback != buf_read)
    {
        pl_byte_t tmp[16];
        while (count > 16)
        {
            if (!pl_read(stream, tmp, 16))
                return false;
            count -= 16;
        }
        return pl_read(stream, tmp, count);
    }

    if (stream->bytes_left < count)
        PL_RETURN_ERROR(stream, "end-of-stream");

    if (!stream->callback(stream, buf, count))
        PL_RETURN_ERROR(stream, "io error");

    if (stream->bytes_left < count)
        stream->bytes_left = 0;
    else
        stream->bytes_left -= count;

    return true;
}

static bool pl_readbyte(pl_istream_t *stream, pl_byte_t *buf)
{
    if (stream->bytes_left == 0)
        PL_RETURN_ERROR(stream, "end-of-stream");

    if (!stream->callback(stream, buf, 1))
        PL_RETURN_ERROR(stream, "io error");

    stream->bytes_left--;

    return true;
}

pl_istream_t pl_istream_from_buffer(const pl_byte_t *buf, size_t msglen)
{
    pl_istream_t stream;
    union {
        void *state;
        const void *c_state;
    } state;
    stream.callback = &buf_read;
    state.c_state = buf;
    stream.state = state.state;
    stream.bytes_left = msglen;
#ifndef PL_NO_ERRMSG
    stream.errmsg = NULL;
#endif
    return stream;
}

bool pl_decode_varint32(pl_istream_t *stream, uint32_t *dest)
{
    pl_byte_t byte;
    uint32_t result;

    if (!pl_readbyte(stream, &byte))
        return false;

    if ((byte & 0x80) == 0)
    {
        result = byte;
    }
    else
    {
        uint_fast8_t bitpos = 7;
        result = byte & 0x7F;

        do
        {
            if (!pl_readbyte(stream, &byte))
                return false;

            if (bitpos >= 32)
            {
                pl_byte_t sign_extension = (bitpos < 63) ? 0xFF : 0x01;
                bool valid_extension = ((byte & 0x7F) == 0x00 ||
                         ((result >> 31) != 0 && byte == sign_extension));

                if (bitpos >= 64 || !valid_extension)
                {
                    PL_RETURN_ERROR(stream, "varint overflow");
                }
            }
            else if (bitpos == 28)
            {
                if ((byte & 0x70) != 0 && (byte & 0x78) != 0x78)
                {
                    PL_RETURN_ERROR(stream, "varint overflow");
                }
                result |= (uint32_t)(byte & 0x0F) << bitpos;
            }
            else
            {
                result |= (uint32_t)(byte & 0x7F) << bitpos;
            }
            bitpos = (uint_fast8_t)(bitpos + 7);
        } while (byte & 0x80);
    }

    *dest = result;
    return true;
}

#ifndef PL_WITHOUT_64BIT
bool pl_decode_varint(pl_istream_t *stream, uint64_t *dest)
{
    pl_byte_t byte;
    uint_fast8_t bitpos = 0;
    uint64_t result = 0;

    do
    {
        if (!pl_readbyte(stream, &byte))
            return false;

        if (bitpos >= 63 && (byte & 0xFE) != 0)
            PL_RETURN_ERROR(stream, "varint overflow");

        result |= (uint64_t)(byte & 0x7F) << bitpos;
        bitpos = (uint_fast8_t)(bitpos + 7);
    } while (byte & 0x80);

    *dest = result;
    return true;
}
#endif

bool pl_decode_bool(pl_istream_t *stream, bool *dest)
{
    uint32_t value;
    if (!pl_decode_varint32(stream, &value))
        return false;

    *dest = (value != 0);
    return true;
}

bool pl_decode_svarint(pl_istream_t *stream, pl_int64_t *dest)
{
    pl_uint64_t value;
    if (!pl_decode_varint(stream, &value))
        return false;

    if (value & 1)
        *dest = (pl_int64_t)(~(value >> 1));
    else
        *dest = (pl_int64_t)(value >> 1);

    return true;
}

bool pl_decode_fixed32(pl_istream_t *stream, void *dest)
{
    union {
        uint32_t fixed32;
        pl_byte_t bytes[4];
    } u;

    if (!pl_read(stream, u.bytes, 4))
        return false;

#if defined(PL_LITTLE_ENDIAN_8BIT) && PL_LITTLE_ENDIAN_8BIT == 1
    *(uint32_t*)dest = u.fixed32;
#else
    *(uint32_t*)dest = ((uint32_t)u.bytes[0] << 0) |
                       ((uint32_t)u.bytes[1] << 8) |
                       ((uint32_t)u.bytes[2] << 16) |
                       ((uint32_t)u.bytes[3] << 24);
#endif
    return true;
}

#ifndef PL_WITHOUT_64BIT
bool pl_decode_fixed64(pl_istream_t *stream, void *dest)
{
    union {
        uint64_t fixed64;
        pl_byte_t bytes[8];
    } u;

    if (!pl_read(stream, u.bytes, 8))
        return false;

#if defined(PL_LITTLE_ENDIAN_8BIT) && PL_LITTLE_ENDIAN_8BIT == 1
    *(uint64_t*)dest = u.fixed64;
#else
    *(uint64_t*)dest = ((uint64_t)u.bytes[0] << 0) |
                       ((uint64_t)u.bytes[1] << 8) |
                       ((uint64_t)u.bytes[2] << 16) |
                       ((uint64_t)u.bytes[3] << 24) |
                       ((uint64_t)u.bytes[4] << 32) |
                       ((uint64_t)u.bytes[5] << 40) |
                       ((uint64_t)u.bytes[6] << 48) |
                       ((uint64_t)u.bytes[7] << 56);
#endif
    return true;
}
#endif

static bool pl_skip_varint(pl_istream_t *stream)
{
    pl_byte_t byte;
    do
    {
        if (!pl_read(stream, &byte, 1))
            return false;
    } while (byte & 0x80);
    return true;
}

static bool pl_skip_string(pl_istream_t *stream)
{
    uint32_t length;
    if (!pl_decode_varint32(stream, &length))
        return false;

    if ((size_t)length != length)
        PL_RETURN_ERROR(stream, "size too large");

    return pl_read(stream, NULL, (size_t)length);
}

bool pl_decode_tag(pl_istream_t *stream, pl_wire_type_t *wire_type, uint32_t *tag, bool *eof)
{
    uint32_t temp;
    *eof = false;
    *wire_type = (pl_wire_type_t)0;
    *tag = 0;

    if (stream->bytes_left == 0)
    {
        *eof = true;
        return false;
    }

    if (!pl_decode_varint32(stream, &temp))
    {
        if (stream->callback != buf_read && stream->bytes_left == 0)
        {
#ifndef PL_NO_ERRMSG
            if (stream->errmsg != NULL && strcmp(stream->errmsg, "io error") == 0)
                stream->errmsg = NULL;
#endif
            *eof = true;
        }
        return false;
    }

    *tag = temp >> 3;
    *wire_type = (pl_wire_type_t)(temp & 7);
    return true;
}

bool pl_skip_field(pl_istream_t *stream, pl_wire_type_t wire_type)
{
    switch (wire_type)
    {
        case PL_WT_VARINT: return pl_skip_varint(stream);
        case PL_WT_64BIT: return pl_read(stream, NULL, 8);
        case PL_WT_STRING: return pl_skip_string(stream);
        case PL_WT_32BIT: return pl_read(stream, NULL, 4);
        case PL_WT_PACKED:
        default: PL_RETURN_ERROR(stream, "invalid wire_type");
    }
}

bool pl_make_string_substream(pl_istream_t *stream, pl_istream_t *substream)
{
    uint32_t size;
    if (!pl_decode_varint32(stream, &size))
        return false;

    *substream = *stream;
    if (substream->bytes_left < size)
        PL_RETURN_ERROR(stream, "parent stream too short");

    substream->bytes_left = (size_t)size;
    stream->bytes_left -= (size_t)size;
    return true;
}

bool pl_close_string_substream(pl_istream_t *stream, pl_istream_t *substream)
{
    if (substream->bytes_left) {
        if (!pl_read(substream, NULL, substream->bytes_left))
            return false;
    }

    stream->state = substream->state;

#ifndef PL_NO_ERRMSG
    stream->errmsg = substream->errmsg;
#endif
    return true;
}

/* Stubs for message-level decode - will be implemented in Task 5 */
bool pl_decode_message(pl_istream_t *stream, const pl_msg_descriptor_t *fields, void *dest_struct)
{
    PL_UNUSED(stream);
    PL_UNUSED(fields);
    PL_UNUSED(dest_struct);
    return true;
}

bool pl_decode_message_ex(pl_istream_t *stream, const pl_msg_descriptor_t *fields, void *dest_struct, unsigned int flags)
{
    PL_UNUSED(stream);
    PL_UNUSED(fields);
    PL_UNUSED(dest_struct);
    PL_UNUSED(flags);
    return true;
}

void pl_release(const pl_msg_descriptor_t *fields, void *dest_struct)
{
    PL_UNUSED(fields);
    PL_UNUSED(dest_struct);
}
