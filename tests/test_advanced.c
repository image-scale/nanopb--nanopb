#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "../include/protolite.h"
#include "../include/protolite_encode.h"
#include "../include/protolite_decode.h"
#include "../include/protolite_common.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define CHECK(name, expr) do { \
    if (expr) { \
        tests_passed++; \
    } else { \
        tests_failed++; \
        printf("FAIL: %s (line %d)\n", name, __LINE__); \
    } \
} while(0)

/* ============================================================
 *  CALLBACK FIELD TESTS
 * ============================================================ */

/* --- Encode callback: writes a string field --- */
static bool encode_string_cb(pl_ostream_t *stream, const pl_field_t *field, void * const *arg)
{
    const char *str = (const char*)*arg;
    if (!pl_encode_tag_for_field(stream, field))
        return false;
    return pl_encode_string(stream, (const pl_byte_t*)str, strlen(str));
}

/* --- Decode callback: reads a string into arg buffer --- */
static bool decode_string_cb(pl_istream_t *stream, const pl_field_t *field, void **arg)
{
    char *buf = (char*)*arg;
    size_t len = stream->bytes_left;
    if (len > 63) len = 63;
    if (!pl_read(stream, (pl_byte_t*)buf, len))
        return false;
    buf[len] = '\0';
    return true;
}

/* Message with a callback string field (traditional per-field style) */
typedef struct {
    int32_t id;
    pl_callback_t name;
} CbMsg;

static const uint32_t CbMsg_field_info[] = {
    PL_FIELDINFO_1(1, PL_ALLOC_STATIC | PL_CARD_REQUIRED | PL_DTYPE_VARINT,
                   offsetof(CbMsg, id), sizeof(int32_t), 0, 1)
    PL_FIELDINFO_2(2, PL_ALLOC_CALLBACK | PL_CARD_REQUIRED | PL_DTYPE_STRING,
                   offsetof(CbMsg, name), sizeof(pl_callback_t), 0, 1)
    0
};
static const pl_msg_descriptor_t *const CbMsg_submsg_info[] = { NULL };
static const pl_msg_descriptor_t CbMsg_msg = {
    CbMsg_field_info, CbMsg_submsg_info, NULL, pl_default_field_callback, 2, 2, 2
};

static void test_callback_encode_string(void)
{
    CbMsg msg;
    memset(&msg, 0, sizeof(msg));
    msg.id = 42;
    msg.name.funcs.encode = &encode_string_cb;
    msg.name.arg = (void*)"hello";

    uint8_t buf[64];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));
    bool ok = pl_encode_message(&stream, &CbMsg_msg, &msg);
    CHECK("cb encode string ok", ok);
    CHECK("cb encode written>0", stream.bytes_written > 0);

    /* Decode to verify */
    char decoded_name[64] = {0};
    CbMsg dst;
    memset(&dst, 0, sizeof(dst));
    dst.name.funcs.decode = &decode_string_cb;
    dst.name.arg = decoded_name;

    pl_istream_t istream = pl_istream_from_buffer(buf, stream.bytes_written);
    ok = pl_decode_message(&istream, &CbMsg_msg, &dst);
    CHECK("cb decode string ok", ok);
    CHECK("cb decode id=42", dst.id == 42);
    CHECK("cb decode name=hello", strcmp(decoded_name, "hello") == 0);
}

/* --- Encode callback: writes a varint --- */
static bool encode_varint_cb(pl_ostream_t *stream, const pl_field_t *field, void * const *arg)
{
    int32_t val = *(const int32_t*)*arg;
    if (!pl_encode_tag_for_field(stream, field))
        return false;
    return pl_encode_varint(stream, (uint64_t)(int64_t)val);
}

/* --- Decode callback: reads a varint --- */
static bool decode_varint_cb(pl_istream_t *stream, const pl_field_t *field, void **arg)
{
    uint64_t val;
    if (!pl_decode_varint(stream, &val))
        return false;
    *(int32_t*)*arg = (int32_t)val;
    return true;
}

/* Message with a callback varint field */
typedef struct {
    pl_callback_t value;
} CbVarintMsg;

static const uint32_t CbVarintMsg_field_info[] = {
    PL_FIELDINFO_2(1, PL_ALLOC_CALLBACK | PL_CARD_REQUIRED | PL_DTYPE_VARINT,
                   offsetof(CbVarintMsg, value), sizeof(pl_callback_t), 0, 1)
    0
};
static const pl_msg_descriptor_t *const CbVarintMsg_submsg_info[] = { NULL };
static const pl_msg_descriptor_t CbVarintMsg_msg = {
    CbVarintMsg_field_info, CbVarintMsg_submsg_info, NULL, pl_default_field_callback, 1, 1, 1
};

