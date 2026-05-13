#ifndef PL_ENABLE_MALLOC
#define PL_ENABLE_MALLOC
#endif
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
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
 *  Message types with pointer-allocated fields
 * ============================================================ */

/* Message with a pointer-allocated int32 */
typedef struct {
    int32_t *value;
} PtrIntMsg;

static const uint32_t PtrIntMsg_field_info[] = {
    PL_FIELDINFO_2(1, PL_ALLOC_POINTER | PL_CARD_OPTIONAL | PL_DTYPE_VARINT,
                   offsetof(PtrIntMsg, value), sizeof(int32_t), 0, 1)
    0
};
static const pl_msg_descriptor_t *const PtrIntMsg_submsg_info[] = { NULL };
static const pl_msg_descriptor_t PtrIntMsg_msg = {
    PtrIntMsg_field_info, PtrIntMsg_submsg_info, NULL, NULL, 1, 0, 1
};

/* Message with a pointer-allocated string */
typedef struct {
    char *name;
} PtrStringMsg;

static const uint32_t PtrStringMsg_field_info[] = {
    PL_FIELDINFO_2(1, PL_ALLOC_POINTER | PL_CARD_OPTIONAL | PL_DTYPE_STRING,
                   offsetof(PtrStringMsg, name), sizeof(char*), 0, 1)
    0
};
static const pl_msg_descriptor_t *const PtrStringMsg_submsg_info[] = { NULL };
static const pl_msg_descriptor_t PtrStringMsg_msg = {
    PtrStringMsg_field_info, PtrStringMsg_submsg_info, NULL, NULL, 1, 0, 1
};

/* Message with a pointer-allocated bytes field */
typedef struct {
    pl_bytes_array_t *data;
} PtrBytesMsg;

static const uint32_t PtrBytesMsg_field_info[] = {
    PL_FIELDINFO_2(1, PL_ALLOC_POINTER | PL_CARD_OPTIONAL | PL_DTYPE_BYTES,
                   offsetof(PtrBytesMsg, data), sizeof(pl_bytes_array_t*), 0, 1)
    0
};
static const pl_msg_descriptor_t *const PtrBytesMsg_submsg_info[] = { NULL };
static const pl_msg_descriptor_t PtrBytesMsg_msg = {
    PtrBytesMsg_field_info, PtrBytesMsg_submsg_info, NULL, NULL, 1, 0, 1
};

/* Simple submessage */
typedef struct {
    int32_t x;
    int32_t y;
} PointMsg;

static const uint32_t PointMsg_field_info[] = {
    PL_FIELDINFO_1(1, PL_ALLOC_STATIC | PL_CARD_REQUIRED | PL_DTYPE_VARINT,
                   offsetof(PointMsg, x), sizeof(int32_t), 0, 1)
    PL_FIELDINFO_1(2, PL_ALLOC_STATIC | PL_CARD_REQUIRED | PL_DTYPE_VARINT,
                   offsetof(PointMsg, y), sizeof(int32_t), 0, 1)
    0
};
static const pl_msg_descriptor_t *const PointMsg_submsg_info[] = { NULL };
static const pl_msg_descriptor_t PointMsg_msg = {
    PointMsg_field_info, PointMsg_submsg_info, NULL, NULL, 2, 2, 2
};

/* Message with a pointer-allocated submessage */
typedef struct {
    int32_t id;
    PointMsg *location;
} PtrSubmsgMsg;

static const uint32_t PtrSubmsgMsg_field_info[] = {
    PL_FIELDINFO_1(1, PL_ALLOC_STATIC | PL_CARD_REQUIRED | PL_DTYPE_VARINT,
                   offsetof(PtrSubmsgMsg, id), sizeof(int32_t), 0, 1)
    PL_FIELDINFO_2(2, PL_ALLOC_POINTER | PL_CARD_OPTIONAL | PL_DTYPE_SUBMESSAGE,
                   offsetof(PtrSubmsgMsg, location), sizeof(PointMsg), 0, 1)
    0
};
static const pl_msg_descriptor_t *const PtrSubmsgMsg_submsg_info[] = {
    &PointMsg_msg, NULL
};
static const pl_msg_descriptor_t PtrSubmsgMsg_msg = {
    PtrSubmsgMsg_field_info, PtrSubmsgMsg_submsg_info, NULL, NULL, 2, 1, 2
};

