#include "protolite.h"
#include "protolite_encode.h"
#include "protolite_common.h"

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

static bool safe_read_bool(const void *pSize)
{
    const char *p = (const char *)pSize;
    size_t i;
    for (i = 0; i < sizeof(bool); i++)
    {
        if (p[i] != 0)
            return true;
    }
    return false;
}

static bool pl_enc_bool(pl_ostream_t *stream, const pl_field_cursor_t *field)
{
    uint32_t value = safe_read_bool(field->pData) ? 1 : 0;
    return pl_encode_varint(stream, value);
}

static bool pl_enc_varint(pl_ostream_t *stream, const pl_field_cursor_t *field)
{
    if (PL_DTYPE(field->type) == PL_DTYPE_UVARINT)
    {
        pl_uint64_t value = 0;

        if (field->data_size == sizeof(uint_least8_t))
            value = *(const uint_least8_t*)field->pData;
        else if (field->data_size == sizeof(uint_least16_t))
            value = *(const uint_least16_t*)field->pData;
        else if (field->data_size == sizeof(uint32_t))
            value = *(const uint32_t*)field->pData;
#ifndef PL_WITHOUT_64BIT
        else if (field->data_size == sizeof(pl_uint64_t))
            value = *(const pl_uint64_t*)field->pData;
#endif
        else
            PL_RETURN_ERROR(stream, "invalid data_size");

        return pl_encode_varint(stream, value);
    }
    else
    {
        pl_int64_t value = 0;

        if (field->data_size == sizeof(int_least8_t))
            value = *(const int_least8_t*)field->pData;
        else if (field->data_size == sizeof(int_least16_t))
            value = *(const int_least16_t*)field->pData;
        else if (field->data_size == sizeof(int32_t))
            value = *(const int32_t*)field->pData;
#ifndef PL_WITHOUT_64BIT
        else if (field->data_size == sizeof(pl_int64_t))
            value = *(const pl_int64_t*)field->pData;
#endif
        else
            PL_RETURN_ERROR(stream, "invalid data_size");

        if (PL_DTYPE(field->type) == PL_DTYPE_SVARINT)
            return pl_encode_svarint(stream, value);
#ifdef PL_WITHOUT_64BIT
        else if (value < 0)
            return pl_encode_varint_32(stream, (uint32_t)value, (uint32_t)-1);
#endif
        else
            return pl_encode_varint(stream, (pl_uint64_t)value);
    }
}

static bool pl_enc_fixed(pl_ostream_t *stream, const pl_field_cursor_t *field)
{
    if (field->data_size == sizeof(uint32_t))
    {
        return pl_encode_fixed32(stream, field->pData);
    }
#ifndef PL_WITHOUT_64BIT
    else if (field->data_size == sizeof(uint64_t))
    {
        return pl_encode_fixed64(stream, field->pData);
    }
#endif
    else
    {
        PL_RETURN_ERROR(stream, "invalid data_size");
    }
}

static bool pl_enc_bytes(pl_ostream_t *stream, const pl_field_cursor_t *field)
{
    const pl_bytes_array_t *bytes = (const pl_bytes_array_t*)field->pData;

    if (bytes == NULL)
        return pl_encode_string(stream, NULL, 0);

    if (PL_ALLOC(field->type) == PL_ALLOC_STATIC &&
        bytes->size > field->data_size - offsetof(pl_bytes_array_t, bytes))
    {
        PL_RETURN_ERROR(stream, "bytes size exceeded");
    }

    return pl_encode_string(stream, bytes->bytes, (size_t)bytes->size);
}