static void test_callback_encode_varint(void)
{
    int32_t val = 999;
    CbVarintMsg msg;
    memset(&msg, 0, sizeof(msg));
    msg.value.funcs.encode = &encode_varint_cb;
    msg.value.arg = &val;

    uint8_t buf[32];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));
    bool ok = pl_encode_message(&stream, &CbVarintMsg_msg, &msg);
    CHECK("cb varint encode ok", ok);

    int32_t decoded_val = 0;
    CbVarintMsg dst;
    memset(&dst, 0, sizeof(dst));
    dst.value.funcs.decode = &decode_varint_cb;
    dst.value.arg = &decoded_val;

    pl_istream_t istream = pl_istream_from_buffer(buf, stream.bytes_written);
    ok = pl_decode_message(&istream, &CbVarintMsg_msg, &dst);
    CHECK("cb varint decode ok", ok);
    CHECK("cb varint value=999", decoded_val == 999);
}

/* --- Encode callback: writes repeated varints --- */
typedef struct {
    int count;
    int32_t values[8];
} RepCbData;

static bool encode_repeated_cb(pl_ostream_t *stream, const pl_field_t *field, void * const *arg)
{
    const RepCbData *data = (const RepCbData*)*arg;
    for (int i = 0; i < data->count; i++)
    {
        if (!pl_encode_tag_for_field(stream, field))
            return false;
        if (!pl_encode_varint(stream, (uint64_t)(int64_t)data->values[i]))
            return false;
    }
    return true;
}

/* --- Decode callback: appends decoded varints --- */
static bool decode_repeated_cb(pl_istream_t *stream, const pl_field_t *field, void **arg)
{
    RepCbData *data = (RepCbData*)*arg;
    uint64_t val;
    if (!pl_decode_varint(stream, &val))
        return false;
    if (data->count < 8)
        data->values[data->count++] = (int32_t)val;
    return true;
}

typedef struct {
    pl_callback_t items;
} CbRepeatedMsg;

static const uint32_t CbRepeatedMsg_field_info[] = {
    PL_FIELDINFO_2(1, PL_ALLOC_CALLBACK | PL_CARD_REQUIRED | PL_DTYPE_VARINT,
                   offsetof(CbRepeatedMsg, items), sizeof(pl_callback_t), 0, 1)
    0
};
static const pl_msg_descriptor_t *const CbRepeatedMsg_submsg_info[] = { NULL };
static const pl_msg_descriptor_t CbRepeatedMsg_msg = {
    CbRepeatedMsg_field_info, CbRepeatedMsg_submsg_info, NULL, pl_default_field_callback, 1, 1, 1
};

static void test_callback_repeated(void)
{
    RepCbData src = {3, {10, 20, 30}};
    CbRepeatedMsg msg;
    memset(&msg, 0, sizeof(msg));
    msg.items.funcs.encode = &encode_repeated_cb;
    msg.items.arg = &src;

    uint8_t buf[64];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));
    bool ok = pl_encode_message(&stream, &CbRepeatedMsg_msg, &msg);
    CHECK("cb repeated encode ok", ok);

    RepCbData dst = {0, {0}};
    CbRepeatedMsg dmsg;
    memset(&dmsg, 0, sizeof(dmsg));
    dmsg.items.funcs.decode = &decode_repeated_cb;
    dmsg.items.arg = &dst;

    pl_istream_t istream = pl_istream_from_buffer(buf, stream.bytes_written);
    ok = pl_decode_message(&istream, &CbRepeatedMsg_msg, &dmsg);
    CHECK("cb repeated decode ok", ok);
    CHECK("cb repeated count=3", dst.count == 3);
    CHECK("cb repeated [0]=10", dst.values[0] == 10);
    CHECK("cb repeated [1]=20", dst.values[1] == 20);
    CHECK("cb repeated [2]=30", dst.values[2] == 30);
}

/* --- Direct message-level callback (not pl_default_field_callback) --- */
static int direct_cb_calls = 0;
static bool direct_encode_cb(pl_istream_t *istream, pl_ostream_t *ostream, const pl_field_cursor_t *field)
{
    (void)istream;
    if (ostream != NULL)
    {
        direct_cb_calls++;
        int32_t val = 77;
        if (!pl_encode_tag_for_field(ostream, field))
            return false;
        return pl_encode_varint(ostream, (uint64_t)(int64_t)val);
    }
    return true;
}

typedef struct {
    pl_callback_t dummy;
} DirectCbMsg;

static const uint32_t DirectCbMsg_field_info[] = {
    PL_FIELDINFO_2(5, PL_ALLOC_CALLBACK | PL_CARD_REQUIRED | PL_DTYPE_VARINT,
                   offsetof(DirectCbMsg, dummy), sizeof(pl_callback_t), 0, 1)
    0
};
static const pl_msg_descriptor_t *const DirectCbMsg_submsg_info[] = { NULL };
static const pl_msg_descriptor_t DirectCbMsg_msg = {
    DirectCbMsg_field_info, DirectCbMsg_submsg_info, NULL, direct_encode_cb, 1, 1, 5
};

