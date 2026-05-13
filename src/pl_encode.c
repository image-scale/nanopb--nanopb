#include "protolite.h"
#include "protolite_encode.h"

#ifdef PL_WITHOUT_64BIT
#define pl_int64_t int32_t
#define pl_uint64_t uint32_t
#else
#define pl_int64_t int64_t
#define pl_uint64_t uint64_t
#endif

static bool buf_write(pl_ostream_t *stream, const pl_byte_t *buf, size_t count)
{
    pl_byte_t *dest = (pl_byte_t*)stream->state;
    stream->state = dest + count;
    memcpy(dest, buf, count * sizeof(pl_byte_t));
    return true;
}

pl_ostream_t pl_ostream_from_buffer(pl_byte_t *buf, size_t bufsize)
{
    pl_ostream_t stream;
    stream.callback = &buf_write;
    stream.state = buf;
    stream.max_size = bufsize;
    stream.bytes_written = 0;
#ifndef PL_NO_ERRMSG
    stream.errmsg = NULL;
#endif
    return stream;
}

bool pl_write(pl_ostream_t *stream, const pl_byte_t *buf, size_t count)
{
    if (count > 0 && stream->callback != NULL)
    {
        if (stream->bytes_written + count < stream->bytes_written ||
            stream->bytes_written + count > stream->max_size)
        {
            PL_RETURN_ERROR(stream, "stream full");
        }

        if (!stream->callback(stream, buf, count))
            PL_RETURN_ERROR(stream, "io error");
    }

    stream->bytes_written += count;
    return true;
}

static bool pl_encode_varint_32(pl_ostream_t *stream, uint32_t low, uint32_t high)
{
    size_t i = 0;
    pl_byte_t buffer[10];
    pl_byte_t byte = (pl_byte_t)(low & 0x7F);
    low >>= 7;

    while (i < 4 && (low != 0 || high != 0))
    {
        byte |= 0x80;
        buffer[i++] = byte;
        byte = (pl_byte_t)(low & 0x7F);
        low >>= 7;
    }

    if (high)
    {
        byte = (pl_byte_t)(byte | ((high & 0x07) << 4));
        high >>= 3;

        while (high)
        {
            byte |= 0x80;
            buffer[i++] = byte;
            byte = (pl_byte_t)(high & 0x7F);
            high >>= 7;
        }
    }

    buffer[i++] = byte;

    return pl_write(stream, buffer, i);
}

bool pl_encode_varint(pl_ostream_t *stream, pl_uint64_t value)
{
    if (value <= 0x7F)
    {
        pl_byte_t byte = (pl_byte_t)value;
        return pl_write(stream, &byte, 1);
    }
    else
    {
#ifdef PL_WITHOUT_64BIT
        return pl_encode_varint_32(stream, value, 0);
#else
        return pl_encode_varint_32(stream, (uint32_t)value, (uint32_t)(value >> 32));
#endif
    }
}

bool pl_encode_svarint(pl_ostream_t *stream, pl_int64_t value)
{
    pl_uint64_t zigzagged;
    pl_uint64_t mask = ((pl_uint64_t)-1) >> 1;
    if (value < 0)
        zigzagged = ~(((pl_uint64_t)value & mask) << 1);
    else
        zigzagged = (pl_uint64_t)value << 1;

    return pl_encode_varint(stream, zigzagged);
}

bool pl_encode_fixed32(pl_ostream_t *stream, const void *value)
{
#if defined(PL_LITTLE_ENDIAN_8BIT) && PL_LITTLE_ENDIAN_8BIT == 1
    return pl_write(stream, (const pl_byte_t*)value, 4);
#else
    uint32_t val = *(const uint32_t*)value;
    pl_byte_t bytes[4];
    bytes[0] = (pl_byte_t)(val & 0xFF);
    bytes[1] = (pl_byte_t)((val >> 8) & 0xFF);
    bytes[2] = (pl_byte_t)((val >> 16) & 0xFF);
    bytes[3] = (pl_byte_t)((val >> 24) & 0xFF);
    return pl_write(stream, bytes, 4);
#endif
}

