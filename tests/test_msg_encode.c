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

#define CHECK_BUF(name, buf, size, expected, expected_size) do { \
    if ((size) == (expected_size) && memcmp(buf, expected, expected_size) == 0) { \
        tests_passed++; \
    } else { \
        tests_failed++; \
        printf("FAIL: %s (line %d) got %zu bytes:", name, __LINE__, (size_t)(size)); \
        for (size_t _i = 0; _i < (size_t)(size); _i++) printf(" %02x", (buf)[_i]); \
        printf(" expected %zu bytes:", (size_t)(expected_size)); \
        for (size_t _i = 0; _i < (size_t)(expected_size); _i++) printf(" %02x", ((const uint8_t*)(expected))[_i]); \
        printf("\n"); \
    } \
} while(0)

/* === Message 1: Simple required int32 === */
typedef struct {
    int32_t id;
} SimpleMsg;

static const uint32_t SimpleMsg_field_info[] = {
    PL_FIELDINFO_1(1, PL_ALLOC_STATIC | PL_CARD_REQUIRED | PL_DTYPE_VARINT,
                   offsetof(SimpleMsg, id), sizeof(int32_t), 0, 1)
    0
};
static const pl_msg_descriptor_t *const SimpleMsg_submsg_info[] = { NULL };
static const pl_msg_descriptor_t SimpleMsg_msg = {
    SimpleMsg_field_info, SimpleMsg_submsg_info, NULL, NULL, 1, 1, 1
};

static void test_simple_int32(void)
{
    SimpleMsg msg = {42};
    uint8_t buf[64];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));

    bool ok = pl_encode_message(&stream, &SimpleMsg_msg, &msg);
    CHECK("simple int32 encodes", ok);
    /* field 1, wire type 0 (varint): tag = (1<<3)|0 = 0x08, value = 42 = 0x2A */
    uint8_t expected[] = {0x08, 0x2A};
    CHECK_BUF("simple int32 bytes", buf, stream.bytes_written, expected, sizeof(expected));
}

static void test_simple_int32_large(void)
{
    SimpleMsg msg = {300};
    uint8_t buf[64];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));

    bool ok = pl_encode_message(&stream, &SimpleMsg_msg, &msg);
    CHECK("large int32 encodes", ok);
    /* 300 = 0x12C => varint 0xAC 0x02; tag=0x08 */
    uint8_t expected[] = {0x08, 0xAC, 0x02};
    CHECK_BUF("large int32 bytes", buf, stream.bytes_written, expected, sizeof(expected));
}

static void test_simple_int32_negative(void)
{
    SimpleMsg msg = {-1};
    uint8_t buf[64];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));

    bool ok = pl_encode_message(&stream, &SimpleMsg_msg, &msg);
    CHECK("negative int32 encodes", ok);
    /* -1 as int32 varint: 10 bytes of 0xFF except last byte 0x01 */
    uint8_t expected[] = {0x08, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01};
    CHECK_BUF("negative int32 bytes", buf, stream.bytes_written, expected, sizeof(expected));
}

/* === Message 2: Optional field with has_xxx === */
typedef struct {
    bool has_value;
    int32_t value;
} OptionalMsg;

static const uint32_t OptionalMsg_field_info[] = {
    PL_FIELDINFO_1(1, PL_ALLOC_STATIC | PL_CARD_OPTIONAL | PL_DTYPE_VARINT,
                   offsetof(OptionalMsg, value), sizeof(int32_t),
                   pl_delta(OptionalMsg, value, has_value), 1)
    0
};
static const pl_msg_descriptor_t *const OptionalMsg_submsg_info[] = { NULL };
static const pl_msg_descriptor_t OptionalMsg_msg = {
    OptionalMsg_field_info, OptionalMsg_submsg_info, NULL, NULL, 1, 0, 1
};

static void test_optional_present(void)
{
    OptionalMsg msg = {true, 7};
    uint8_t buf[64];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));

    bool ok = pl_encode_message(&stream, &OptionalMsg_msg, &msg);
    CHECK("optional present encodes", ok);
    uint8_t expected[] = {0x08, 0x07};
    CHECK_BUF("optional present bytes", buf, stream.bytes_written, expected, sizeof(expected));
}

static void test_optional_absent(void)
{
    OptionalMsg msg = {false, 7};
    uint8_t buf[64];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));

    bool ok = pl_encode_message(&stream, &OptionalMsg_msg, &msg);
    CHECK("optional absent encodes", ok);
    CHECK("optional absent zero bytes", stream.bytes_written == 0);
}

/* === Message 3: String field === */
typedef struct {
    int32_t id;
    char name[32];
} PersonMsg;