static void test_direct_callback_encode(void)
{
    DirectCbMsg msg;
    memset(&msg, 0, sizeof(msg));
    direct_cb_calls = 0;

    uint8_t buf[32];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));
    bool ok = pl_encode_message(&stream, &DirectCbMsg_msg, &msg);
    CHECK("direct cb encode ok", ok);
    CHECK("direct cb called once", direct_cb_calls == 1);
    CHECK("direct cb wrote bytes", stream.bytes_written > 0);
}

/* --- Callback with NULL function pointer (skip gracefully) --- */
static void test_callback_null_func(void)
{
    CbVarintMsg msg;
    memset(&msg, 0, sizeof(msg));

    uint8_t buf[32];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));
    bool ok = pl_encode_message(&stream, &CbVarintMsg_msg, &msg);
    CHECK("cb null encode ok", ok);
    CHECK("cb null encode empty", stream.bytes_written == 0);
}

/* ============================================================
 *  EXTENSION FIELD TESTS
 * ============================================================ */

/* Base message with an extension field slot */
typedef struct {
    int32_t id;
    pl_extension_t *extensions;
} ExtendableMsg;

static const uint32_t ExtendableMsg_field_info[] = {
    PL_FIELDINFO_1(1, PL_ALLOC_STATIC | PL_CARD_REQUIRED | PL_DTYPE_VARINT,
                   offsetof(ExtendableMsg, id), sizeof(int32_t), 0, 1)
    PL_FIELDINFO_1(2, PL_ALLOC_STATIC | PL_CARD_OPTIONAL | PL_DTYPE_EXTENSION,
                   offsetof(ExtendableMsg, extensions), sizeof(pl_extension_t*), 0, 1)
    0
};
static const pl_msg_descriptor_t *const ExtendableMsg_submsg_info[] = { NULL };
static const pl_msg_descriptor_t ExtendableMsg_msg = {
    ExtendableMsg_field_info, ExtendableMsg_submsg_info, NULL, NULL, 2, 1, 2
};

/* Extension type descriptor for extension field: int32 at tag 100 */
static const uint32_t ext_int32_field_info[] = {
    PL_FIELDINFO_2(100, PL_ALLOC_STATIC | PL_CARD_REQUIRED | PL_DTYPE_VARINT,
                   0, sizeof(int32_t), 0, 1)
    0
};
static const pl_msg_descriptor_t *const ext_int32_submsg_info[] = { NULL };
static const pl_msg_descriptor_t ext_int32_msg = {
    ext_int32_field_info, ext_int32_submsg_info, NULL, NULL, 1, 1, 100
};
static const pl_extension_type_t ext_int32_type = {
    NULL, NULL, &ext_int32_msg
};

static void test_extension_encode_decode(void)
{
    int32_t ext_value = 12345;
    pl_extension_t ext = pl_extension_init_zero;
    ext.type = &ext_int32_type;
    ext.dest = &ext_value;

    ExtendableMsg src;
    memset(&src, 0, sizeof(src));
    src.id = 7;
    src.extensions = &ext;

    uint8_t buf[64];
    pl_ostream_t ostream = pl_ostream_from_buffer(buf, sizeof(buf));
    bool ok = pl_encode_message(&ostream, &ExtendableMsg_msg, &src);
    CHECK("ext encode ok", ok);
    CHECK("ext encode bytes>0", ostream.bytes_written > 0);

    /* Decode */
    int32_t dec_ext_value = 0;
    pl_extension_t dec_ext = pl_extension_init_zero;
    dec_ext.type = &ext_int32_type;
    dec_ext.dest = &dec_ext_value;

    ExtendableMsg dst;
    memset(&dst, 0, sizeof(dst));
    dst.extensions = &dec_ext;

    pl_istream_t istream = pl_istream_from_buffer(buf, ostream.bytes_written);
    ok = pl_decode_message(&istream, &ExtendableMsg_msg, &dst);
    CHECK("ext decode ok", ok);
    CHECK("ext decode id=7", dst.id == 7);
    CHECK("ext decode found", dec_ext.found == true);
    CHECK("ext decode value=12345", dec_ext_value == 12345);
}

static void test_extension_absent(void)
{
    ExtendableMsg src;
    memset(&src, 0, sizeof(src));
    src.id = 3;
    src.extensions = NULL;

    uint8_t buf[32];
    pl_ostream_t ostream = pl_ostream_from_buffer(buf, sizeof(buf));
    bool ok = pl_encode_message(&ostream, &ExtendableMsg_msg, &src);
    CHECK("ext absent encode ok", ok);

    int32_t dec_ext_value = 0;
    pl_extension_t dec_ext = pl_extension_init_zero;
    dec_ext.type = &ext_int32_type;
    dec_ext.dest = &dec_ext_value;

    ExtendableMsg dst;
    memset(&dst, 0, sizeof(dst));
    dst.extensions = &dec_ext;

    pl_istream_t istream = pl_istream_from_buffer(buf, ostream.bytes_written);
    ok = pl_decode_message(&istream, &ExtendableMsg_msg, &dst);
    CHECK("ext absent decode ok", ok);
    CHECK("ext absent not found", dec_ext.found == false);
    CHECK("ext absent value=0", dec_ext_value == 0);
}