static bool pl_enc_string(pl_ostream_t *stream, const pl_field_cursor_t *field)
{
    size_t size = 0;
    size_t max_size = (size_t)field->data_size;
    const char *str = (const char*)field->pData;

    if (PL_ALLOC(field->type) == PL_ALLOC_POINTER)
    {
        max_size = (size_t)-1;
    }
    else
    {
        if (max_size == 0)
            PL_RETURN_ERROR(stream, "zero-length string");
        max_size -= 1;
    }

    if (str == NULL)
    {
        size = 0;
    }
    else
    {
        const char *p = str;
        while (size < max_size && *p != '\0')
        {
            size++;
            p++;
        }
        if (*p != '\0')
            PL_RETURN_ERROR(stream, "unterminated string");
    }

#ifdef PL_VALIDATE_UTF8
    if (!pl_validate_utf8(str))
        PL_RETURN_ERROR(stream, "invalid utf8");
#endif

    return pl_encode_string(stream, (const pl_byte_t*)str, size);
}

static bool pl_enc_submessage(pl_ostream_t *stream, const pl_field_cursor_t *field)
{
    if (field->submsg_desc == NULL)
        PL_RETURN_ERROR(stream, "invalid field descriptor");

    if (PL_DTYPE(field->type) == PL_DTYPE_SUBMSG_W_CB && field->pSize != NULL)
    {
        pl_callback_t *callback = (pl_callback_t*)field->pSize - 1;
        if (callback->funcs.encode)
        {
            if (!callback->funcs.encode(stream, field, &callback->arg))
                return false;
        }
    }

    return pl_encode_submessage(stream, field->submsg_desc, field->pData);
}

static bool pl_enc_fixed_length_bytes(pl_ostream_t *stream, const pl_field_cursor_t *field)
{
    return pl_encode_string(stream, (const pl_byte_t*)field->pData, (size_t)field->data_size);
}

static bool encode_basic_field(pl_ostream_t *stream, const pl_field_cursor_t *field)
{
    if (!field->pData)
        return true;

    if (!pl_encode_tag_for_field(stream, field))
        return false;

    switch (PL_DTYPE(field->type))
    {
        case PL_DTYPE_BOOL:
            return pl_enc_bool(stream, field);

        case PL_DTYPE_VARINT:
        case PL_DTYPE_UVARINT:
        case PL_DTYPE_SVARINT:
            return pl_enc_varint(stream, field);

        case PL_DTYPE_FIXED32:
        case PL_DTYPE_FIXED64:
            return pl_enc_fixed(stream, field);

        case PL_DTYPE_BYTES:
            return pl_enc_bytes(stream, field);

        case PL_DTYPE_STRING:
            return pl_enc_string(stream, field);

        case PL_DTYPE_SUBMESSAGE:
        case PL_DTYPE_SUBMSG_W_CB:
            return pl_enc_submessage(stream, field);

        case PL_DTYPE_FIXED_LENGTH_BYTES:
            return pl_enc_fixed_length_bytes(stream, field);

        default:
            PL_RETURN_ERROR(stream, "invalid field type");
    }
}