static const uint32_t PersonMsg_field_info[] = {
    PL_FIELDINFO_1(1, PL_ALLOC_STATIC | PL_CARD_REQUIRED | PL_DTYPE_VARINT,
                   offsetof(PersonMsg, id), sizeof(int32_t), 0, 1)
    PL_FIELDINFO_2(2, PL_ALLOC_STATIC | PL_CARD_REQUIRED | PL_DTYPE_STRING,
                   offsetof(PersonMsg, name), sizeof(((PersonMsg*)0)->name), 0, 1)
    0
};
static const pl_msg_descriptor_t *const PersonMsg_submsg_info[] = { NULL };
static const pl_msg_descriptor_t PersonMsg_msg = {
    PersonMsg_field_info, PersonMsg_submsg_info, NULL, NULL, 2, 2, 2
};

static void test_string_field(void)
{
    PersonMsg msg;
    memset(&msg, 0, sizeof(msg));
    msg.id = 1;
    strcpy(msg.name, "Alice");

    uint8_t buf[64];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));

    bool ok = pl_encode_message(&stream, &PersonMsg_msg, &msg);
    CHECK("string field encodes", ok);
    /* field 1: tag=0x08, val=0x01
     * field 2: tag=(2<<3|2)=0x12, len=5, "Alice" */
    uint8_t expected[] = {0x08, 0x01, 0x12, 0x05, 'A', 'l', 'i', 'c', 'e'};
    CHECK_BUF("string field bytes", buf, stream.bytes_written, expected, sizeof(expected));
}

static void test_empty_string(void)
{
    PersonMsg msg;
    memset(&msg, 0, sizeof(msg));
    msg.id = 1;
    msg.name[0] = '\0';

    uint8_t buf[64];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));

    bool ok = pl_encode_message(&stream, &PersonMsg_msg, &msg);
    CHECK("empty string encodes", ok);
    /* field 1: tag=0x08, val=0x01
     * field 2: tag=0x12, len=0 */
    uint8_t expected[] = {0x08, 0x01, 0x12, 0x00};
    CHECK_BUF("empty string bytes", buf, stream.bytes_written, expected, sizeof(expected));
}

/* === Message 4: Repeated packed int32 === */
typedef struct {
    pl_size_t values_count;
    int32_t values[8];
} RepeatedMsg;

static const uint32_t RepeatedMsg_field_info[] = {
    PL_FIELDINFO_2(1, PL_ALLOC_STATIC | PL_CARD_REPEATED | PL_DTYPE_VARINT,
                   offsetof(RepeatedMsg, values), sizeof(int32_t),
                   pl_delta(RepeatedMsg, values, values_count), 8)
    0
};
static const pl_msg_descriptor_t *const RepeatedMsg_submsg_info[] = { NULL };
static const pl_msg_descriptor_t RepeatedMsg_msg = {
    RepeatedMsg_field_info, RepeatedMsg_submsg_info, NULL, NULL, 1, 0, 1
};

static void test_repeated_packed(void)
{
    RepeatedMsg msg;
    memset(&msg, 0, sizeof(msg));
    msg.values_count = 3;
    msg.values[0] = 1;
    msg.values[1] = 2;
    msg.values[2] = 3;

    uint8_t buf[64];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));

    bool ok = pl_encode_message(&stream, &RepeatedMsg_msg, &msg);
    CHECK("repeated packed encodes", ok);
    /* packed: tag=(1<<3|2)=0x0A, len=3, values: 0x01 0x02 0x03 */
    uint8_t expected[] = {0x0A, 0x03, 0x01, 0x02, 0x03};
    CHECK_BUF("repeated packed bytes", buf, stream.bytes_written, expected, sizeof(expected));
}

static void test_repeated_empty(void)
{
    RepeatedMsg msg;
    memset(&msg, 0, sizeof(msg));
    msg.values_count = 0;

    uint8_t buf[64];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));

    bool ok = pl_encode_message(&stream, &RepeatedMsg_msg, &msg);
    CHECK("repeated empty encodes", ok);
    CHECK("repeated empty zero bytes", stream.bytes_written == 0);
}

/* === Message 5: Nested submessage === */
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

typedef struct {
    int32_t id;
    PointMsg location;
} PlaceMsg;

static const uint32_t PlaceMsg_field_info[] = {
    PL_FIELDINFO_1(1, PL_ALLOC_STATIC | PL_CARD_REQUIRED | PL_DTYPE_VARINT,
                   offsetof(PlaceMsg, id), sizeof(int32_t), 0, 1)
    PL_FIELDINFO_2(2, PL_ALLOC_STATIC | PL_CARD_REQUIRED | PL_DTYPE_SUBMESSAGE,
                   offsetof(PlaceMsg, location), sizeof(PointMsg), 0, 1)
    0
};
static const pl_msg_descriptor_t *const PlaceMsg_submsg_info[] = {
    &PointMsg_msg, NULL
};
static const pl_msg_descriptor_t PlaceMsg_msg = {
    PlaceMsg_field_info, PlaceMsg_submsg_info, NULL, NULL, 2, 2, 2
};