/* Extension for string at tag 101 */
typedef char ext_str_t[32];

static const uint32_t ext_str_field_info[] = {
    PL_FIELDINFO_2(101, PL_ALLOC_STATIC | PL_CARD_REQUIRED | PL_DTYPE_STRING,
                   0, sizeof(ext_str_t), 0, 1)
    0
};
static const pl_msg_descriptor_t *const ext_str_submsg_info[] = { NULL };
static const pl_msg_descriptor_t ext_str_msg = {
    ext_str_field_info, ext_str_submsg_info, NULL, NULL, 1, 1, 101
};
static const pl_extension_type_t ext_str_type = {
    NULL, NULL, &ext_str_msg
};

static void test_extension_chained(void)
{
    int32_t ext1_val = 555;
    pl_extension_t ext1 = pl_extension_init_zero;
    ext1.type = &ext_int32_type;
    ext1.dest = &ext1_val;

    ext_str_t ext2_val;
    strcpy(ext2_val, "ext_test");
    pl_extension_t ext2 = pl_extension_init_zero;
    ext2.type = &ext_str_type;
    ext2.dest = ext2_val;

    ext1.next = &ext2;

    ExtendableMsg src;
    memset(&src, 0, sizeof(src));
    src.id = 1;
    src.extensions = &ext1;

    uint8_t buf[128];
    pl_ostream_t ostream = pl_ostream_from_buffer(buf, sizeof(buf));
    bool ok = pl_encode_message(&ostream, &ExtendableMsg_msg, &src);
    CHECK("ext chain encode ok", ok);

    /* Decode with chained extensions */
    int32_t d_ext1 = 0;
    ext_str_t d_ext2;
    memset(d_ext2, 0, sizeof(d_ext2));

    pl_extension_t d1 = pl_extension_init_zero;
    d1.type = &ext_int32_type;
    d1.dest = &d_ext1;

    pl_extension_t d2 = pl_extension_init_zero;
    d2.type = &ext_str_type;
    d2.dest = d_ext2;

    d1.next = &d2;

    ExtendableMsg dst;
    memset(&dst, 0, sizeof(dst));
    dst.extensions = &d1;

    pl_istream_t istream = pl_istream_from_buffer(buf, ostream.bytes_written);
    ok = pl_decode_message(&istream, &ExtendableMsg_msg, &dst);
    CHECK("ext chain decode ok", ok);
    CHECK("ext chain id=1", dst.id == 1);
    CHECK("ext chain ext1 found", d1.found == true);
    CHECK("ext chain ext1=555", d_ext1 == 555);
    CHECK("ext chain ext2 found", d2.found == true);
    CHECK("ext chain ext2=ext_test", strcmp(d_ext2, "ext_test") == 0);
}

/* --- Custom extension encoder/decoder --- */
static bool custom_ext_encode(pl_ostream_t *stream, const pl_extension_t *extension)
{
    int32_t val = *(const int32_t*)extension->dest;
    if (!pl_encode_tag(stream, PL_WT_VARINT, 200))
        return false;
    return pl_encode_varint(stream, (uint64_t)(int64_t)val);
}

static bool custom_ext_decode(pl_istream_t *stream, pl_extension_t *extension,
                               uint32_t tag, pl_wire_type_t wire_type)
{
    if (tag == 200 && wire_type == PL_WT_VARINT)
    {
        uint64_t val;
        if (!pl_decode_varint(stream, &val))
            return false;
        *(int32_t*)extension->dest = (int32_t)val;
        extension->found = true;
    }
    return true;
}

static const pl_extension_type_t custom_ext_type = {
    custom_ext_decode, custom_ext_encode, NULL
};

static void test_extension_custom(void)
{
    int32_t ext_val = 9999;
    pl_extension_t ext = pl_extension_init_zero;
    ext.type = &custom_ext_type;
    ext.dest = &ext_val;

    ExtendableMsg src;
    memset(&src, 0, sizeof(src));
    src.id = 5;
    src.extensions = &ext;

    uint8_t buf[64];
    pl_ostream_t ostream = pl_ostream_from_buffer(buf, sizeof(buf));
    bool ok = pl_encode_message(&ostream, &ExtendableMsg_msg, &src);
    CHECK("custom ext encode ok", ok);

    int32_t d_val = 0;
    pl_extension_t d_ext = pl_extension_init_zero;
    d_ext.type = &custom_ext_type;
    d_ext.dest = &d_val;

    ExtendableMsg dst;
    memset(&dst, 0, sizeof(dst));
    dst.extensions = &d_ext;

    pl_istream_t istream = pl_istream_from_buffer(buf, ostream.bytes_written);
    ok = pl_decode_message(&istream, &ExtendableMsg_msg, &dst);
    CHECK("custom ext decode ok", ok);
    CHECK("custom ext found", d_ext.found == true);
    CHECK("custom ext val=9999", d_val == 9999);
}