/* Message with pointer-allocated repeated int32 array */
typedef struct {
    pl_size_t values_count;
    int32_t *values;
} PtrRepeatedMsg;

static const uint32_t PtrRepeatedMsg_field_info[] = {
    PL_FIELDINFO_2(1, PL_ALLOC_POINTER | PL_CARD_REPEATED | PL_DTYPE_VARINT,
                   offsetof(PtrRepeatedMsg, values), sizeof(int32_t),
                   pl_delta(PtrRepeatedMsg, values, values_count), 1)
    0
};
static const pl_msg_descriptor_t *const PtrRepeatedMsg_submsg_info[] = { NULL };
static const pl_msg_descriptor_t PtrRepeatedMsg_msg = {
    PtrRepeatedMsg_field_info, PtrRepeatedMsg_submsg_info, NULL, NULL, 1, 0, 1
};

/* Message with pointer-allocated repeated strings */
typedef struct {
    pl_size_t names_count;
    char **names;
} PtrRepeatedStringMsg;

static const uint32_t PtrRepeatedStringMsg_field_info[] = {
    PL_FIELDINFO_2(1, PL_ALLOC_POINTER | PL_CARD_REPEATED | PL_DTYPE_STRING,
                   offsetof(PtrRepeatedStringMsg, names), sizeof(char*),
                   pl_delta(PtrRepeatedStringMsg, names, names_count), 1)
    0
};
static const pl_msg_descriptor_t *const PtrRepeatedStringMsg_submsg_info[] = { NULL };
static const pl_msg_descriptor_t PtrRepeatedStringMsg_msg = {
    PtrRepeatedStringMsg_field_info, PtrRepeatedStringMsg_submsg_info, NULL, NULL, 1, 0, 1
};

/* Message with pointer-allocated repeated fixed32 (packed) */
typedef struct {
    pl_size_t values_count;
    float *values;
} PtrRepeatedFixed32Msg;

static const uint32_t PtrRepeatedFixed32Msg_field_info[] = {
    PL_FIELDINFO_2(1, PL_ALLOC_POINTER | PL_CARD_REPEATED | PL_DTYPE_FIXED32,
                   offsetof(PtrRepeatedFixed32Msg, values), sizeof(float),
                   pl_delta(PtrRepeatedFixed32Msg, values, values_count), 1)
    0
};
static const pl_msg_descriptor_t *const PtrRepeatedFixed32Msg_submsg_info[] = { NULL };
static const pl_msg_descriptor_t PtrRepeatedFixed32Msg_msg = {
    PtrRepeatedFixed32Msg_field_info, PtrRepeatedFixed32Msg_submsg_info, NULL, NULL, 1, 0, 1
};

/* Message with pointer bool */
typedef struct {
    bool *flag;
} PtrBoolMsg;

static const uint32_t PtrBoolMsg_field_info[] = {
    PL_FIELDINFO_2(1, PL_ALLOC_POINTER | PL_CARD_OPTIONAL | PL_DTYPE_BOOL,
                   offsetof(PtrBoolMsg, flag), sizeof(bool), 0, 1)
    0
};
static const pl_msg_descriptor_t *const PtrBoolMsg_submsg_info[] = { NULL };
static const pl_msg_descriptor_t PtrBoolMsg_msg = {
    PtrBoolMsg_field_info, PtrBoolMsg_submsg_info, NULL, NULL, 1, 0, 1
};

/* Message with pointer fixed64 */
typedef struct {
    double *value;
} PtrFixed64Msg;

static const uint32_t PtrFixed64Msg_field_info[] = {
    PL_FIELDINFO_2(1, PL_ALLOC_POINTER | PL_CARD_OPTIONAL | PL_DTYPE_FIXED64,
                   offsetof(PtrFixed64Msg, value), sizeof(double), 0, 1)
    0
};
static const pl_msg_descriptor_t *const PtrFixed64Msg_submsg_info[] = { NULL };
static const pl_msg_descriptor_t PtrFixed64Msg_msg = {
    PtrFixed64Msg_field_info, PtrFixed64Msg_submsg_info, NULL, NULL, 1, 0, 1
};

