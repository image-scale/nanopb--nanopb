#include "protolite.h"
#include "protolite_decode.h"
#include "protolite_common.h"

#ifdef PL_WITHOUT_64BIT
#define pl_int64_t int32_t
#define pl_uint64_t uint32_t
#else
#define pl_int64_t int64_t
#define pl_uint64_t uint64_t
#endif

static bool pl_message_set_to_defaults(pl_field_cursor_t *iter);
static bool read_raw_value(pl_istream_t *stream, pl_wire_type_t wire_type, pl_byte_t *buf, size_t *size);

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

/* Field decoders */

static bool pl_dec_bool(pl_istream_t *stream, const pl_field_cursor_t *field)
{
    return pl_decode_bool(stream, (bool*)field->pData);
}

static bool pl_dec_varint(pl_istream_t *stream, const pl_field_cursor_t *field)
{
    if (PL_DTYPE(field->type) == PL_DTYPE_UVARINT)
    {
        pl_uint64_t value, clamped;
        if (!pl_decode_varint(stream, &value))
            return false;

#ifndef PL_WITHOUT_64BIT
        if (field->data_size == sizeof(pl_uint64_t))
            clamped = *(pl_uint64_t*)field->pData = value;
        else
#endif
        if (field->data_size == sizeof(uint32_t))
            clamped = *(uint32_t*)field->pData = (uint32_t)value;
        else if (field->data_size == sizeof(uint_least16_t))
            clamped = *(uint_least16_t*)field->pData = (uint_least16_t)value;
        else if (field->data_size == sizeof(uint_least8_t))
            clamped = *(uint_least8_t*)field->pData = (uint_least8_t)value;
        else
            PL_RETURN_ERROR(stream, "invalid data_size");

        if (clamped != value)
            PL_RETURN_ERROR(stream, "integer too large");

        return true;
    }
    else
    {
        pl_uint64_t value;
        pl_int64_t svalue;
        pl_int64_t clamped;

        if (PL_DTYPE(field->type) == PL_DTYPE_SVARINT)
        {
            if (!pl_decode_svarint(stream, &svalue))
                return false;
        }
        else
        {
            if (!pl_decode_varint(stream, &value))
                return false;

            if (field->data_size == sizeof(pl_int64_t))
                svalue = (pl_int64_t)value;
            else
                svalue = (int32_t)value;
        }

#ifndef PL_WITHOUT_64BIT
        if (field->data_size == sizeof(pl_int64_t))
            clamped = *(pl_int64_t*)field->pData = svalue;
        else
#endif
        if (field->data_size == sizeof(int32_t))
            clamped = *(int32_t*)field->pData = (int32_t)svalue;
        else if (field->data_size == sizeof(int_least16_t))
            clamped = *(int_least16_t*)field->pData = (int_least16_t)svalue;
        else if (field->data_size == sizeof(int_least8_t))
            clamped = *(int_least8_t*)field->pData = (int_least8_t)svalue;
        else
            PL_RETURN_ERROR(stream, "invalid data_size");

        if (clamped != svalue)
            PL_RETURN_ERROR(stream, "integer too large");

        return true;
    }
}

static bool pl_dec_bytes(pl_istream_t *stream, const pl_field_cursor_t *field)
{
    uint32_t size;
    size_t alloc_size;
    pl_bytes_array_t *dest;

    if (!pl_decode_varint32(stream, &size))
        return false;

    if (size > PL_SIZE_MAX)
        PL_RETURN_ERROR(stream, "bytes overflow");

    alloc_size = PL_BYTES_ARRAY_T_ALLOCSIZE(size);
    if (size > alloc_size)
        PL_RETURN_ERROR(stream, "size too large");

    if (PL_ALLOC(field->type) == PL_ALLOC_POINTER)
    {
#ifndef PL_ENABLE_MALLOC
        PL_RETURN_ERROR(stream, "no malloc support");
#else
        if (stream->bytes_left < size)
            PL_RETURN_ERROR(stream, "end-of-stream");

        {
            void *ptr = *(void**)field->pData;
            ptr = pl_realloc(ptr, alloc_size);
            if (ptr == NULL)
                PL_RETURN_ERROR(stream, "realloc failed");
            *(void**)field->pData = ptr;
            dest = (pl_bytes_array_t*)ptr;
        }
#endif
    }
    else
    {
        if (alloc_size > field->data_size)
            PL_RETURN_ERROR(stream, "bytes overflow");
        dest = (pl_bytes_array_t*)field->pData;
    }

    dest->size = (pl_size_t)size;
    return pl_read(stream, dest->bytes, (size_t)size);
}