/* --- Extension field that is not matched (skipped) --- */
static void test_extension_unmatched_skipped(void)
{
    /* Encode with extension at tag 100 */
    int32_t ext_val = 42;
    pl_extension_t ext = pl_extension_init_zero;
    ext.type = &ext_int32_type;
    ext.dest = &ext_val;

    ExtendableMsg src;
    memset(&src, 0, sizeof(src));
    src.id = 1;
    src.extensions = &ext;

    uint8_t buf[64];
    pl_ostream_t ostream = pl_ostream_from_buffer(buf, sizeof(buf));
    pl_encode_message(&ostream, &ExtendableMsg_msg, &src);

    /* Decode with a different extension type that looks for tag 101 */
    ext_str_t d_str;
    memset(d_str, 0, sizeof(d_str));
    pl_extension_t d_ext = pl_extension_init_zero;
    d_ext.type = &ext_str_type;
    d_ext.dest = d_str;

    ExtendableMsg dst;
    memset(&dst, 0, sizeof(dst));
    dst.extensions = &d_ext;

    pl_istream_t istream = pl_istream_from_buffer(buf, ostream.bytes_written);
    bool ok = pl_decode_message(&istream, &ExtendableMsg_msg, &dst);
    CHECK("ext unmatched decode ok", ok);
    CHECK("ext unmatched not found", d_ext.found == false);
    CHECK("ext unmatched id=1", dst.id == 1);
}

/* ============================================================
 *  ENCODE SIZE CALCULATION TESTS
 * ============================================================ */

typedef struct {
    int32_t id;
    bool has_name;
    char name[16];
} SizeMsg;

static const uint32_t SizeMsg_field_info[] = {
    PL_FIELDINFO_1(1, PL_ALLOC_STATIC | PL_CARD_REQUIRED | PL_DTYPE_VARINT,
                   offsetof(SizeMsg, id), sizeof(int32_t), 0, 1)
    PL_FIELDINFO_2(2, PL_ALLOC_STATIC | PL_CARD_OPTIONAL | PL_DTYPE_STRING,
                   offsetof(SizeMsg, name), sizeof(((SizeMsg*)0)->name),
                   pl_delta(SizeMsg, name, has_name), 1)
    0
};
static const pl_msg_descriptor_t *const SizeMsg_submsg_info[] = { NULL };
static const pl_msg_descriptor_t SizeMsg_msg = {
    SizeMsg_field_info, SizeMsg_submsg_info, NULL, NULL, 2, 1, 2
};

static void test_encode_size_matches(void)
{
    SizeMsg msg;
    memset(&msg, 0, sizeof(msg));
    msg.id = 300;
    msg.has_name = true;
    strcpy(msg.name, "size_test");

    size_t size;
    bool ok = pl_get_encoded_size(&size, &SizeMsg_msg, &msg);
    CHECK("enc size calc ok", ok);

    uint8_t buf[64];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));
    ok = pl_encode_message(&stream, &SizeMsg_msg, &msg);
    CHECK("enc size encode ok", ok);
    CHECK("enc size matches", size == stream.bytes_written);
}

static void test_encode_size_empty_optional(void)
{
    SizeMsg msg;
    memset(&msg, 0, sizeof(msg));
    msg.id = 1;
    msg.has_name = false;

    size_t size;
    pl_get_encoded_size(&size, &SizeMsg_msg, &msg);

    uint8_t buf[32];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));
    pl_encode_message(&stream, &SizeMsg_msg, &msg);
    CHECK("enc size no opt matches", size == stream.bytes_written);
}

/* ============================================================
 *  DELIMITED AND NULL-TERMINATED MODE TESTS (encode + decode)
 * ============================================================ */

typedef struct {
    int32_t value;
} SimpleMsg;

static const uint32_t SimpleMsg_field_info[] = {
    PL_FIELDINFO_1(1, PL_ALLOC_STATIC | PL_CARD_REQUIRED | PL_DTYPE_VARINT,
                   offsetof(SimpleMsg, value), sizeof(int32_t), 0, 1)
    0
};
static const pl_msg_descriptor_t *const SimpleMsg_submsg_info[] = { NULL };
static const pl_msg_descriptor_t SimpleMsg_msg = {
    SimpleMsg_field_info, SimpleMsg_submsg_info, NULL, NULL, 1, 1, 1
};