static void test_submessage(void)
{
    PlaceMsg msg;
    memset(&msg, 0, sizeof(msg));
    msg.id = 1;
    msg.location.x = 10;
    msg.location.y = 20;

    uint8_t buf[64];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));

    bool ok = pl_encode_message(&stream, &PlaceMsg_msg, &msg);
    CHECK("submessage encodes", ok);
    /* field 1: tag=0x08, val=1
     * field 2: tag=(2<<3|2)=0x12, len=4
     *   sub field 1: tag=0x08, val=10=0x0A
     *   sub field 2: tag=0x10, val=20=0x14 */
    uint8_t expected[] = {0x08, 0x01, 0x12, 0x04, 0x08, 0x0A, 0x10, 0x14};
    CHECK_BUF("submessage bytes", buf, stream.bytes_written, expected, sizeof(expected));
}

/* === Message 6: Bool field === */
typedef struct {
    bool active;
    int32_t count;
} BoolMsg;

static const uint32_t BoolMsg_field_info[] = {
    PL_FIELDINFO_1(1, PL_ALLOC_STATIC | PL_CARD_REQUIRED | PL_DTYPE_BOOL,
                   offsetof(BoolMsg, active), sizeof(bool), 0, 1)
    PL_FIELDINFO_1(2, PL_ALLOC_STATIC | PL_CARD_REQUIRED | PL_DTYPE_VARINT,
                   offsetof(BoolMsg, count), sizeof(int32_t), 0, 1)
    0
};
static const pl_msg_descriptor_t *const BoolMsg_submsg_info[] = { NULL };
static const pl_msg_descriptor_t BoolMsg_msg = {
    BoolMsg_field_info, BoolMsg_submsg_info, NULL, NULL, 2, 2, 2
};

static void test_bool_field(void)
{
    BoolMsg msg = {true, 5};
    uint8_t buf[64];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));

    bool ok = pl_encode_message(&stream, &BoolMsg_msg, &msg);
    CHECK("bool true encodes", ok);
    uint8_t expected[] = {0x08, 0x01, 0x10, 0x05};
    CHECK_BUF("bool true bytes", buf, stream.bytes_written, expected, sizeof(expected));
}

static void test_bool_false(void)
{
    BoolMsg msg = {false, 5};
    uint8_t buf[64];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));

    bool ok = pl_encode_message(&stream, &BoolMsg_msg, &msg);
    CHECK("bool false encodes", ok);
    uint8_t expected[] = {0x08, 0x00, 0x10, 0x05};
    CHECK_BUF("bool false bytes", buf, stream.bytes_written, expected, sizeof(expected));
}

/* === Message 7: Fixed32 (float) field === */
typedef struct {
    float temperature;
} FloatMsg;

static const uint32_t FloatMsg_field_info[] = {
    PL_FIELDINFO_1(1, PL_ALLOC_STATIC | PL_CARD_REQUIRED | PL_DTYPE_FIXED32,
                   offsetof(FloatMsg, temperature), sizeof(float), 0, 1)
    0
};
static const pl_msg_descriptor_t *const FloatMsg_submsg_info[] = { NULL };
static const pl_msg_descriptor_t FloatMsg_msg = {
    FloatMsg_field_info, FloatMsg_submsg_info, NULL, NULL, 1, 1, 1
};

static void test_fixed32_field(void)
{
    FloatMsg msg = {3.14f};
    uint8_t buf[64];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));

    bool ok = pl_encode_message(&stream, &FloatMsg_msg, &msg);
    CHECK("fixed32 encodes", ok);
    CHECK("fixed32 size", stream.bytes_written == 5);
    /* tag = (1<<3)|5 = 0x0D */
    CHECK("fixed32 tag", buf[0] == 0x0D);
    /* Verify round-trip by reading back */
    uint32_t raw;
    memcpy(&raw, &buf[1], 4);
    float decoded;
    memcpy(&decoded, &raw, sizeof(float));
    CHECK("fixed32 value round-trip", decoded == 3.14f);
}

/* === Message 8: Signed varint (sint32) field === */
typedef struct {
    int32_t value;
} SvarintMsg;

static const uint32_t SvarintMsg_field_info[] = {
    PL_FIELDINFO_1(1, PL_ALLOC_STATIC | PL_CARD_REQUIRED | PL_DTYPE_SVARINT,
                   offsetof(SvarintMsg, value), sizeof(int32_t), 0, 1)
    0
};
static const pl_msg_descriptor_t *const SvarintMsg_submsg_info[] = { NULL };
static const pl_msg_descriptor_t SvarintMsg_msg = {
    SvarintMsg_field_info, SvarintMsg_submsg_info, NULL, NULL, 1, 1, 1
};