/* Message with pointer svarint */
typedef struct {
    int32_t *value;
} PtrSvarintMsg;

static const uint32_t PtrSvarintMsg_field_info[] = {
    PL_FIELDINFO_2(1, PL_ALLOC_POINTER | PL_CARD_OPTIONAL | PL_DTYPE_SVARINT,
                   offsetof(PtrSvarintMsg, value), sizeof(int32_t), 0, 1)
    0
};
static const pl_msg_descriptor_t *const PtrSvarintMsg_submsg_info[] = { NULL };
static const pl_msg_descriptor_t PtrSvarintMsg_msg = {
    PtrSvarintMsg_field_info, PtrSvarintMsg_submsg_info, NULL, NULL, 1, 0, 1
};

/* ============================================================
 *  TESTS: Pointer scalar field decode + release
 * ============================================================ */

static void test_ptr_int_decode(void)
{
    uint8_t data[] = {0x08, 0x2A};
    pl_istream_t stream = pl_istream_from_buffer(data, sizeof(data));
    PtrIntMsg msg;
    memset(&msg, 0, sizeof(msg));

    bool ok = pl_decode_message(&stream, &PtrIntMsg_msg, &msg);
    CHECK("ptr int decode ok", ok);
    CHECK("ptr int not null", msg.value != NULL);
    if (msg.value)
        CHECK("ptr int value=42", *msg.value == 42);

    pl_release(&PtrIntMsg_msg, &msg);
    CHECK("ptr int released", msg.value == NULL);
}

static void test_ptr_int_absent(void)
{
    uint8_t data[] = {};
    pl_istream_t stream = pl_istream_from_buffer(data, 0);
    PtrIntMsg msg;
    memset(&msg, 0, sizeof(msg));

    bool ok = pl_decode_message(&stream, &PtrIntMsg_msg, &msg);
    CHECK("ptr int absent ok", ok);
    CHECK("ptr int absent null", msg.value == NULL);

    pl_release(&PtrIntMsg_msg, &msg);
}

static void test_ptr_bool_decode(void)
{
    uint8_t data[] = {0x08, 0x01};
    pl_istream_t stream = pl_istream_from_buffer(data, sizeof(data));
    PtrBoolMsg msg;
    memset(&msg, 0, sizeof(msg));

    bool ok = pl_decode_message(&stream, &PtrBoolMsg_msg, &msg);
    CHECK("ptr bool ok", ok);
    CHECK("ptr bool not null", msg.flag != NULL);
    if (msg.flag)
        CHECK("ptr bool value=true", *msg.flag == true);

    pl_release(&PtrBoolMsg_msg, &msg);
    CHECK("ptr bool released", msg.flag == NULL);
}

static void test_ptr_fixed64_decode(void)
{
    double src_val = 3.14159;
    PtrFixed64Msg src;
    memset(&src, 0, sizeof(src));
    src.value = &src_val;

    uint8_t buf[32];
    pl_ostream_t ostream = pl_ostream_from_buffer(buf, sizeof(buf));
    bool ok = pl_encode_message(&ostream, &PtrFixed64Msg_msg, &src);
    CHECK("ptr f64 encode ok", ok);

    PtrFixed64Msg dst;
    memset(&dst, 0, sizeof(dst));
    pl_istream_t istream = pl_istream_from_buffer(buf, ostream.bytes_written);
    ok = pl_decode_message(&istream, &PtrFixed64Msg_msg, &dst);
    CHECK("ptr f64 decode ok", ok);
    CHECK("ptr f64 not null", dst.value != NULL);
    if (dst.value)
        CHECK("ptr f64 value", *dst.value == 3.14159);

    pl_release(&PtrFixed64Msg_msg, &dst);
}

static void test_ptr_svarint_decode(void)
{
    uint8_t data[] = {0x08, 0x03};
    pl_istream_t stream = pl_istream_from_buffer(data, sizeof(data));
    PtrSvarintMsg msg;
    memset(&msg, 0, sizeof(msg));

    bool ok = pl_decode_message(&stream, &PtrSvarintMsg_msg, &msg);
    CHECK("ptr svarint ok", ok);
    CHECK("ptr svarint not null", msg.value != NULL);
    if (msg.value)
        CHECK("ptr svarint value=-2", *msg.value == -2);

    pl_release(&PtrSvarintMsg_msg, &msg);
}