static void test_delimited_roundtrip(void)
{
    SimpleMsg src = {300};
    uint8_t buf[32];
    pl_ostream_t ostream = pl_ostream_from_buffer(buf, sizeof(buf));
    bool ok = pl_encode_message_ex(&ostream, &SimpleMsg_msg, &src, PL_ENCODE_DELIMITED);
    CHECK("delim encode ok", ok);

    SimpleMsg dst;
    pl_istream_t istream = pl_istream_from_buffer(buf, ostream.bytes_written);
    ok = pl_decode_message_ex(&istream, &SimpleMsg_msg, &dst, PL_DECODE_DELIMITED);
    CHECK("delim decode ok", ok);
    CHECK("delim value=300", dst.value == 300);
}

static void test_nullterm_roundtrip(void)
{
    SimpleMsg src = {42};
    uint8_t buf[32];
    pl_ostream_t ostream = pl_ostream_from_buffer(buf, sizeof(buf));
    bool ok = pl_encode_message_ex(&ostream, &SimpleMsg_msg, &src, PL_ENCODE_NULLTERMINATED);
    CHECK("nullterm encode ok", ok);

    SimpleMsg dst;
    pl_istream_t istream = pl_istream_from_buffer(buf, ostream.bytes_written);
    ok = pl_decode_message_ex(&istream, &SimpleMsg_msg, &dst, PL_DECODE_NULLTERMINATED);
    CHECK("nullterm decode ok", ok);
    CHECK("nullterm value=42", dst.value == 42);
}

static void test_delimited_multiple_messages(void)
{
    uint8_t buf[64];
    pl_ostream_t ostream = pl_ostream_from_buffer(buf, sizeof(buf));

    SimpleMsg m1 = {10}, m2 = {20};
    pl_encode_message_ex(&ostream, &SimpleMsg_msg, &m1, PL_ENCODE_DELIMITED);
    pl_encode_message_ex(&ostream, &SimpleMsg_msg, &m2, PL_ENCODE_DELIMITED);

    pl_istream_t istream = pl_istream_from_buffer(buf, ostream.bytes_written);

    SimpleMsg d1, d2;
    bool ok1 = pl_decode_message_ex(&istream, &SimpleMsg_msg, &d1, PL_DECODE_DELIMITED);
    bool ok2 = pl_decode_message_ex(&istream, &SimpleMsg_msg, &d2, PL_DECODE_DELIMITED);
    CHECK("multi delim ok1", ok1);
    CHECK("multi delim ok2", ok2);
    CHECK("multi delim d1=10", d1.value == 10);
    CHECK("multi delim d2=20", d2.value == 20);
}

/* ============================================================
 *  PL_BIND MACRO TESTS WITH CALLBACKS
 * ============================================================ */

typedef struct {
    int32_t id;
    pl_callback_t data;
} BindCbMsg;

#define BindCbMsg_FIELDLIST(X, a) \
    X(a, STATIC, REQUIRED, INT32, id, 1) \
    X(a, CALLBACK, REQUIRED, STRING, data, 2)

#define BindCbMsg_DEFAULT NULL
#define BindCbMsg_CALLBACK pl_default_field_callback

PL_BIND(BindCbMsg, BindCbMsg, AUTO)

static void test_bind_callback(void)
{
    BindCbMsg msg;
    memset(&msg, 0, sizeof(msg));
    msg.id = 50;
    msg.data.funcs.encode = &encode_string_cb;
    msg.data.arg = (void*)"bind_cb";

    uint8_t buf[64];
    pl_ostream_t ostream = pl_ostream_from_buffer(buf, sizeof(buf));
    bool ok = pl_encode_message(&ostream, &BindCbMsg_msg, &msg);
    CHECK("bind cb encode ok", ok);

    char decoded[64] = {0};
    BindCbMsg dst;
    memset(&dst, 0, sizeof(dst));
    dst.data.funcs.decode = &decode_string_cb;
    dst.data.arg = decoded;

    pl_istream_t istream = pl_istream_from_buffer(buf, ostream.bytes_written);
    ok = pl_decode_message(&istream, &BindCbMsg_msg, &dst);
    CHECK("bind cb decode ok", ok);
    CHECK("bind cb id=50", dst.id == 50);
    CHECK("bind cb data=bind_cb", strcmp(decoded, "bind_cb") == 0);
}

/* ============================================================
 *  ENCODE SIZE WITH EXTENSIONS
 * ============================================================ */

static void test_encode_size_with_extension(void)
{
    int32_t ext_val = 42;
    pl_extension_t ext = pl_extension_init_zero;
    ext.type = &ext_int32_type;
    ext.dest = &ext_val;

    ExtendableMsg msg;
    memset(&msg, 0, sizeof(msg));
    msg.id = 1;
    msg.extensions = &ext;

    size_t size;
    bool ok = pl_get_encoded_size(&size, &ExtendableMsg_msg, &msg);
    CHECK("ext size calc ok", ok);

    uint8_t buf[64];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));
    ok = pl_encode_message(&stream, &ExtendableMsg_msg, &msg);
    CHECK("ext size encode ok", ok);
    CHECK("ext size matches", size == stream.bytes_written);
}

