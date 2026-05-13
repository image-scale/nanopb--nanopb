#include "protolite_common.h"

static bool load_descriptor_values(pl_field_cursor_t *iter)
{
    uint32_t word0;
    uint32_t data_offset;
    int_least8_t size_offset;

    if (iter->index >= iter->descriptor->field_count)
        return false;

    word0 = iter->descriptor->field_info[iter->field_info_index];
    iter->type = (pl_type_t)((word0 >> 8) & 0xFF);

    switch (word0 & 3)
    {
        case 0: {
            iter->array_size = 1;
            iter->tag = (pl_size_t)((word0 >> 2) & 0x3F);
            size_offset = (int_least8_t)((word0 >> 24) & 0x0F);
            data_offset = (word0 >> 16) & 0xFF;
            iter->data_size = (pl_size_t)((word0 >> 28) & 0x0F);
            break;
        }

        case 1: {
            uint32_t word1 = iter->descriptor->field_info[iter->field_info_index + 1];
            iter->array_size = (pl_size_t)((word0 >> 16) & 0x0FFF);
            iter->tag = (pl_size_t)(((word0 >> 2) & 0x3F) | ((word1 >> 28) << 6));
            size_offset = (int_least8_t)((word0 >> 28) & 0x0F);
            data_offset = word1 & 0xFFFF;
            iter->data_size = (pl_size_t)((word1 >> 16) & 0x0FFF);
            break;
        }

        case 2: {
            uint32_t word1 = iter->descriptor->field_info[iter->field_info_index + 1];
            uint32_t word2 = iter->descriptor->field_info[iter->field_info_index + 2];
            uint32_t word3 = iter->descriptor->field_info[iter->field_info_index + 3];
            iter->array_size = (pl_size_t)(word0 >> 16);
            iter->tag = (pl_size_t)(((word0 >> 2) & 0x3F) | ((word1 >> 8) << 6));
            size_offset = (int_least8_t)(word1 & 0xFF);
            data_offset = word2;
            iter->data_size = (pl_size_t)word3;
            break;
        }

        default: {
            uint32_t word1 = iter->descriptor->field_info[iter->field_info_index + 1];
            uint32_t word2 = iter->descriptor->field_info[iter->field_info_index + 2];
            uint32_t word3 = iter->descriptor->field_info[iter->field_info_index + 3];
            uint32_t word4 = iter->descriptor->field_info[iter->field_info_index + 4];
            iter->array_size = (pl_size_t)word4;
            iter->tag = (pl_size_t)(((word0 >> 2) & 0x3F) | ((word1 >> 8) << 6));
            size_offset = (int_least8_t)(word1 & 0xFF);
            data_offset = word2;
            iter->data_size = (pl_size_t)word3;
            break;
        }
    }

    if (!iter->message)
    {
        iter->pField = NULL;
        iter->pSize = NULL;
    }
    else
    {
        iter->pField = (char*)iter->message + data_offset;

        if (size_offset)
        {
            iter->pSize = (char*)iter->pField - size_offset;
        }
        else if (PL_CARD(iter->type) == PL_CARD_REPEATED &&
                 (PL_ALLOC(iter->type) == PL_ALLOC_STATIC ||
                  PL_ALLOC(iter->type) == PL_ALLOC_POINTER))
        {
            iter->pSize = &iter->array_size;
        }
        else
        {
            iter->pSize = NULL;
        }

        if (PL_ALLOC(iter->type) == PL_ALLOC_POINTER && iter->pField != NULL)
        {
            iter->pData = *(void**)iter->pField;
        }
        else
        {
            iter->pData = iter->pField;
        }
    }

    if (PL_DTYPE_IS_SUBMSG(iter->type))
    {
        iter->submsg_desc = iter->descriptor->submsg_info[iter->submessage_index];
    }
    else
    {
        iter->submsg_desc = NULL;
    }

    return true;
}

static void advance_iterator(pl_field_cursor_t *iter)
{
    iter->index++;

    if (iter->index >= iter->descriptor->field_count)
    {
        iter->index = 0;
        iter->field_info_index = 0;
        iter->submessage_index = 0;
        iter->required_field_index = 0;
    }
    else
    {
        uint32_t prev_descriptor = iter->descriptor->field_info[iter->field_info_index];
        pl_type_t prev_type = (prev_descriptor >> 8) & 0xFF;
        pl_size_t descriptor_len = (pl_size_t)(1 << (prev_descriptor & 3));

        iter->field_info_index = (pl_size_t)(iter->field_info_index + descriptor_len);
        iter->required_field_index = (pl_size_t)(iter->required_field_index + (PL_CARD(prev_type) == PL_CARD_REQUIRED));
        iter->submessage_index = (pl_size_t)(iter->submessage_index + PL_DTYPE_IS_SUBMSG(prev_type));
    }
}

bool pl_field_cursor_begin(pl_field_cursor_t *iter, const pl_msg_descriptor_t *desc, void *message)
{
    memset(iter, 0, sizeof(*iter));
    iter->descriptor = desc;
    iter->message = message;
    return load_descriptor_values(iter);
}