static bool pl_dec_string(pl_istream_t *stream, const pl_field_cursor_t *field)
{
    uint32_t size;
    size_t alloc_size;
    pl_byte_t *dest = (pl_byte_t*)field->pData;

    if (!pl_decode_varint32(stream, &size))
        return false;

    if (size == (uint32_t)-1)
        PL_RETURN_ERROR(stream, "size too large");

    alloc_size = (size_t)(size + 1);
    if (alloc_size < size)
        PL_RETURN_ERROR(stream, "size too large");

    if (PL_ALLOC(field->type) == PL_ALLOC_POINTER)
    {
#ifndef PL_ENABLE_MALLOC
        PL_RETURN_ERROR(stream, "no malloc support");
#else
        if (stream->bytes_left < size)
            PL_RETURN_ERROR(stream, "end-of-stream");

        {
            void *ptr = *(void**)field->pData;
            ptr = pl_realloc(ptr, alloc_size);
            if (ptr == NULL)
                PL_RETURN_ERROR(stream, "realloc failed");
            *(void**)field->pData = ptr;
            dest = (pl_byte_t*)ptr;
        }
#endif
    }
    else
    {
        if (alloc_size > field->data_size)
            PL_RETURN_ERROR(stream, "string overflow");
    }

    dest[size] = 0;

    if (!pl_read(stream, dest, (size_t)size))
        return false;

#ifdef PL_VALIDATE_UTF8
    if (!pl_validate_utf8((const char*)dest))
        PL_RETURN_ERROR(stream, "invalid utf8");
#endif

    return true;
}

static bool pl_decode_inner(pl_istream_t *stream, const pl_msg_descriptor_t *fields, void *dest_struct, unsigned int flags);

static bool pl_dec_submessage(pl_istream_t *stream, const pl_field_cursor_t *field)
{
    bool status = true;
    bool submsg_consumed = false;
    pl_istream_t substream;

    if (!pl_make_string_substream(stream, &substream))
        return false;

    if (field->submsg_desc == NULL)
        PL_RETURN_ERROR(stream, "invalid field descriptor");

    if (PL_DTYPE(field->type) == PL_DTYPE_SUBMSG_W_CB && field->pSize != NULL)
    {
        pl_callback_t *callback = (pl_callback_t*)field->pSize - 1;
        if (callback->funcs.decode)
        {
            status = callback->funcs.decode(&substream, field, &callback->arg);
            if (substream.bytes_left == 0)
                submsg_consumed = true;
        }
    }

    if (status && !submsg_consumed)
    {
        unsigned int flags = 0;

        if (PL_ALLOC(field->type) == PL_ALLOC_STATIC &&
            PL_CARD(field->type) != PL_CARD_REPEATED)
        {
            flags = PL_DECODE_NOINIT;
        }

        status = pl_decode_inner(&substream, field->submsg_desc, field->pData, flags);
    }

    if (!pl_close_string_substream(stream, &substream))
        return false;

    return status;
}

static bool pl_dec_fixed_length_bytes(pl_istream_t *stream, const pl_field_cursor_t *field)
{
    uint32_t size;

    if (!pl_decode_varint32(stream, &size))
        return false;

    if (size > PL_SIZE_MAX)
        PL_RETURN_ERROR(stream, "bytes overflow");

    if (size == 0)
    {
        memset(field->pData, 0, (size_t)field->data_size);
        return true;
    }

    if (size != field->data_size)
        PL_RETURN_ERROR(stream, "incorrect fixed length bytes size");

    return pl_read(stream, (pl_byte_t*)field->pData, (size_t)field->data_size);
}