/* ============================================================
 *  ENCODE SIZE WITH CALLBACKS
 * ============================================================ */

static void test_encode_size_with_callback(void)
{
    CbMsg msg;
    memset(&msg, 0, sizeof(msg));
    msg.id = 1;
    msg.name.funcs.encode = &encode_string_cb;
    msg.name.arg = (void*)"hi";

    size_t size;
    bool ok = pl_get_encoded_size(&size, &CbMsg_msg, &msg);
    CHECK("cb size calc ok", ok);

    uint8_t buf[32];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));
    ok = pl_encode_message(&stream, &CbMsg_msg, &msg);
    CHECK("cb size encode ok", ok);
    CHECK("cb size matches", size == stream.bytes_written);
}

/* ============================================================
 *  EXTENSION ROUND-TRIP WITH SUBMESSAGE EXTENSION
 * ============================================================ */

typedef struct {
    int32_t x;
    int32_t y;
} ExtPointMsg;

static const uint32_t ExtPointMsg_field_info[] = {
    PL_FIELDINFO_1(1, PL_ALLOC_STATIC | PL_CARD_REQUIRED | PL_DTYPE_VARINT,
                   offsetof(ExtPointMsg, x), sizeof(int32_t), 0, 1)
    PL_FIELDINFO_1(2, PL_ALLOC_STATIC | PL_CARD_REQUIRED | PL_DTYPE_VARINT,
                   offsetof(ExtPointMsg, y), sizeof(int32_t), 0, 1)
    0
};
static const pl_msg_descriptor_t *const ExtPointMsg_submsg_info[] = { NULL };
static const pl_msg_descriptor_t ExtPointMsg_msg = {
    ExtPointMsg_field_info, ExtPointMsg_submsg_info, NULL, NULL, 2, 2, 2
};

static const uint32_t ext_submsg_field_info[] = {
    PL_FIELDINFO_2(102, PL_ALLOC_STATIC | PL_CARD_REQUIRED | PL_DTYPE_SUBMESSAGE,
                   0, sizeof(ExtPointMsg), 0, 1)
    0
};
static const pl_msg_descriptor_t *const ext_submsg_submsg_info[] = {
    &ExtPointMsg_msg, NULL
};
static const pl_msg_descriptor_t ext_submsg_msg = {
    ext_submsg_field_info, ext_submsg_submsg_info, NULL, NULL, 1, 1, 102
};
static const pl_extension_type_t ext_submsg_type = {
    NULL, NULL, &ext_submsg_msg
};

static void test_extension_submessage(void)
{
    ExtPointMsg ext_val = {100, 200};
    pl_extension_t ext = pl_extension_init_zero;
    ext.type = &ext_submsg_type;
    ext.dest = &ext_val;

    ExtendableMsg src;
    memset(&src, 0, sizeof(src));
    src.id = 9;
    src.extensions = &ext;

    uint8_t buf[128];
    pl_ostream_t ostream = pl_ostream_from_buffer(buf, sizeof(buf));
    bool ok = pl_encode_message(&ostream, &ExtendableMsg_msg, &src);
    CHECK("ext submsg encode ok", ok);

    ExtPointMsg d_ext_val;
    memset(&d_ext_val, 0, sizeof(d_ext_val));
    pl_extension_t d_ext = pl_extension_init_zero;
    d_ext.type = &ext_submsg_type;
    d_ext.dest = &d_ext_val;

    ExtendableMsg dst;
    memset(&dst, 0, sizeof(dst));
    dst.extensions = &d_ext;

    pl_istream_t istream = pl_istream_from_buffer(buf, ostream.bytes_written);
    ok = pl_decode_message(&istream, &ExtendableMsg_msg, &dst);
    CHECK("ext submsg decode ok", ok);
    CHECK("ext submsg id=9", dst.id == 9);
    CHECK("ext submsg found", d_ext.found == true);
    CHECK("ext submsg x=100", d_ext_val.x == 100);
    CHECK("ext submsg y=200", d_ext_val.y == 200);
}

/* ============================================================
 *  CALLBACK ENCODE FOR FIXED32 (WIRE TYPE 32-BIT)
 * ============================================================ */

static bool encode_fixed32_cb(pl_ostream_t *stream, const pl_field_t *field, void * const *arg)
{
    float val = *(const float*)*arg;
    if (!pl_encode_tag_for_field(stream, field))
        return false;
    return pl_encode_fixed32(stream, &val);
}

static bool decode_fixed32_cb(pl_istream_t *stream, const pl_field_t *field, void **arg)
{
    uint32_t raw;
    if (!pl_decode_fixed32(stream, &raw))
        return false;
    memcpy(*arg, &raw, sizeof(float));
    return true;
}

typedef struct {
    pl_callback_t temp;
} CbFixed32Msg;