bool pl_field_cursor_begin_extension(pl_field_cursor_t *iter, pl_extension_t *extension)
{
    const pl_msg_descriptor_t *msg = (const pl_msg_descriptor_t*)extension->type->arg;
    bool status;

    uint32_t word0 = msg->field_info[0];
    if (PL_ALLOC(word0 >> 8) == PL_ALLOC_POINTER)
    {
        status = pl_field_cursor_begin(iter, msg, &extension->dest);
    }
    else
    {
        status = pl_field_cursor_begin(iter, msg, extension->dest);
    }

    iter->pSize = &extension->found;
    return status;
}

bool pl_field_cursor_next(pl_field_cursor_t *iter)
{
    advance_iterator(iter);
    (void)load_descriptor_values(iter);
    return iter->index != 0;
}

bool pl_field_cursor_find(pl_field_cursor_t *iter, uint32_t tag)
{
    if (iter->tag == tag)
    {
        return true;
    }
    else if (tag > iter->descriptor->largest_tag)
    {
        return false;
    }
    else
    {
        pl_size_t start = iter->index;
        uint32_t fieldinfo;

        if (tag < iter->tag)
        {
            iter->index = iter->descriptor->field_count;
        }

        do
        {
            advance_iterator(iter);

            fieldinfo = iter->descriptor->field_info[iter->field_info_index];

            if (((fieldinfo >> 2) & 0x3F) == (tag & 0x3F))
            {
                (void)load_descriptor_values(iter);

                if (iter->tag == tag &&
                    PL_DTYPE(iter->type) != PL_DTYPE_EXTENSION)
                {
                    return true;
                }
            }
        } while (iter->index != start);

        (void)load_descriptor_values(iter);
        return false;
    }
}

bool pl_field_cursor_find_extension(pl_field_cursor_t *iter)
{
    if (PL_DTYPE(iter->type) == PL_DTYPE_EXTENSION)
    {
        return true;
    }
    else
    {
        pl_size_t start = iter->index;
        uint32_t fieldinfo;

        do
        {
            advance_iterator(iter);

            fieldinfo = iter->descriptor->field_info[iter->field_info_index];

            if (PL_DTYPE((fieldinfo >> 8) & 0xFF) == PL_DTYPE_EXTENSION)
            {
                return load_descriptor_values(iter);
            }
        } while (iter->index != start);

        (void)load_descriptor_values(iter);
        return false;
    }
}

static void *pl_const_cast(const void *p)
{
    union {
        void *p1;
        const void *p2;
    } t;
    t.p2 = p;
    return t.p1;
}

bool pl_field_cursor_begin_const(pl_field_cursor_t *iter, const pl_msg_descriptor_t *desc, const void *message)
{
    return pl_field_cursor_begin(iter, desc, pl_const_cast(message));
}

bool pl_field_cursor_begin_extension_const(pl_field_cursor_t *iter, const pl_extension_t *extension)
{
    return pl_field_cursor_begin_extension(iter, (pl_extension_t*)pl_const_cast(extension));
}

bool pl_default_field_callback(pl_istream_t *istream, pl_ostream_t *ostream, const pl_field_t *field)
{
    if (field->data_size == sizeof(pl_callback_t))
    {
        pl_callback_t *pCallback = (pl_callback_t*)field->pData;

        if (pCallback != NULL)
        {
            if (istream != NULL && pCallback->funcs.decode != NULL)
            {
                return pCallback->funcs.decode(istream, field, &pCallback->arg);
            }

            if (ostream != NULL && pCallback->funcs.encode != NULL)
            {
                return pCallback->funcs.encode(ostream, field, &pCallback->arg);
            }
        }
    }

    return true;
}

#ifdef PL_VALIDATE_UTF8
bool pl_validate_utf8(const char *str)
{
    const pl_byte_t *s = (const pl_byte_t*)str;
    while (*s)
    {
        if (*s < 0x80)
        {
            s++;
        }
        else if ((s[0] & 0xe0) == 0xc0)
        {
            if ((s[1] & 0xc0) != 0x80 ||
                (s[0] & 0xfe) == 0xc0)
                return false;
            else
                s += 2;
        }
        else if ((s[0] & 0xf0) == 0xe0)
        {
            if ((s[1] & 0xc0) != 0x80 ||
                (s[2] & 0xc0) != 0x80 ||
                (s[0] == 0xe0 && (s[1] & 0xe0) == 0x80) ||
                (s[0] == 0xed && (s[1] & 0xe0) == 0xa0) ||
                (s[0] == 0xef && s[1] == 0xbf &&
                (s[2] & 0xfe) == 0xbe))
                return false;
            else
                s += 3;
        }
        else if ((s[0] & 0xf8) == 0xf0)
        {
            if ((s[1] & 0xc0) != 0x80 ||
                (s[2] & 0xc0) != 0x80 ||
                (s[3] & 0xc0) != 0x80 ||
                (s[0] == 0xf0 && (s[1] & 0xf0) == 0x80) ||
                (s[0] == 0xf4 && s[1] > 0x8f) || s[0] > 0xf4)
                return false;
            else
                s += 4;
        }
        else
        {
            return false;
        }
    }

    return true;
}
#endif