static void test_svarint_field(void)
{
    SvarintMsg msg = {-1};
    uint8_t buf[64];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));

    bool ok = pl_encode_message(&stream, &SvarintMsg_msg, &msg);
    CHECK("svarint encodes", ok);
    /* zigzag(-1) = 1 => tag=0x08, val=0x01 */
    uint8_t expected[] = {0x08, 0x01};
    CHECK_BUF("svarint -1 bytes", buf, stream.bytes_written, expected, sizeof(expected));
}

static void test_svarint_positive(void)
{
    SvarintMsg msg = {1};
    uint8_t buf[64];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));

    bool ok = pl_encode_message(&stream, &SvarintMsg_msg, &msg);
    CHECK("svarint positive encodes", ok);
    /* zigzag(1) = 2 => tag=0x08, val=0x02 */
    uint8_t expected[] = {0x08, 0x02};
    CHECK_BUF("svarint +1 bytes", buf, stream.bytes_written, expected, sizeof(expected));
}

static void test_svarint_neg2(void)
{
    SvarintMsg msg = {-2};
    uint8_t buf[64];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));

    bool ok = pl_encode_message(&stream, &SvarintMsg_msg, &msg);
    CHECK("svarint -2 encodes", ok);
    /* zigzag(-2) = 3 => tag=0x08, val=0x03 */
    uint8_t expected[] = {0x08, 0x03};
    CHECK_BUF("svarint -2 bytes", buf, stream.bytes_written, expected, sizeof(expected));
}

/* === Message 9: Bytes field === */
typedef struct {
    PL_BYTES_ARRAY_T(16) data;
} BytesMsg;

static const uint32_t BytesMsg_field_info[] = {
    PL_FIELDINFO_2(1, PL_ALLOC_STATIC | PL_CARD_REQUIRED | PL_DTYPE_BYTES,
                   offsetof(BytesMsg, data), sizeof(((BytesMsg*)0)->data), 0, 1)
    0
};
static const pl_msg_descriptor_t *const BytesMsg_submsg_info[] = { NULL };
static const pl_msg_descriptor_t BytesMsg_msg = {
    BytesMsg_field_info, BytesMsg_submsg_info, NULL, NULL, 1, 1, 1
};

static void test_bytes_field(void)
{
    BytesMsg msg;
    memset(&msg, 0, sizeof(msg));
    msg.data.size = 3;
    msg.data.bytes[0] = 0xDE;
    msg.data.bytes[1] = 0xAD;
    msg.data.bytes[2] = 0xBE;

    uint8_t buf[64];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));

    bool ok = pl_encode_message(&stream, &BytesMsg_msg, &msg);
    CHECK("bytes field encodes", ok);
    /* tag=(1<<3|2)=0x0A, len=3, data */
    uint8_t expected[] = {0x0A, 0x03, 0xDE, 0xAD, 0xBE};
    CHECK_BUF("bytes field bytes", buf, stream.bytes_written, expected, sizeof(expected));
}

/* === Message 10: Proto3 singular (no has_field, skip defaults) === */
typedef struct {
    int32_t value;
    char name[16];
} Proto3Msg;

static const uint32_t Proto3Msg_field_info[] = {
    PL_FIELDINFO_1(1, PL_ALLOC_STATIC | PL_CARD_OPTIONAL | PL_DTYPE_VARINT,
                   offsetof(Proto3Msg, value), sizeof(int32_t), 0, 1)
    PL_FIELDINFO_2(2, PL_ALLOC_STATIC | PL_CARD_OPTIONAL | PL_DTYPE_STRING,
                   offsetof(Proto3Msg, name), sizeof(((Proto3Msg*)0)->name), 0, 1)
    0
};
static const pl_msg_descriptor_t *const Proto3Msg_submsg_info[] = { NULL };
static const pl_msg_descriptor_t Proto3Msg_msg = {
    Proto3Msg_field_info, Proto3Msg_submsg_info, NULL, NULL, 2, 0, 2
};

static void test_proto3_defaults_skipped(void)
{
    Proto3Msg msg;
    memset(&msg, 0, sizeof(msg));

    uint8_t buf[64];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));

    bool ok = pl_encode_message(&stream, &Proto3Msg_msg, &msg);
    CHECK("proto3 zero encodes", ok);
    CHECK("proto3 zero produces empty", stream.bytes_written == 0);
}