static bool decode_basic_field(pl_istream_t *stream, pl_wire_type_t wire_type, pl_field_cursor_t *field)
{
    switch (PL_DTYPE(field->type))
    {
        case PL_DTYPE_BOOL:
            if (wire_type != PL_WT_VARINT && wire_type != PL_WT_PACKED)
                PL_RETURN_ERROR(stream, "wrong wire type");
            return pl_dec_bool(stream, field);

        case PL_DTYPE_VARINT:
        case PL_DTYPE_UVARINT:
        case PL_DTYPE_SVARINT:
            if (wire_type != PL_WT_VARINT && wire_type != PL_WT_PACKED)
                PL_RETURN_ERROR(stream, "wrong wire type");
            return pl_dec_varint(stream, field);

        case PL_DTYPE_FIXED32:
            if (wire_type != PL_WT_32BIT && wire_type != PL_WT_PACKED)
                PL_RETURN_ERROR(stream, "wrong wire type");
            return pl_decode_fixed32(stream, field->pData);

        case PL_DTYPE_FIXED64:
            if (wire_type != PL_WT_64BIT && wire_type != PL_WT_PACKED)
                PL_RETURN_ERROR(stream, "wrong wire type");
#ifdef PL_WITHOUT_64BIT
            PL_RETURN_ERROR(stream, "invalid data_size");
#else
            return pl_decode_fixed64(stream, field->pData);
#endif

        case PL_DTYPE_BYTES:
            if (wire_type != PL_WT_STRING)
                PL_RETURN_ERROR(stream, "wrong wire type");
            return pl_dec_bytes(stream, field);

        case PL_DTYPE_STRING:
            if (wire_type != PL_WT_STRING)
                PL_RETURN_ERROR(stream, "wrong wire type");
            return pl_dec_string(stream, field);

        case PL_DTYPE_SUBMESSAGE:
        case PL_DTYPE_SUBMSG_W_CB:
            if (wire_type != PL_WT_STRING)
                PL_RETURN_ERROR(stream, "wrong wire type");
            return pl_dec_submessage(stream, field);

        case PL_DTYPE_FIXED_LENGTH_BYTES:
            if (wire_type != PL_WT_STRING)
                PL_RETURN_ERROR(stream, "wrong wire type");
            return pl_dec_fixed_length_bytes(stream, field);

        default:
            PL_RETURN_ERROR(stream, "invalid field type");
    }
}

static bool decode_static_field(pl_istream_t *stream, pl_wire_type_t wire_type, pl_field_cursor_t *field)
{
    switch (PL_CARD(field->type))
    {
        case PL_CARD_REQUIRED:
            return decode_basic_field(stream, wire_type, field);

        case PL_CARD_OPTIONAL:
            if (field->pSize != NULL)
                *(bool*)field->pSize = true;
            return decode_basic_field(stream, wire_type, field);

        case PL_CARD_REPEATED:
            if (wire_type == PL_WT_STRING
                && PL_DTYPE(field->type) <= PL_DTYPE_LAST_PACKABLE)
            {
                bool status = true;
                pl_istream_t substream;
                pl_size_t *size = (pl_size_t*)field->pSize;
                field->pData = (char*)field->pField + field->data_size * (*size);

                if (!pl_make_string_substream(stream, &substream))
                    return false;

                while (substream.bytes_left > 0 && *size < field->array_size)
                {
                    if (!decode_basic_field(&substream, PL_WT_PACKED, field))
                    {
                        status = false;
                        break;
                    }
                    (*size)++;
                    field->pData = (char*)field->pData + field->data_size;
                }

                if (substream.bytes_left != 0)
                    PL_RETURN_ERROR(stream, "array overflow");
                if (!pl_close_string_substream(stream, &substream))
                    return false;

                return status;
            }
            else
            {
                pl_size_t *size = (pl_size_t*)field->pSize;
                field->pData = (char*)field->pField + field->data_size * (*size);

                if ((*size)++ >= field->array_size)
                    PL_RETURN_ERROR(stream, "array overflow");

                return decode_basic_field(stream, wire_type, field);
            }

        case PL_CARD_ONEOF:
            if (PL_DTYPE_IS_SUBMSG(field->type) &&
                *(pl_size_t*)field->pSize != field->tag)
            {
                memset(field->pData, 0, (size_t)field->data_size);
            }
            *(pl_size_t*)field->pSize = field->tag;
            return decode_basic_field(stream, wire_type, field);

        default:
            PL_RETURN_ERROR(stream, "invalid field type");
    }
}