static const uint32_t CbFixed32Msg_field_info[] = {
    PL_FIELDINFO_2(1, PL_ALLOC_CALLBACK | PL_CARD_REQUIRED | PL_DTYPE_FIXED32,
                   offsetof(CbFixed32Msg, temp), sizeof(pl_callback_t), 0, 1)
    0
};
static const pl_msg_descriptor_t *const CbFixed32Msg_submsg_info[] = { NULL };
static const pl_msg_descriptor_t CbFixed32Msg_msg = {
    CbFixed32Msg_field_info, CbFixed32Msg_submsg_info, NULL, pl_default_field_callback, 1, 1, 1
};

static void test_callback_fixed32(void)
{
    float val = 3.14f;
    CbFixed32Msg msg;
    memset(&msg, 0, sizeof(msg));
    msg.temp.funcs.encode = &encode_fixed32_cb;
    msg.temp.arg = &val;

    uint8_t buf[32];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));
    bool ok = pl_encode_message(&stream, &CbFixed32Msg_msg, &msg);
    CHECK("cb fixed32 encode ok", ok);

    float d_val = 0;
    CbFixed32Msg dst;
    memset(&dst, 0, sizeof(dst));
    dst.temp.funcs.decode = &decode_fixed32_cb;
    dst.temp.arg = &d_val;

    pl_istream_t istream = pl_istream_from_buffer(buf, stream.bytes_written);
    ok = pl_decode_message(&istream, &CbFixed32Msg_msg, &dst);
    CHECK("cb fixed32 decode ok", ok);
    CHECK("cb fixed32 val=3.14", d_val == 3.14f);
}

/* ============================================================
 *  CALLBACK ENCODE FOR FIXED64 (WIRE TYPE 64-BIT)
 * ============================================================ */

static bool encode_fixed64_cb(pl_ostream_t *stream, const pl_field_t *field, void * const *arg)
{
    double val = *(const double*)*arg;
    if (!pl_encode_tag_for_field(stream, field))
        return false;
    return pl_encode_fixed64(stream, &val);
}

static bool decode_fixed64_cb(pl_istream_t *stream, const pl_field_t *field, void **arg)
{
    uint64_t raw;
    if (!pl_decode_fixed64(stream, &raw))
        return false;
    memcpy(*arg, &raw, sizeof(double));
    return true;
}

typedef struct {
    pl_callback_t val;
} CbFixed64Msg;

static const uint32_t CbFixed64Msg_field_info[] = {
    PL_FIELDINFO_2(1, PL_ALLOC_CALLBACK | PL_CARD_REQUIRED | PL_DTYPE_FIXED64,
                   offsetof(CbFixed64Msg, val), sizeof(pl_callback_t), 0, 1)
    0
};
static const pl_msg_descriptor_t *const CbFixed64Msg_submsg_info[] = { NULL };
static const pl_msg_descriptor_t CbFixed64Msg_msg = {
    CbFixed64Msg_field_info, CbFixed64Msg_submsg_info, NULL, pl_default_field_callback, 1, 1, 1
};

static void test_callback_fixed64(void)
{
    double val = 2.718281828;
    CbFixed64Msg msg;
    memset(&msg, 0, sizeof(msg));
    msg.val.funcs.encode = &encode_fixed64_cb;
    msg.val.arg = &val;

    uint8_t buf[32];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));
    bool ok = pl_encode_message(&stream, &CbFixed64Msg_msg, &msg);
    CHECK("cb fixed64 encode ok", ok);

    double d_val = 0;
    CbFixed64Msg dst;
    memset(&dst, 0, sizeof(dst));
    dst.val.funcs.decode = &decode_fixed64_cb;
    dst.val.arg = &d_val;

    pl_istream_t istream = pl_istream_from_buffer(buf, stream.bytes_written);
    ok = pl_decode_message(&istream, &CbFixed64Msg_msg, &dst);
    CHECK("cb fixed64 decode ok", ok);
    CHECK("cb fixed64 val=e", d_val == 2.718281828);
}

int main(void)
{
    printf("=== Advanced Tests (Callbacks, Extensions, Modes) ===\n");

    /* Callback tests */
    test_callback_encode_string();
    test_callback_encode_varint();
    test_callback_repeated();
    test_direct_callback_encode();
    test_callback_null_func();
    test_callback_fixed32();
    test_callback_fixed64();

    /* Extension tests */
    test_extension_encode_decode();
    test_extension_absent();
    test_extension_chained();
    test_extension_custom();
    test_extension_unmatched_skipped();
    test_extension_submessage();

    /* Encode size tests */
    test_encode_size_matches();
    test_encode_size_empty_optional();
    test_encode_size_with_extension();
    test_encode_size_with_callback();

    /* Delimited/null-terminated mode tests */
    test_delimited_roundtrip();
    test_nullterm_roundtrip();
    test_delimited_multiple_messages();

    /* PL_BIND with callback */
    test_bind_callback();

    printf("\nResults: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