static void test_proto3_nonzero_encodes(void)
{
    Proto3Msg msg;
    memset(&msg, 0, sizeof(msg));
    msg.value = 42;
    strcpy(msg.name, "hi");

    uint8_t buf[64];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));

    bool ok = pl_encode_message(&stream, &Proto3Msg_msg, &msg);
    CHECK("proto3 nonzero encodes", ok);
    uint8_t expected[] = {0x08, 0x2A, 0x12, 0x02, 'h', 'i'};
    CHECK_BUF("proto3 nonzero bytes", buf, stream.bytes_written, expected, sizeof(expected));
}

static void test_proto3_partial(void)
{
    Proto3Msg msg;
    memset(&msg, 0, sizeof(msg));
    msg.value = 0;
    strcpy(msg.name, "hi");

    uint8_t buf[64];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));

    bool ok = pl_encode_message(&stream, &Proto3Msg_msg, &msg);
    CHECK("proto3 partial encodes", ok);
    /* Only name should encode, value=0 is skipped */
    uint8_t expected[] = {0x12, 0x02, 'h', 'i'};
    CHECK_BUF("proto3 partial bytes", buf, stream.bytes_written, expected, sizeof(expected));
}

/* === Message 11: Empty message === */
static const uint32_t EmptyMsg_field_info[] = { 0 };
static const pl_msg_descriptor_t *const EmptyMsg_submsg_info[] = { NULL };
static const pl_msg_descriptor_t EmptyMsg_msg = {
    EmptyMsg_field_info, EmptyMsg_submsg_info, NULL, NULL, 0, 0, 0
};

static void test_empty_message(void)
{
    int dummy;
    uint8_t buf[64];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));

    bool ok = pl_encode_message(&stream, &EmptyMsg_msg, &dummy);
    CHECK("empty msg encodes", ok);
    CHECK("empty msg zero bytes", stream.bytes_written == 0);
}

/* === Message 12: Oneof field === */
typedef struct {
    pl_size_t which_value;
    union {
        int32_t int_val;
        float float_val;
    } value;
} OneofMsg;

static const uint32_t OneofMsg_field_info[] = {
    PL_FIELDINFO_1(1, PL_ALLOC_STATIC | PL_CARD_ONEOF | PL_DTYPE_VARINT,
                   offsetof(OneofMsg, value.int_val), sizeof(int32_t),
                   pl_delta(OneofMsg, value.int_val, which_value), 1)
    PL_FIELDINFO_1(2, PL_ALLOC_STATIC | PL_CARD_ONEOF | PL_DTYPE_FIXED32,
                   offsetof(OneofMsg, value.float_val), sizeof(float),
                   pl_delta(OneofMsg, value.float_val, which_value), 1)
    0
};
static const pl_msg_descriptor_t *const OneofMsg_submsg_info[] = { NULL };
static const pl_msg_descriptor_t OneofMsg_msg = {
    OneofMsg_field_info, OneofMsg_submsg_info, NULL, NULL, 2, 0, 2
};

static void test_oneof_int(void)
{
    OneofMsg msg;
    memset(&msg, 0, sizeof(msg));
    msg.which_value = 1;
    msg.value.int_val = 99;

    uint8_t buf[64];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));

    bool ok = pl_encode_message(&stream, &OneofMsg_msg, &msg);
    CHECK("oneof int encodes", ok);
    uint8_t expected[] = {0x08, 0x63};
    CHECK_BUF("oneof int bytes", buf, stream.bytes_written, expected, sizeof(expected));
}

static void test_oneof_float(void)
{
    OneofMsg msg;
    memset(&msg, 0, sizeof(msg));
    msg.which_value = 2;
    msg.value.float_val = 1.0f;

    uint8_t buf[64];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));

    bool ok = pl_encode_message(&stream, &OneofMsg_msg, &msg);
    CHECK("oneof float encodes", ok);
    CHECK("oneof float size", stream.bytes_written == 5);
    /* tag = (2<<3)|5 = 0x15 */
    CHECK("oneof float tag", buf[0] == 0x15);
    float decoded;
    memcpy(&decoded, &buf[1], 4);
    CHECK("oneof float value", decoded == 1.0f);
}

static void test_oneof_none(void)
{
    OneofMsg msg;
    memset(&msg, 0, sizeof(msg));
    msg.which_value = 0;

    uint8_t buf[64];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));

    bool ok = pl_encode_message(&stream, &OneofMsg_msg, &msg);
    CHECK("oneof none encodes", ok);
    CHECK("oneof none zero bytes", stream.bytes_written == 0);
}