static bool decode_callback_field(pl_istream_t *stream, pl_wire_type_t wire_type, pl_field_cursor_t *field)
{
    if (!field->descriptor->field_callback)
        return pl_skip_field(stream, wire_type);

    if (wire_type == PL_WT_STRING)
    {
        pl_istream_t substream;
        size_t prev_bytes_left;

        if (!pl_make_string_substream(stream, &substream))
            return false;

        if (PL_DTYPE(field->type) == PL_DTYPE_SUBMSG_W_CB && field->pSize != NULL)
        {
            pl_callback_t *callback;
            *(pl_size_t*)field->pSize = field->tag;
            callback = (pl_callback_t*)field->pSize - 1;

            if (callback->funcs.decode)
            {
                if (!callback->funcs.decode(&substream, field, &callback->arg))
                {
                    PL_SET_ERROR(stream, substream.errmsg ? substream.errmsg : "submsg callback failed");
                    return false;
                }
            }
        }

        do
        {
            prev_bytes_left = substream.bytes_left;
            if (!field->descriptor->field_callback(&substream, NULL, field))
            {
                PL_SET_ERROR(stream, substream.errmsg ? substream.errmsg : "callback failed");
                return false;
            }
        } while (substream.bytes_left > 0 && substream.bytes_left < prev_bytes_left);

        if (!pl_close_string_substream(stream, &substream))
            return false;

        return true;
    }
    else
    {
        pl_istream_t substream;
        pl_byte_t buffer[10];
        size_t size = sizeof(buffer);

        if (!read_raw_value(stream, wire_type, buffer, &size))
            return false;
        substream = pl_istream_from_buffer(buffer, size);

        return field->descriptor->field_callback(&substream, NULL, field);
    }
}

static bool read_raw_value(pl_istream_t *stream, pl_wire_type_t wire_type, pl_byte_t *buf, size_t *size)
{
    size_t max_size = *size;
    switch (wire_type)
    {
        case PL_WT_VARINT:
            *size = 0;
            do
            {
                (*size)++;
                if (*size > max_size)
                    PL_RETURN_ERROR(stream, "varint overflow");
                if (!pl_read(stream, buf, 1))
                    return false;
            } while (*buf++ & 0x80);
            return true;

        case PL_WT_64BIT:
            *size = 8;
            return pl_read(stream, buf, 8);

        case PL_WT_32BIT:
            *size = 4;
            return pl_read(stream, buf, 4);

        default:
            PL_RETURN_ERROR(stream, "invalid wire_type");
    }
}

static bool decode_field(pl_istream_t *stream, pl_wire_type_t wire_type, pl_field_cursor_t *field)
{
    switch (PL_ALLOC(field->type))
    {
        case PL_ALLOC_STATIC:
            return decode_static_field(stream, wire_type, field);

        case PL_ALLOC_CALLBACK:
            return decode_callback_field(stream, wire_type, field);

        case PL_ALLOC_POINTER:
#ifndef PL_ENABLE_MALLOC
            PL_RETURN_ERROR(stream, "no malloc support");
#else
            PL_RETURN_ERROR(stream, "pointer decode not implemented");
#endif

        default:
            PL_RETURN_ERROR(stream, "invalid field type");
    }
}