/* ============================================================
 *  TESTS: Pointer string field decode + release
 * ============================================================ */

static void test_ptr_string_decode(void)
{
    uint8_t data[] = {0x0A, 0x05, 'h', 'e', 'l', 'l', 'o'};
    pl_istream_t stream = pl_istream_from_buffer(data, sizeof(data));
    PtrStringMsg msg;
    memset(&msg, 0, sizeof(msg));

    bool ok = pl_decode_message(&stream, &PtrStringMsg_msg, &msg);
    CHECK("ptr string ok", ok);
    CHECK("ptr string not null", msg.name != NULL);
    if (msg.name)
        CHECK("ptr string value=hello", strcmp(msg.name, "hello") == 0);

    pl_release(&PtrStringMsg_msg, &msg);
    CHECK("ptr string released", msg.name == NULL);
}

static void test_ptr_string_empty(void)
{
    uint8_t data[] = {0x0A, 0x00};
    pl_istream_t stream = pl_istream_from_buffer(data, sizeof(data));
    PtrStringMsg msg;
    memset(&msg, 0, sizeof(msg));

    bool ok = pl_decode_message(&stream, &PtrStringMsg_msg, &msg);
    CHECK("ptr string empty ok", ok);
    CHECK("ptr string empty not null", msg.name != NULL);
    if (msg.name)
        CHECK("ptr string empty value=\"\"", msg.name[0] == '\0');

    pl_release(&PtrStringMsg_msg, &msg);
}

static void test_ptr_string_absent(void)
{
    uint8_t data[] = {};
    pl_istream_t stream = pl_istream_from_buffer(data, 0);
    PtrStringMsg msg;
    memset(&msg, 0, sizeof(msg));

    bool ok = pl_decode_message(&stream, &PtrStringMsg_msg, &msg);
    CHECK("ptr string absent ok", ok);
    CHECK("ptr string absent null", msg.name == NULL);

    pl_release(&PtrStringMsg_msg, &msg);
}

/* ============================================================
 *  TESTS: Pointer bytes field
 * ============================================================ */

static void test_ptr_bytes_decode(void)
{
    uint8_t data[] = {0x0A, 0x03, 0xDE, 0xAD, 0xBE};
    pl_istream_t stream = pl_istream_from_buffer(data, sizeof(data));
    PtrBytesMsg msg;
    memset(&msg, 0, sizeof(msg));

    bool ok = pl_decode_message(&stream, &PtrBytesMsg_msg, &msg);
    CHECK("ptr bytes ok", ok);
    CHECK("ptr bytes not null", msg.data != NULL);
    if (msg.data)
    {
        CHECK("ptr bytes size=3", msg.data->size == 3);
        CHECK("ptr bytes [0]", msg.data->bytes[0] == 0xDE);
        CHECK("ptr bytes [1]", msg.data->bytes[1] == 0xAD);
        CHECK("ptr bytes [2]", msg.data->bytes[2] == 0xBE);
    }

    pl_release(&PtrBytesMsg_msg, &msg);
    CHECK("ptr bytes released", msg.data == NULL);
}

/* ============================================================
 *  TESTS: Pointer submessage field
 * ============================================================ */

static void test_ptr_submsg_decode(void)
{
    uint8_t data[] = {0x08, 0x01, 0x12, 0x04, 0x08, 0x0A, 0x10, 0x14};
    pl_istream_t stream = pl_istream_from_buffer(data, sizeof(data));
    PtrSubmsgMsg msg;
    memset(&msg, 0, sizeof(msg));

    bool ok = pl_decode_message(&stream, &PtrSubmsgMsg_msg, &msg);
    CHECK("ptr submsg ok", ok);
    CHECK("ptr submsg id=1", msg.id == 1);
    CHECK("ptr submsg not null", msg.location != NULL);
    if (msg.location)
    {
        CHECK("ptr submsg x=10", msg.location->x == 10);
        CHECK("ptr submsg y=20", msg.location->y == 20);
    }

    pl_release(&PtrSubmsgMsg_msg, &msg);
    CHECK("ptr submsg released", msg.location == NULL);
}