/* === Test pl_get_encoded_size === */
static void test_get_encoded_size(void)
{
    SimpleMsg msg = {42};
    size_t size;
    bool ok = pl_get_encoded_size(&size, &SimpleMsg_msg, &msg);
    CHECK("get_encoded_size ok", ok);
    CHECK("get_encoded_size == 2", size == 2);

    PlaceMsg place;
    memset(&place, 0, sizeof(place));
    place.id = 1;
    place.location.x = 10;
    place.location.y = 20;

    ok = pl_get_encoded_size(&size, &PlaceMsg_msg, &place);
    CHECK("get_encoded_size submsg ok", ok);
    CHECK("get_encoded_size submsg == 8", size == 8);
}

static void test_get_encoded_size_matches_actual(void)
{
    PersonMsg msg;
    memset(&msg, 0, sizeof(msg));
    msg.id = 123;
    strcpy(msg.name, "Bob");

    size_t predicted;
    bool ok = pl_get_encoded_size(&predicted, &PersonMsg_msg, &msg);
    CHECK("predicted size ok", ok);

    uint8_t buf[128];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));
    ok = pl_encode_message(&stream, &PersonMsg_msg, &msg);
    CHECK("actual encode ok", ok);

    CHECK("predicted == actual", predicted == stream.bytes_written);
}

/* === Test pl_encode_message_ex DELIMITED === */
static void test_encode_delimited(void)
{
    SimpleMsg msg = {42};
    uint8_t buf[64];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));

    bool ok = pl_encode_message_ex(&stream, &SimpleMsg_msg, &msg, PL_ENCODE_DELIMITED);
    CHECK("delimited encodes", ok);
    /* length prefix (2 bytes msg) + msg data */
    uint8_t expected[] = {0x02, 0x08, 0x2A};
    CHECK_BUF("delimited bytes", buf, stream.bytes_written, expected, sizeof(expected));
}

/* === Test pl_encode_message_ex NULLTERMINATED === */
static void test_encode_nullterminated(void)
{
    SimpleMsg msg = {42};
    uint8_t buf[64];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));

    bool ok = pl_encode_message_ex(&stream, &SimpleMsg_msg, &msg, PL_ENCODE_NULLTERMINATED);
    CHECK("nullterminated encodes", ok);
    /* msg data + 0x00 */
    uint8_t expected[] = {0x08, 0x2A, 0x00};
    CHECK_BUF("nullterminated bytes", buf, stream.bytes_written, expected, sizeof(expected));
}

/* === Test buffer overflow === */
static void test_buffer_overflow(void)
{
    SimpleMsg msg = {42};
    uint8_t buf[1];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));

    bool ok = pl_encode_message(&stream, &SimpleMsg_msg, &msg);
    CHECK("overflow returns false", !ok);
}

/* === Test sizing stream (NULL callback) === */
static void test_sizing_stream(void)
{
    SimpleMsg msg = {42};
    pl_ostream_t stream = PL_OSTREAM_SIZING;

    bool ok = pl_encode_message(&stream, &SimpleMsg_msg, &msg);
    CHECK("sizing stream ok", ok);
    CHECK("sizing stream bytes == 2", stream.bytes_written == 2);
}

/* === Test multiple fields: required + optional + repeated === */
typedef struct {
    int32_t id;
    bool has_label;
    char label[16];
    pl_size_t tags_count;
    int32_t tags[4];
} MultiMsg;

static const uint32_t MultiMsg_field_info[] = {
    PL_FIELDINFO_1(1, PL_ALLOC_STATIC | PL_CARD_REQUIRED | PL_DTYPE_VARINT,
                   offsetof(MultiMsg, id), sizeof(int32_t), 0, 1)
    PL_FIELDINFO_2(2, PL_ALLOC_STATIC | PL_CARD_OPTIONAL | PL_DTYPE_STRING,
                   offsetof(MultiMsg, label), sizeof(((MultiMsg*)0)->label),
                   pl_delta(MultiMsg, label, has_label), 1)
    PL_FIELDINFO_2(3, PL_ALLOC_STATIC | PL_CARD_REPEATED | PL_DTYPE_VARINT,
                   offsetof(MultiMsg, tags), sizeof(int32_t),
                   pl_delta(MultiMsg, tags, tags_count), 4)
    0
};
static const pl_msg_descriptor_t *const MultiMsg_submsg_info[] = { NULL };
static const pl_msg_descriptor_t MultiMsg_msg = {
    MultiMsg_field_info, MultiMsg_submsg_info, NULL, NULL, 3, 1, 3
};