static bool encode_array(pl_ostream_t *stream, pl_field_cursor_t *field)
{
    pl_size_t i;
    pl_size_t count;

    count = *(pl_size_t*)field->pSize;

    if (count == 0)
        return true;

    if (PL_ALLOC(field->type) != PL_ALLOC_POINTER && count > field->array_size)
        PL_RETURN_ERROR(stream, "array max size exceeded");

    if (PL_DTYPE(field->type) <= PL_DTYPE_LAST_PACKABLE)
    {
        size_t size;
        if (!pl_encode_tag(stream, PL_WT_STRING, field->tag))
            return false;

        if (PL_DTYPE(field->type) == PL_DTYPE_FIXED32)
        {
            size = 4 * (size_t)count;
        }
        else if (PL_DTYPE(field->type) == PL_DTYPE_FIXED64)
        {
            size = 8 * (size_t)count;
        }
        else
        {
            pl_ostream_t sizestream = PL_OSTREAM_SIZING;
            void *pData_orig = field->pData;
            for (i = 0; i < count; i++)
            {
                if (!pl_enc_varint(&sizestream, field))
                    PL_RETURN_ERROR(stream, PL_GET_ERROR(&sizestream));
                field->pData = (char*)field->pData + field->data_size;
            }
            field->pData = pData_orig;
            size = sizestream.bytes_written;
        }

        if (!pl_encode_varint(stream, (pl_uint64_t)size))
            return false;

        if (stream->callback == NULL)
            return pl_write(stream, NULL, size);

        for (i = 0; i < count; i++)
        {
            if (PL_DTYPE(field->type) == PL_DTYPE_FIXED32 || PL_DTYPE(field->type) == PL_DTYPE_FIXED64)
            {
                if (!pl_enc_fixed(stream, field))
                    return false;
            }
            else
            {
                if (!pl_enc_varint(stream, field))
                    return false;
            }
            field->pData = (char*)field->pData + field->data_size;
        }
    }
    else
    {
        for (i = 0; i < count; i++)
        {
            if (PL_ALLOC(field->type) == PL_ALLOC_POINTER &&
                (PL_DTYPE(field->type) == PL_DTYPE_STRING ||
                 PL_DTYPE(field->type) == PL_DTYPE_BYTES))
            {
                field->pData = *(void* const*)field->pData;
            }
            if (!encode_basic_field(stream, field))
                return false;
            if (PL_ALLOC(field->type) == PL_ALLOC_POINTER &&
                (PL_DTYPE(field->type) == PL_DTYPE_STRING ||
                 PL_DTYPE(field->type) == PL_DTYPE_BYTES))
            {
                field->pData = *(char**)field->pField + field->data_size * (i + 1);
            }
            else
            {
                field->pData = (char*)field->pData + field->data_size;
            }
        }
    }

    return true;
}

static bool pl_check_proto3_default_value(const pl_field_cursor_t *field)
{
    pl_type_t type = field->type;

    if (PL_ALLOC(type) == PL_ALLOC_STATIC)
    {
        if (PL_CARD(type) == PL_CARD_REQUIRED)
            return false;
        else if (PL_CARD(type) == PL_CARD_REPEATED)
            return *(const pl_size_t*)field->pSize == 0;
        else if (PL_CARD(type) == PL_CARD_ONEOF)
            return *(const pl_size_t*)field->pSize == 0;
        else if (PL_CARD(type) == PL_CARD_OPTIONAL && field->pSize != NULL)
            return safe_read_bool(field->pSize) == false;
        else if (field->descriptor->default_value)
            return false;

        if (PL_DTYPE(type) <= PL_DTYPE_LAST_PACKABLE)
        {
            pl_size_t i;
            const char *p = (const char*)field->pData;
            for (i = 0; i < field->data_size; i++)
            {
                if (p[i] != 0)
                    return false;
            }
            return true;
        }
        else if (PL_DTYPE(type) == PL_DTYPE_BYTES)
        {
            const pl_bytes_array_t *bytes = (const pl_bytes_array_t*)field->pData;
            return bytes->size == 0;
        }
        else if (PL_DTYPE(type) == PL_DTYPE_STRING)
        {
            return *(const char*)field->pData == '\0';
        }
        else if (PL_DTYPE(type) == PL_DTYPE_FIXED_LENGTH_BYTES)
        {
            return field->data_size == 0;
        }
        else if (PL_DTYPE_IS_SUBMSG(type))
        {
            pl_field_cursor_t iter;
            if (pl_field_cursor_begin_const(&iter, field->submsg_desc, field->pData))
            {
                do
                {
                    if (!pl_check_proto3_default_value(&iter))
                        return false;
                } while (pl_field_cursor_next(&iter));
            }
            return true;
        }
    }
    else if (PL_ALLOC(type) == PL_ALLOC_POINTER)
    {
        return field->pData == NULL;
    }
    else if (PL_ALLOC(type) == PL_ALLOC_CALLBACK)
    {
        if (PL_DTYPE(type) == PL_DTYPE_EXTENSION)
        {
            const pl_extension_t *extension = *(const pl_extension_t* const *)field->pData;
            return extension == NULL;
        }
        else if (field->descriptor->field_callback == pl_default_field_callback)
        {
            pl_callback_t *pCallback = (pl_callback_t*)field->pData;
            return pCallback->funcs.encode == NULL;
        }
        else
        {
            return field->descriptor->field_callback == NULL;
        }
    }

    return false;
}