static void test_ptr_submsg_absent(void)
{
    uint8_t data[] = {0x08, 0x05};
    pl_istream_t stream = pl_istream_from_buffer(data, sizeof(data));
    PtrSubmsgMsg msg;
    memset(&msg, 0, sizeof(msg));

    bool ok = pl_decode_message(&stream, &PtrSubmsgMsg_msg, &msg);
    CHECK("ptr submsg absent ok", ok);
    CHECK("ptr submsg absent null", msg.location == NULL);

    pl_release(&PtrSubmsgMsg_msg, &msg);
}

/* ============================================================
 *  TESTS: Pointer repeated field
 * ============================================================ */

static void test_ptr_repeated_packed(void)
{
    uint8_t data[] = {0x0A, 0x03, 0x01, 0x02, 0x03};
    pl_istream_t stream = pl_istream_from_buffer(data, sizeof(data));
    PtrRepeatedMsg msg;
    memset(&msg, 0, sizeof(msg));

    bool ok = pl_decode_message(&stream, &PtrRepeatedMsg_msg, &msg);
    CHECK("ptr rep packed ok", ok);
    CHECK("ptr rep count=3", msg.values_count == 3);
    CHECK("ptr rep not null", msg.values != NULL);
    if (msg.values)
    {
        CHECK("ptr rep [0]=1", msg.values[0] == 1);
        CHECK("ptr rep [1]=2", msg.values[1] == 2);
        CHECK("ptr rep [2]=3", msg.values[2] == 3);
    }

    pl_release(&PtrRepeatedMsg_msg, &msg);
    CHECK("ptr rep released", msg.values == NULL);
    CHECK("ptr rep count=0", msg.values_count == 0);
}

static void test_ptr_repeated_unpacked(void)
{
    uint8_t data[] = {0x08, 0x0A, 0x08, 0x14, 0x08, 0x1E};
    pl_istream_t stream = pl_istream_from_buffer(data, sizeof(data));
    PtrRepeatedMsg msg;
    memset(&msg, 0, sizeof(msg));

    bool ok = pl_decode_message(&stream, &PtrRepeatedMsg_msg, &msg);
    CHECK("ptr rep unpacked ok", ok);
    CHECK("ptr rep unpacked count=3", msg.values_count == 3);
    if (msg.values)
    {
        CHECK("ptr rep unpacked [0]=10", msg.values[0] == 10);
        CHECK("ptr rep unpacked [1]=20", msg.values[1] == 20);
        CHECK("ptr rep unpacked [2]=30", msg.values[2] == 30);
    }

    pl_release(&PtrRepeatedMsg_msg, &msg);
}

static void test_ptr_repeated_empty(void)
{
    uint8_t data[] = {};
    pl_istream_t stream = pl_istream_from_buffer(data, 0);
    PtrRepeatedMsg msg;
    memset(&msg, 0, sizeof(msg));

    bool ok = pl_decode_message(&stream, &PtrRepeatedMsg_msg, &msg);
    CHECK("ptr rep empty ok", ok);
    CHECK("ptr rep empty count=0", msg.values_count == 0);
    CHECK("ptr rep empty null", msg.values == NULL);

    pl_release(&PtrRepeatedMsg_msg, &msg);
}

static void test_ptr_repeated_fixed32(void)
{
    PtrRepeatedFixed32Msg src;
    memset(&src, 0, sizeof(src));
    float vals[] = {1.0f, 2.0f, 3.0f};
    src.values_count = 3;
    src.values = vals;

    uint8_t buf[64];
    pl_ostream_t ostream = pl_ostream_from_buffer(buf, sizeof(buf));
    bool ok = pl_encode_message(&ostream, &PtrRepeatedFixed32Msg_msg, &src);
    CHECK("ptr rep f32 encode ok", ok);

    PtrRepeatedFixed32Msg dst;
    memset(&dst, 0, sizeof(dst));
    pl_istream_t istream = pl_istream_from_buffer(buf, ostream.bytes_written);
    ok = pl_decode_message(&istream, &PtrRepeatedFixed32Msg_msg, &dst);
    CHECK("ptr rep f32 decode ok", ok);
    CHECK("ptr rep f32 count=3", dst.values_count == 3);
    if (dst.values)
    {
        CHECK("ptr rep f32 [0]=1.0", dst.values[0] == 1.0f);
        CHECK("ptr rep f32 [1]=2.0", dst.values[1] == 2.0f);
        CHECK("ptr rep f32 [2]=3.0", dst.values[2] == 3.0f);
    }

    pl_release(&PtrRepeatedFixed32Msg_msg, &dst);
}