static void test_multi_field(void)
{
    MultiMsg msg;
    memset(&msg, 0, sizeof(msg));
    msg.id = 10;
    msg.has_label = true;
    strcpy(msg.label, "ab");
    msg.tags_count = 2;
    msg.tags[0] = 100;
    msg.tags[1] = 200;

    uint8_t buf[64];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));

    bool ok = pl_encode_message(&stream, &MultiMsg_msg, &msg);
    CHECK("multi field encodes", ok);

    /* field 1: tag=0x08, val=0x0A (10)
     * field 2: tag=0x12, len=2, "ab"
     * field 3: packed tag=0x1A, len=3, val=100(0x64) val=200(0xC8 0x01) */
    uint8_t expected[] = {0x08, 0x0A, 0x12, 0x02, 'a', 'b', 0x1A, 0x03, 0x64, 0xC8, 0x01};
    CHECK_BUF("multi field bytes", buf, stream.bytes_written, expected, sizeof(expected));
}

static void test_multi_field_optional_absent(void)
{
    MultiMsg msg;
    memset(&msg, 0, sizeof(msg));
    msg.id = 10;
    msg.has_label = false;
    msg.tags_count = 0;

    uint8_t buf[64];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));

    bool ok = pl_encode_message(&stream, &MultiMsg_msg, &msg);
    CHECK("multi field opt absent encodes", ok);
    /* Only field 1 */
    uint8_t expected[] = {0x08, 0x0A};
    CHECK_BUF("multi field opt absent bytes", buf, stream.bytes_written, expected, sizeof(expected));
}

/* === Test repeated strings (unpacked) === */
typedef struct {
    pl_size_t names_count;
    char names[3][16];
} RepeatedStringMsg;

static const uint32_t RepeatedStringMsg_field_info[] = {
    PL_FIELDINFO_2(1, PL_ALLOC_STATIC | PL_CARD_REPEATED | PL_DTYPE_STRING,
                   offsetof(RepeatedStringMsg, names), 16,
                   pl_delta(RepeatedStringMsg, names, names_count), 3)
    0
};
static const pl_msg_descriptor_t *const RepeatedStringMsg_submsg_info[] = { NULL };
static const pl_msg_descriptor_t RepeatedStringMsg_msg = {
    RepeatedStringMsg_field_info, RepeatedStringMsg_submsg_info, NULL, NULL, 1, 0, 1
};

static void test_repeated_strings(void)
{
    RepeatedStringMsg msg;
    memset(&msg, 0, sizeof(msg));
    msg.names_count = 2;
    strcpy(msg.names[0], "hi");
    strcpy(msg.names[1], "lo");

    uint8_t buf[64];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));

    bool ok = pl_encode_message(&stream, &RepeatedStringMsg_msg, &msg);
    CHECK("repeated strings encode", ok);
    /* Each string gets its own tag:
     * tag=0x0A, len=2, "hi"
     * tag=0x0A, len=2, "lo" */
    uint8_t expected[] = {0x0A, 0x02, 'h', 'i', 0x0A, 0x02, 'l', 'o'};
    CHECK_BUF("repeated strings bytes", buf, stream.bytes_written, expected, sizeof(expected));
}

/* === Test unsigned varint (uint32) === */
typedef struct {
    uint32_t value;
} UvarintMsg;

static const uint32_t UvarintMsg_field_info[] = {
    PL_FIELDINFO_1(1, PL_ALLOC_STATIC | PL_CARD_REQUIRED | PL_DTYPE_UVARINT,
                   offsetof(UvarintMsg, value), sizeof(uint32_t), 0, 1)
    0
};
static const pl_msg_descriptor_t *const UvarintMsg_submsg_info[] = { NULL };
static const pl_msg_descriptor_t UvarintMsg_msg = {
    UvarintMsg_field_info, UvarintMsg_submsg_info, NULL, NULL, 1, 1, 1
};

static void test_uvarint_field(void)
{
    UvarintMsg msg = {300};
    uint8_t buf[64];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));

    bool ok = pl_encode_message(&stream, &UvarintMsg_msg, &msg);
    CHECK("uvarint encodes", ok);
    uint8_t expected[] = {0x08, 0xAC, 0x02};
    CHECK_BUF("uvarint bytes", buf, stream.bytes_written, expected, sizeof(expected));
}

/* === Test fixed-length bytes === */
typedef struct {
    uint8_t hash[4];
} FixedBytesMsg;

static const uint32_t FixedBytesMsg_field_info[] = {
    PL_FIELDINFO_2(1, PL_ALLOC_STATIC | PL_CARD_REQUIRED | PL_DTYPE_FIXED_LENGTH_BYTES,
                   offsetof(FixedBytesMsg, hash), sizeof(((FixedBytesMsg*)0)->hash), 0, 1)
    0
};
static const pl_msg_descriptor_t *const FixedBytesMsg_submsg_info[] = { NULL };
static const pl_msg_descriptor_t FixedBytesMsg_msg = {
    FixedBytesMsg_field_info, FixedBytesMsg_submsg_info, NULL, NULL, 1, 1, 1
};