static bool encode_callback_field(pl_ostream_t *stream, const pl_field_cursor_t *field)
{
    if (field->descriptor->field_callback != NULL)
    {
        if (!field->descriptor->field_callback(NULL, stream, field))
            PL_RETURN_ERROR(stream, "callback error");
    }
    return true;
}

static bool default_extension_encoder(pl_ostream_t *stream, const pl_extension_t *extension);
static bool encode_extension_field(pl_ostream_t *stream, const pl_field_cursor_t *field);
static bool encode_field(pl_ostream_t *stream, pl_field_cursor_t *field);

static bool encode_field(pl_ostream_t *stream, pl_field_cursor_t *field)
{
    if (PL_CARD(field->type) == PL_CARD_ONEOF)
    {
        if (*(const pl_size_t*)field->pSize != field->tag)
            return true;
    }
    else if (PL_CARD(field->type) == PL_CARD_OPTIONAL)
    {
        if (field->pSize)
        {
            if (safe_read_bool(field->pSize) == false)
                return true;
        }
        else if (PL_ALLOC(field->type) == PL_ALLOC_STATIC)
        {
            if (pl_check_proto3_default_value(field))
                return true;
        }
    }

    if (!field->pData)
    {
        if (PL_CARD(field->type) == PL_CARD_REQUIRED)
            PL_RETURN_ERROR(stream, "missing required field");
        return true;
    }

    if (PL_ALLOC(field->type) == PL_ALLOC_CALLBACK)
    {
        return encode_callback_field(stream, field);
    }
    else if (PL_CARD(field->type) == PL_CARD_REPEATED)
    {
        return encode_array(stream, field);
    }
    else
    {
        return encode_basic_field(stream, field);
    }
}

static bool default_extension_encoder(pl_ostream_t *stream, const pl_extension_t *extension)
{
    pl_field_cursor_t iter;

    if (!pl_field_cursor_begin_extension_const(&iter, extension))
        PL_RETURN_ERROR(stream, "invalid extension");

    return encode_field(stream, &iter);
}

static bool encode_extension_field(pl_ostream_t *stream, const pl_field_cursor_t *field)
{
    const pl_extension_t *extension = *(const pl_extension_t* const *)field->pData;

    while (extension)
    {
        bool status;
        if (extension->type->encode)
            status = extension->type->encode(stream, extension);
        else
            status = default_extension_encoder(stream, extension);

        if (!status)
            return false;

        extension = extension->next;
    }

    return true;
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

bool pl_encode_message(pl_ostream_t *stream, const pl_msg_descriptor_t *fields, const void *src_struct)
{
    pl_field_cursor_t iter;
    if (!pl_field_cursor_begin_const(&iter, fields, src_struct))
        return true;

    do {
        if (PL_DTYPE(iter.type) == PL_DTYPE_EXTENSION)
        {
            if (!encode_extension_field(stream, &iter))
                return false;
        }
        else
        {
            if (!encode_field(stream, &iter))
                return false;
        }
    } while (pl_field_cursor_next(&iter));

    return true;
}

bool pl_encode_message_ex(pl_ostream_t *stream, const pl_msg_descriptor_t *fields, const void *src_struct, unsigned int flags)
{
    if ((flags & PL_ENCODE_DELIMITED) != 0)
    {
        return pl_encode_submessage(stream, fields, src_struct);
    }
    else if ((flags & PL_ENCODE_NULLTERMINATED) != 0)
    {
        const pl_byte_t zero = 0;

        if (!pl_encode_message(stream, fields, src_struct))
            return false;

        return pl_write(stream, &zero, 1);
    }
    else
    {
        return pl_encode_message(stream, fields, src_struct);
    }
}

bool pl_get_encoded_size(size_t *size, const pl_msg_descriptor_t *fields, const void *src_struct)
{
    pl_ostream_t stream = PL_OSTREAM_SIZING;

    if (!pl_encode_message(&stream, fields, src_struct))
        return false;

    *size = stream.bytes_written;
    return true;
}