#ifndef PL_WITHOUT_64BIT
bool pl_encode_fixed64(pl_ostream_t *stream, const void *value)
{
#if defined(PL_LITTLE_ENDIAN_8BIT) && PL_LITTLE_ENDIAN_8BIT == 1
    return pl_write(stream, (const pl_byte_t*)value, 8);
#else
    uint64_t val = *(const uint64_t*)value;
    pl_byte_t bytes[8];
    bytes[0] = (pl_byte_t)(val & 0xFF);
    bytes[1] = (pl_byte_t)((val >> 8) & 0xFF);
    bytes[2] = (pl_byte_t)((val >> 16) & 0xFF);
    bytes[3] = (pl_byte_t)((val >> 24) & 0xFF);
    bytes[4] = (pl_byte_t)((val >> 32) & 0xFF);
    bytes[5] = (pl_byte_t)((val >> 40) & 0xFF);
    bytes[6] = (pl_byte_t)((val >> 48) & 0xFF);
    bytes[7] = (pl_byte_t)((val >> 56) & 0xFF);
    return pl_write(stream, bytes, 8);
#endif
}
#endif

bool pl_encode_tag(pl_ostream_t *stream, pl_wire_type_t wiretype, uint32_t field_number)
{
    pl_uint64_t tag = ((pl_uint64_t)field_number << 3) | wiretype;
    return pl_encode_varint(stream, tag);
}

bool pl_encode_string(pl_ostream_t *stream, const pl_byte_t *buffer, size_t size)
{
    if (!pl_encode_varint(stream, (pl_uint64_t)size))
        return false;

    return pl_write(stream, buffer, size);
}

bool pl_encode_tag_for_field(pl_ostream_t *stream, const pl_field_cursor_t *field)
{
    pl_wire_type_t wiretype;
    switch (PL_DTYPE(field->type))
    {
        case PL_DTYPE_BOOL:
        case PL_DTYPE_VARINT:
        case PL_DTYPE_UVARINT:
        case PL_DTYPE_SVARINT:
            wiretype = PL_WT_VARINT;
            break;

        case PL_DTYPE_FIXED32:
            wiretype = PL_WT_32BIT;
            break;

        case PL_DTYPE_FIXED64:
            wiretype = PL_WT_64BIT;
            break;

        case PL_DTYPE_BYTES:
        case PL_DTYPE_STRING:
        case PL_DTYPE_SUBMESSAGE:
        case PL_DTYPE_SUBMSG_W_CB:
        case PL_DTYPE_FIXED_LENGTH_BYTES:
            wiretype = PL_WT_STRING;
            break;

        default:
            PL_RETURN_ERROR(stream, "invalid field type");
    }

    return pl_encode_tag(stream, wiretype, field->tag);
}

bool pl_encode_submessage(pl_ostream_t *stream, const pl_msg_descriptor_t *fields, const void *src_struct)
{
    pl_ostream_t substream = PL_OSTREAM_SIZING;

    if (!pl_encode_message(&substream, fields, src_struct))
    {
#ifndef PL_NO_ERRMSG
        stream->errmsg = substream.errmsg;
#endif
        return false;
    }

    if (!pl_encode_varint(stream, (pl_uint64_t)substream.bytes_written))
        return false;

    if (stream->callback == NULL)
        return pl_write(stream, NULL, substream.bytes_written);

    if (stream->bytes_written + substream.bytes_written > stream->max_size)
        PL_RETURN_ERROR(stream, "stream full");

    {
        size_t size = substream.bytes_written;
        bool status;
        substream.callback = stream->callback;
        substream.state = stream->state;
        substream.max_size = size;
        substream.bytes_written = 0;
#ifndef PL_NO_ERRMSG
        substream.errmsg = NULL;
#endif

        status = pl_encode_message(&substream, fields, src_struct);

        stream->bytes_written += substream.bytes_written;
        stream->state = substream.state;
#ifndef PL_NO_ERRMSG
        stream->errmsg = substream.errmsg;
#endif

        if (substream.bytes_written != size)
            PL_RETURN_ERROR(stream, "submsg size changed");

        return status;
    }
}

/* Stub for pl_encode_message - will be fully implemented in Task 4 */
bool pl_encode_message(pl_ostream_t *stream, const pl_msg_descriptor_t *fields, const void *src_struct)
{
    PL_UNUSED(stream);
    PL_UNUSED(fields);
    PL_UNUSED(src_struct);
    return true;
}

bool pl_encode_message_ex(pl_ostream_t *stream, const pl_msg_descriptor_t *fields, const void *src_struct, unsigned int flags)
{
    PL_UNUSED(stream);
    PL_UNUSED(fields);
    PL_UNUSED(src_struct);
    PL_UNUSED(flags);
    return true;
}

bool pl_get_encoded_size(size_t *size, const pl_msg_descriptor_t *fields, const void *src_struct)
{
    pl_ostream_t stream = PL_OSTREAM_SIZING;

    if (!pl_encode_message(&stream, fields, src_struct))
        return false;

    *size = stream.bytes_written;
    return true;
}