static void test_fixed_length_bytes(void)
{
    FixedBytesMsg msg = {{0x01, 0x02, 0x03, 0x04}};
    uint8_t buf[64];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));

    bool ok = pl_encode_message(&stream, &FixedBytesMsg_msg, &msg);
    CHECK("fixed length bytes encodes", ok);
    /* tag=(1<<3|2)=0x0A, len=4, data */
    uint8_t expected[] = {0x0A, 0x04, 0x01, 0x02, 0x03, 0x04};
    CHECK_BUF("fixed length bytes data", buf, stream.bytes_written, expected, sizeof(expected));
}

/* === Test repeated packed fixed32 === */
typedef struct {
    pl_size_t values_count;
    float values[4];
} RepeatedFixed32Msg;

static const uint32_t RepeatedFixed32Msg_field_info[] = {
    PL_FIELDINFO_2(1, PL_ALLOC_STATIC | PL_CARD_REPEATED | PL_DTYPE_FIXED32,
                   offsetof(RepeatedFixed32Msg, values), sizeof(float),
                   pl_delta(RepeatedFixed32Msg, values, values_count), 4)
    0
};
static const pl_msg_descriptor_t *const RepeatedFixed32Msg_submsg_info[] = { NULL };
static const pl_msg_descriptor_t RepeatedFixed32Msg_msg = {
    RepeatedFixed32Msg_field_info, RepeatedFixed32Msg_submsg_info, NULL, NULL, 1, 0, 1
};

static void test_repeated_fixed32(void)
{
    RepeatedFixed32Msg msg;
    memset(&msg, 0, sizeof(msg));
    msg.values_count = 2;
    msg.values[0] = 1.0f;
    msg.values[1] = 2.0f;

    uint8_t buf[64];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));

    bool ok = pl_encode_message(&stream, &RepeatedFixed32Msg_msg, &msg);
    CHECK("repeated fixed32 encodes", ok);
    /* packed: tag=0x0A, len=8 (2*4 bytes) */
    CHECK("repeated fixed32 tag", buf[0] == 0x0A);
    CHECK("repeated fixed32 len", buf[1] == 8);
    CHECK("repeated fixed32 total size", stream.bytes_written == 10);
    float v0, v1;
    memcpy(&v0, &buf[2], 4);
    memcpy(&v1, &buf[6], 4);
    CHECK("repeated fixed32 val[0]", v0 == 1.0f);
    CHECK("repeated fixed32 val[1]", v1 == 2.0f);
}

/* === Test PL_BIND macro encoding === */
typedef struct {
    int32_t value;
    bool has_label;
    char label[16];
} BindEncMsg;

#define BindEncMsg_FIELDLIST(X, a) \
    X(a, STATIC, REQUIRED, INT32, value, 1) \
    X(a, STATIC, OPTIONAL, STRING, label, 2)

#define BindEncMsg_DEFAULT NULL
#define BindEncMsg_CALLBACK NULL

PL_BIND(BindEncMsg, BindEncMsg, AUTO)

static void test_bind_macro_encode(void)
{
    BindEncMsg msg;
    memset(&msg, 0, sizeof(msg));
    msg.value = 7;
    msg.has_label = true;
    strcpy(msg.label, "ok");

    uint8_t buf[64];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));

    bool ok = pl_encode_message(&stream, &BindEncMsg_msg, &msg);
    CHECK("bind macro encodes", ok);
    uint8_t expected[] = {0x08, 0x07, 0x12, 0x02, 'o', 'k'};
    CHECK_BUF("bind macro bytes", buf, stream.bytes_written, expected, sizeof(expected));
}

int main(void)
{
    printf("=== Message Encoding Tests ===\n");

    test_simple_int32();
    test_simple_int32_large();
    test_simple_int32_negative();
    test_optional_present();
    test_optional_absent();
    test_string_field();
    test_empty_string();
    test_repeated_packed();
    test_repeated_empty();
    test_submessage();
    test_bool_field();
    test_bool_false();
    test_fixed32_field();
    test_svarint_field();
    test_svarint_positive();
    test_svarint_neg2();
    test_bytes_field();
    test_proto3_defaults_skipped();
    test_proto3_nonzero_encodes();
    test_proto3_partial();
    test_empty_message();
    test_oneof_int();
    test_oneof_float();
    test_oneof_none();
    test_get_encoded_size();
    test_get_encoded_size_matches_actual();
    test_encode_delimited();
    test_encode_nullterminated();
    test_buffer_overflow();
    test_sizing_stream();
    test_multi_field();
    test_multi_field_optional_absent();
    test_repeated_strings();
    test_uvarint_field();
    test_fixed_length_bytes();
    test_repeated_fixed32();
    test_bind_macro_encode();

    printf("\nResults: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