/* ============================================================
 *  TESTS: Pointer repeated strings (each element is char*)
 * ============================================================ */

static void test_ptr_repeated_strings(void)
{
    PtrRepeatedStringMsg src;
    memset(&src, 0, sizeof(src));
    char *strs[] = {"hello", "world"};
    src.names_count = 2;
    src.names = strs;

    uint8_t buf[64];
    pl_ostream_t ostream = pl_ostream_from_buffer(buf, sizeof(buf));
    bool ok = pl_encode_message(&ostream, &PtrRepeatedStringMsg_msg, &src);
    CHECK("ptr rep str encode ok", ok);

    PtrRepeatedStringMsg dst;
    memset(&dst, 0, sizeof(dst));
    pl_istream_t istream = pl_istream_from_buffer(buf, ostream.bytes_written);
    ok = pl_decode_message(&istream, &PtrRepeatedStringMsg_msg, &dst);
    CHECK("ptr rep str decode ok", ok);
    CHECK("ptr rep str count=2", dst.names_count == 2);
    if (dst.names)
    {
        CHECK("ptr rep str [0]=hello", dst.names[0] != NULL && strcmp(dst.names[0], "hello") == 0);
        CHECK("ptr rep str [1]=world", dst.names[1] != NULL && strcmp(dst.names[1], "world") == 0);
    }

    pl_release(&PtrRepeatedStringMsg_msg, &dst);
    CHECK("ptr rep str released", dst.names == NULL);
    CHECK("ptr rep str count=0", dst.names_count == 0);
}

/* ============================================================
 *  TESTS: Pointer encode (encode from pointer fields)
 * ============================================================ */

static void test_ptr_int_encode(void)
{
    int32_t val = 99;
    PtrIntMsg src;
    memset(&src, 0, sizeof(src));
    src.value = &val;

    uint8_t buf[32];
    pl_ostream_t ostream = pl_ostream_from_buffer(buf, sizeof(buf));
    bool ok = pl_encode_message(&ostream, &PtrIntMsg_msg, &src);
    CHECK("ptr int encode ok", ok);
    CHECK("ptr int encode bytes>0", ostream.bytes_written > 0);

    PtrIntMsg dst;
    memset(&dst, 0, sizeof(dst));
    pl_istream_t istream = pl_istream_from_buffer(buf, ostream.bytes_written);
    ok = pl_decode_message(&istream, &PtrIntMsg_msg, &dst);
    CHECK("ptr int rt ok", ok);
    if (dst.value)
        CHECK("ptr int rt value=99", *dst.value == 99);

    pl_release(&PtrIntMsg_msg, &dst);
}

static void test_ptr_int_encode_null(void)
{
    PtrIntMsg src;
    memset(&src, 0, sizeof(src));

    uint8_t buf[32];
    pl_ostream_t ostream = pl_ostream_from_buffer(buf, sizeof(buf));
    bool ok = pl_encode_message(&ostream, &PtrIntMsg_msg, &src);
    CHECK("ptr null encode ok", ok);
    CHECK("ptr null encode 0 bytes", ostream.bytes_written == 0);
}

static void test_ptr_string_encode(void)
{
    PtrStringMsg src;
    memset(&src, 0, sizeof(src));
    src.name = "test123";

    uint8_t buf[32];
    pl_ostream_t ostream = pl_ostream_from_buffer(buf, sizeof(buf));
    bool ok = pl_encode_message(&ostream, &PtrStringMsg_msg, &src);
    CHECK("ptr str encode ok", ok);

    PtrStringMsg dst;
    memset(&dst, 0, sizeof(dst));
    pl_istream_t istream = pl_istream_from_buffer(buf, ostream.bytes_written);
    ok = pl_decode_message(&istream, &PtrStringMsg_msg, &dst);
    CHECK("ptr str rt ok", ok);
    if (dst.name)
        CHECK("ptr str rt value", strcmp(dst.name, "test123") == 0);

    pl_release(&PtrStringMsg_msg, &dst);
}