static bool pl_field_set_to_default(pl_field_cursor_t *field)
{
    pl_type_t type = field->type;

    if (PL_DTYPE(type) == PL_DTYPE_EXTENSION)
    {
        pl_extension_t *ext = *(pl_extension_t* const *)field->pData;
        while (ext != NULL)
        {
            ext->found = false;
            ext = ext->next;
        }
    }
    else if (PL_ALLOC(type) == PL_ALLOC_STATIC)
    {
        bool init_data = true;
        if (PL_CARD(type) == PL_CARD_OPTIONAL && field->pSize != NULL)
        {
            *(bool*)field->pSize = false;
        }
        else if (PL_CARD(type) == PL_CARD_REPEATED ||
                 PL_CARD(type) == PL_CARD_ONEOF)
        {
            *(pl_size_t*)field->pSize = 0;
            init_data = false;
        }

        if (init_data)
        {
            if (PL_DTYPE_IS_SUBMSG(field->type) &&
                (field->submsg_desc->default_value != NULL ||
                 field->submsg_desc->field_callback != NULL ||
                 field->submsg_desc->submsg_info[0] != NULL))
            {
                pl_field_cursor_t submsg_iter;
                if (pl_field_cursor_begin(&submsg_iter, field->submsg_desc, field->pData))
                {
                    if (!pl_message_set_to_defaults(&submsg_iter))
                        return false;
                }
            }
            else
            {
                memset(field->pData, 0, (size_t)field->data_size);
            }
        }
    }
    else if (PL_ALLOC(type) == PL_ALLOC_POINTER)
    {
        *(void**)field->pField = NULL;
        if (PL_CARD(type) == PL_CARD_REPEATED ||
            PL_CARD(type) == PL_CARD_ONEOF)
        {
            *(pl_size_t*)field->pSize = 0;
        }
    }

    return true;
}

static bool pl_message_set_to_defaults(pl_field_cursor_t *iter)
{
    pl_istream_t defstream = PL_ISTREAM_EMPTY;
    uint32_t tag = 0;
    pl_wire_type_t wire_type = PL_WT_VARINT;
    bool eof;

    if (iter->descriptor->default_value)
    {
        defstream = pl_istream_from_buffer(iter->descriptor->default_value, (size_t)-1);
        if (!pl_decode_tag(&defstream, &wire_type, &tag, &eof))
            return false;
    }

    do
    {
        if (!pl_field_set_to_default(iter))
            return false;

        if (tag != 0 && iter->tag == tag)
        {
            if (!decode_field(&defstream, wire_type, iter))
                return false;
            if (!pl_decode_tag(&defstream, &wire_type, &tag, &eof))
                return false;

            if (iter->pSize)
                *(bool*)iter->pSize = false;
        }
    } while (pl_field_cursor_next(iter));

    return true;
}

static bool default_extension_decoder(pl_istream_t *stream, pl_extension_t *extension, uint32_t tag, pl_wire_type_t wire_type)
{
    pl_field_cursor_t iter;

    if (!pl_field_cursor_begin_extension(&iter, extension))
        PL_RETURN_ERROR(stream, "invalid extension");

    if (iter.tag != tag || !iter.message)
        return true;

    extension->found = true;
    return decode_field(stream, wire_type, &iter);
}

static bool decode_extension(pl_istream_t *stream, uint32_t tag, pl_wire_type_t wire_type, pl_extension_t *extension)
{
    size_t pos = stream->bytes_left;

    while (extension != NULL && pos == stream->bytes_left)
    {
        bool status;
        if (extension->type->decode)
            status = extension->type->decode(stream, extension, tag, wire_type);
        else
            status = default_extension_decoder(stream, extension, tag, wire_type);

        if (!status)
            return false;

        extension = extension->next;
    }

    return true;
}

typedef struct {
    uint32_t bitfield[(PL_MAX_REQUIRED_FIELDS + 31) / 32];
} pl_fields_seen_t;