/* ============================================================
 *  TESTS: Full round-trip with pointer submessage
 * ============================================================ */

static void test_ptr_submsg_roundtrip(void)
{
    PointMsg pt = {50, -75};
    PtrSubmsgMsg src;
    memset(&src, 0, sizeof(src));
    src.id = 42;
    src.location = &pt;

    uint8_t buf[64];
    pl_ostream_t ostream = pl_ostream_from_buffer(buf, sizeof(buf));
    bool ok = pl_encode_message(&ostream, &PtrSubmsgMsg_msg, &src);
    CHECK("ptr submsg rt encode ok", ok);

    PtrSubmsgMsg dst;
    memset(&dst, 0, sizeof(dst));
    pl_istream_t istream = pl_istream_from_buffer(buf, ostream.bytes_written);
    ok = pl_decode_message(&istream, &PtrSubmsgMsg_msg, &dst);
    CHECK("ptr submsg rt decode ok", ok);
    CHECK("ptr submsg rt id=42", dst.id == 42);
    if (dst.location)
    {
        CHECK("ptr submsg rt x=50", dst.location->x == 50);
        CHECK("ptr submsg rt y=-75", dst.location->y == -75);
    }

    pl_release(&PtrSubmsgMsg_msg, &dst);
    CHECK("ptr submsg rt released", dst.location == NULL);
}

/* ============================================================
 *  TESTS: Decode failure releases memory
 * ============================================================ */

static void test_decode_failure_releases(void)
{
    /* Truncated data - string says length 10 but only 3 bytes */
    uint8_t data[] = {0x0A, 0x0A, 'a', 'b', 'c'};
    pl_istream_t stream = pl_istream_from_buffer(data, sizeof(data));
    PtrStringMsg msg;
    memset(&msg, 0, sizeof(msg));

    bool ok = pl_decode_message(&stream, &PtrStringMsg_msg, &msg);
    CHECK("decode fail returns false", !ok);
    CHECK("decode fail name null", msg.name == NULL);
}

/* ============================================================
 *  TESTS: Double release is safe
 * ============================================================ */

static void test_double_release(void)
{
    uint8_t data[] = {0x08, 0x2A};
    pl_istream_t stream = pl_istream_from_buffer(data, sizeof(data));
    PtrIntMsg msg;
    memset(&msg, 0, sizeof(msg));

    pl_decode_message(&stream, &PtrIntMsg_msg, &msg);
    pl_release(&PtrIntMsg_msg, &msg);
    pl_release(&PtrIntMsg_msg, &msg);
    CHECK("double release safe", msg.value == NULL);
}

/* ============================================================
 *  TESTS: Release on NULL struct
 * ============================================================ */

static void test_release_null(void)
{
    pl_release(&PtrIntMsg_msg, NULL);
    CHECK("release null safe", true);
}

int main(void)
{
    printf("=== Dynamic Allocation Tests ===\n");

    /* Pointer scalar decode */
    test_ptr_int_decode();
    test_ptr_int_absent();
    test_ptr_bool_decode();
    test_ptr_fixed64_decode();
    test_ptr_svarint_decode();

    /* Pointer string decode */
    test_ptr_string_decode();
    test_ptr_string_empty();
    test_ptr_string_absent();

    /* Pointer bytes decode */
    test_ptr_bytes_decode();

    /* Pointer submessage decode */
    test_ptr_submsg_decode();
    test_ptr_submsg_absent();

    /* Pointer repeated decode */
    test_ptr_repeated_packed();
    test_ptr_repeated_unpacked();
    test_ptr_repeated_empty();
    test_ptr_repeated_fixed32();
    test_ptr_repeated_strings();

    /* Pointer encode */
    test_ptr_int_encode();
    test_ptr_int_encode_null();
    test_ptr_string_encode();

    /* Round-trips */
    test_ptr_submsg_roundtrip();

    /* Error handling */
    test_decode_failure_releases();
    test_double_release();
    test_release_null();

    printf("\nResults: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