static bool pl_decode_inner(pl_istream_t *stream, const pl_msg_descriptor_t *fields, void *dest_struct, unsigned int flags)
{
    uint32_t extension_range_start = 0;
    pl_extension_t *extensions = NULL;

    uint32_t tag;
    pl_wire_type_t wire_type;
    bool eof;

    pl_fields_seen_t fields_seen = {{0, 0}};
    const uint32_t allbits = ~(uint32_t)0;

    pl_field_cursor_t iter;

    if (pl_field_cursor_begin(&iter, fields, dest_struct))
    {
        if ((flags & PL_DECODE_NOINIT) == 0)
        {
            if (!pl_message_set_to_defaults(&iter))
                PL_RETURN_ERROR(stream, "failed to set defaults");
        }
    }

    while (pl_decode_tag(stream, &wire_type, &tag, &eof))
    {
        if (tag == 0)
        {
            if (flags & PL_DECODE_NULLTERMINATED)
            {
                eof = true;
                break;
            }
            else
            {
                PL_RETURN_ERROR(stream, "zero tag");
            }
        }

        if (!pl_field_cursor_find(&iter, tag) || PL_DTYPE(iter.type) == PL_DTYPE_EXTENSION)
        {
            if (extension_range_start == 0)
            {
                if (pl_field_cursor_find_extension(&iter))
                {
                    extensions = *(pl_extension_t* const *)iter.pData;
                    extension_range_start = iter.tag;
                }

                if (!extensions)
                    extension_range_start = (uint32_t)-1;
            }

            if (tag >= extension_range_start)
            {
                size_t pos = stream->bytes_left;

                if (!decode_extension(stream, tag, wire_type, extensions))
                    return false;

                if (pos != stream->bytes_left)
                    continue;
            }

            if (!pl_skip_field(stream, wire_type))
                return false;
            continue;
        }

        if (PL_CARD(iter.type) == PL_CARD_REQUIRED
            && iter.required_field_index < PL_MAX_REQUIRED_FIELDS)
        {
            uint32_t tmp = ((uint32_t)1 << (iter.required_field_index & 31));
            fields_seen.bitfield[iter.required_field_index >> 5] |= tmp;
        }

        if (!decode_field(stream, wire_type, &iter))
            return false;
    }

    if (!eof)
        return false;

    {
        pl_size_t req_field_count = iter.descriptor->required_field_count;

        if (req_field_count > 0)
        {
            pl_size_t i;

            if (req_field_count > PL_MAX_REQUIRED_FIELDS)
                req_field_count = PL_MAX_REQUIRED_FIELDS;

            for (i = 0; i < (req_field_count >> 5); i++)
            {
                if (fields_seen.bitfield[i] != allbits)
                    PL_RETURN_ERROR(stream, "missing required field");
            }

            if ((req_field_count & 31) != 0)
            {
                if (fields_seen.bitfield[req_field_count >> 5] !=
                    (allbits >> (uint_least8_t)(32 - (req_field_count & 31))))
                {
                    PL_RETURN_ERROR(stream, "missing required field");
                }
            }
        }
    }

    return true;
}

bool pl_decode_message(pl_istream_t *stream, const pl_msg_descriptor_t *fields, void *dest_struct)
{
    return pl_decode_message_ex(stream, fields, dest_struct, 0);
}

bool pl_decode_message_ex(pl_istream_t *stream, const pl_msg_descriptor_t *fields, void *dest_struct, unsigned int flags)
{
    bool status;

    if ((flags & PL_DECODE_DELIMITED) == 0)
    {
        status = pl_decode_inner(stream, fields, dest_struct, flags);
    }
    else
    {
        pl_istream_t substream;
        if (!pl_make_string_substream(stream, &substream))
            return false;

        status = pl_decode_inner(&substream, fields, dest_struct, flags);

        if (!pl_close_string_substream(stream, &substream))
            status = false;
    }

#ifdef PL_ENABLE_MALLOC
    if (!status)
        pl_release(fields, dest_struct);
#endif

    return status;
}

void pl_release(const pl_msg_descriptor_t *fields, void *dest_struct)
{
#ifdef PL_ENABLE_MALLOC
    pl_field_cursor_t iter;

    if (!dest_struct)
        return;

    if (!pl_field_cursor_begin(&iter, fields, dest_struct))
        return;

    do
    {
        if (PL_ALLOC(iter.type) == PL_ALLOC_POINTER)
        {
            if (PL_CARD(iter.type) == PL_CARD_REPEATED)
                *(pl_size_t*)iter.pSize = 0;

            pl_free(*(void**)iter.pField);
            *(void**)iter.pField = NULL;
        }
    } while (pl_field_cursor_next(&iter));
#else
    PL_UNUSED(fields);
    PL_UNUSED(dest_struct);
#endif
}
