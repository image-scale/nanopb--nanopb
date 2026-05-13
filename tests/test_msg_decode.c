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

static void test_decode_simple_int32(void)
{
    uint8_t data[] = {0x08, 0x2A};
    pl_istream_t stream = pl_istream_from_buffer(data, sizeof(data));
    SimpleMsg msg;

    bool ok = pl_decode_message(&stream, &SimpleMsg_msg, &msg);
    CHECK("simple decode ok", ok);
    CHECK("simple decode value=42", msg.id == 42);
}

static void test_decode_large_int32(void)
{
    uint8_t data[] = {0x08, 0xAC, 0x02};
    pl_istream_t stream = pl_istream_from_buffer(data, sizeof(data));
    SimpleMsg msg;

    bool ok = pl_decode_message(&stream, &SimpleMsg_msg, &msg);
    CHECK("large decode ok", ok);
    CHECK("large decode value=300", msg.id == 300);
}

static void test_decode_negative_int32(void)
{
    uint8_t data[] = {0x08, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01};
    pl_istream_t stream = pl_istream_from_buffer(data, sizeof(data));
    SimpleMsg msg;

    bool ok = pl_decode_message(&stream, &SimpleMsg_msg, &msg);
    CHECK("negative decode ok", ok);
    CHECK("negative decode value=-1", msg.id == -1);
}

/* === Message 2: Optional field === */
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

static void test_decode_optional_present(void)
{
    uint8_t data[] = {0x08, 0x07};
    pl_istream_t stream = pl_istream_from_buffer(data, sizeof(data));
    OptionalMsg msg;

    bool ok = pl_decode_message(&stream, &OptionalMsg_msg, &msg);
    CHECK("optional present ok", ok);
    CHECK("optional present has=true", msg.has_value == true);
    CHECK("optional present value=7", msg.value == 7);
}

static void test_decode_optional_absent(void)
{
    uint8_t data[] = {};
    pl_istream_t stream = pl_istream_from_buffer(data, 0);
    OptionalMsg msg;
    msg.has_value = true;
    msg.value = 99;

    bool ok = pl_decode_message(&stream, &OptionalMsg_msg, &msg);
    CHECK("optional absent ok", ok);
    CHECK("optional absent has=false", msg.has_value == false);
    CHECK("optional absent value=0", msg.value == 0);
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

static void test_decode_string(void)
{
    uint8_t data[] = {0x08, 0x01, 0x12, 0x05, 'A', 'l', 'i', 'c', 'e'};
    pl_istream_t stream = pl_istream_from_buffer(data, sizeof(data));
    PersonMsg msg;

    bool ok = pl_decode_message(&stream, &PersonMsg_msg, &msg);
    CHECK("string decode ok", ok);
    CHECK("string decode id=1", msg.id == 1);
    CHECK("string decode name=Alice", strcmp(msg.name, "Alice") == 0);
}

static void test_decode_empty_string(void)
{
    uint8_t data[] = {0x08, 0x01, 0x12, 0x00};
    pl_istream_t stream = pl_istream_from_buffer(data, sizeof(data));
    PersonMsg msg;

    bool ok = pl_decode_message(&stream, &PersonMsg_msg, &msg);
    CHECK("empty string ok", ok);
    CHECK("empty string name=\"\"", msg.name[0] == '\0');
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

static void test_decode_repeated_packed(void)
{
    uint8_t data[] = {0x0A, 0x03, 0x01, 0x02, 0x03};
    pl_istream_t stream = pl_istream_from_buffer(data, sizeof(data));
    RepeatedMsg msg;

    bool ok = pl_decode_message(&stream, &RepeatedMsg_msg, &msg);
    CHECK("packed decode ok", ok);
    CHECK("packed count=3", msg.values_count == 3);
    CHECK("packed [0]=1", msg.values[0] == 1);
    CHECK("packed [1]=2", msg.values[1] == 2);
    CHECK("packed [2]=3", msg.values[2] == 3);
}

static void test_decode_repeated_empty(void)
{
    uint8_t data[] = {};
    pl_istream_t stream = pl_istream_from_buffer(data, 0);
    RepeatedMsg msg;

    bool ok = pl_decode_message(&stream, &RepeatedMsg_msg, &msg);
    CHECK("empty repeated ok", ok);
    CHECK("empty repeated count=0", msg.values_count == 0);
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

static void test_decode_submessage(void)
{
    uint8_t data[] = {0x08, 0x01, 0x12, 0x04, 0x08, 0x0A, 0x10, 0x14};
    pl_istream_t stream = pl_istream_from_buffer(data, sizeof(data));
    PlaceMsg msg;

    bool ok = pl_decode_message(&stream, &PlaceMsg_msg, &msg);
    CHECK("submsg decode ok", ok);
    CHECK("submsg id=1", msg.id == 1);
    CHECK("submsg x=10", msg.location.x == 10);
    CHECK("submsg y=20", msg.location.y == 20);
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

static void test_decode_bool(void)
{
    uint8_t data[] = {0x08, 0x01, 0x10, 0x05};
    pl_istream_t stream = pl_istream_from_buffer(data, sizeof(data));
    BoolMsg msg;

    bool ok = pl_decode_message(&stream, &BoolMsg_msg, &msg);
    CHECK("bool decode ok", ok);
    CHECK("bool active=true", msg.active == true);
    CHECK("bool count=5", msg.count == 5);
}

static void test_decode_bool_false(void)
{
    uint8_t data[] = {0x08, 0x00, 0x10, 0x05};
    pl_istream_t stream = pl_istream_from_buffer(data, sizeof(data));
    BoolMsg msg;

    bool ok = pl_decode_message(&stream, &BoolMsg_msg, &msg);
    CHECK("bool false ok", ok);
    CHECK("bool active=false", msg.active == false);
}

/* === Message 7: Fixed32 (float) === */
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

static void test_decode_fixed32(void)
{
    FloatMsg src = {3.14f};
    uint8_t buf[16];
    pl_ostream_t ostream = pl_ostream_from_buffer(buf, sizeof(buf));
    pl_encode_message(&ostream, &FloatMsg_msg, &src);

    pl_istream_t istream = pl_istream_from_buffer(buf, ostream.bytes_written);
    FloatMsg msg;
    bool ok = pl_decode_message(&istream, &FloatMsg_msg, &msg);
    CHECK("fixed32 decode ok", ok);
    CHECK("fixed32 value=3.14", msg.temperature == 3.14f);
}

/* === Message 8: Signed varint === */
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

static void test_decode_svarint(void)
{
    uint8_t data[] = {0x08, 0x01};
    pl_istream_t stream = pl_istream_from_buffer(data, sizeof(data));
    SvarintMsg msg;

    bool ok = pl_decode_message(&stream, &SvarintMsg_msg, &msg);
    CHECK("svarint decode ok", ok);
    CHECK("svarint value=-1", msg.value == -1);
}

static void test_decode_svarint_positive(void)
{
    uint8_t data[] = {0x08, 0x02};
    pl_istream_t stream = pl_istream_from_buffer(data, sizeof(data));
    SvarintMsg msg;

    bool ok = pl_decode_message(&stream, &SvarintMsg_msg, &msg);
    CHECK("svarint pos ok", ok);
    CHECK("svarint pos value=1", msg.value == 1);
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

static void test_decode_bytes(void)
{
    uint8_t data[] = {0x0A, 0x03, 0xDE, 0xAD, 0xBE};
    pl_istream_t stream = pl_istream_from_buffer(data, sizeof(data));
    BytesMsg msg;

    bool ok = pl_decode_message(&stream, &BytesMsg_msg, &msg);
    CHECK("bytes decode ok", ok);
    CHECK("bytes size=3", msg.data.size == 3);
    CHECK("bytes [0]=0xDE", msg.data.bytes[0] == 0xDE);
    CHECK("bytes [1]=0xAD", msg.data.bytes[1] == 0xAD);
    CHECK("bytes [2]=0xBE", msg.data.bytes[2] == 0xBE);
}

/* === Missing required field === */
static void test_missing_required(void)
{
    uint8_t data[] = {};
    pl_istream_t stream = pl_istream_from_buffer(data, 0);
    SimpleMsg msg;

    bool ok = pl_decode_message(&stream, &SimpleMsg_msg, &msg);
    CHECK("missing required returns false", !ok);
}

/* === Unknown fields skipped === */
static void test_unknown_fields_skipped(void)
{
    /* field 1=42, unknown field 99 (varint)=7 */
    uint8_t data[] = {0x08, 0x2A, 0xF8, 0x06, 0x07};
    pl_istream_t stream = pl_istream_from_buffer(data, sizeof(data));
    SimpleMsg msg;

    bool ok = pl_decode_message(&stream, &SimpleMsg_msg, &msg);
    CHECK("unknown skip ok", ok);
    CHECK("unknown skip value=42", msg.id == 42);
}

/* === Oneof field === */
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

static void test_decode_oneof_int(void)
{
    uint8_t data[] = {0x08, 0x63};
    pl_istream_t stream = pl_istream_from_buffer(data, sizeof(data));
    OneofMsg msg;

    bool ok = pl_decode_message(&stream, &OneofMsg_msg, &msg);
    CHECK("oneof int ok", ok);
    CHECK("oneof which=1", msg.which_value == 1);
    CHECK("oneof int=99", msg.value.int_val == 99);
}

static void test_decode_oneof_float(void)
{
    float fval = 1.0f;
    uint8_t data[6];
    data[0] = 0x15;
    memcpy(&data[1], &fval, 4);

    pl_istream_t stream = pl_istream_from_buffer(data, 5);
    OneofMsg msg;

    bool ok = pl_decode_message(&stream, &OneofMsg_msg, &msg);
    CHECK("oneof float ok", ok);
    CHECK("oneof which=2", msg.which_value == 2);
    CHECK("oneof float=1.0", msg.value.float_val == 1.0f);
}

/* === Empty message === */
static const uint32_t EmptyMsg_field_info[] = { 0 };
static const pl_msg_descriptor_t *const EmptyMsg_submsg_info[] = { NULL };
static const pl_msg_descriptor_t EmptyMsg_msg = {
    EmptyMsg_field_info, EmptyMsg_submsg_info, NULL, NULL, 0, 0, 0
};

static void test_decode_empty_message(void)
{
    uint8_t data[] = {};
    pl_istream_t stream = pl_istream_from_buffer(data, 0);
    int dummy = 0;

    bool ok = pl_decode_message(&stream, &EmptyMsg_msg, &dummy);
    CHECK("empty msg ok", ok);
}

/* === Defaults initialization === */
static void test_defaults_initialized(void)
{
    uint8_t data[] = {0x08, 0x01, 0x12, 0x02, 'h', 'i'};
    pl_istream_t stream = pl_istream_from_buffer(data, sizeof(data));
    PersonMsg msg;
    memset(&msg, 0xFF, sizeof(msg));

    bool ok = pl_decode_message(&stream, &PersonMsg_msg, &msg);
    CHECK("defaults init ok", ok);
    CHECK("defaults id=1", msg.id == 1);
    CHECK("defaults name=hi", strcmp(msg.name, "hi") == 0);
}

/* === Delimited mode === */
static void test_decode_delimited(void)
{
    uint8_t data[] = {0x02, 0x08, 0x2A};
    pl_istream_t stream = pl_istream_from_buffer(data, sizeof(data));
    SimpleMsg msg;

    bool ok = pl_decode_message_ex(&stream, &SimpleMsg_msg, &msg, PL_DECODE_DELIMITED);
    CHECK("delimited decode ok", ok);
    CHECK("delimited value=42", msg.id == 42);
}

/* === Null-terminated mode === */
static void test_decode_nullterminated(void)
{
    uint8_t data[] = {0x08, 0x2A, 0x00};
    pl_istream_t stream = pl_istream_from_buffer(data, sizeof(data));
    SimpleMsg msg;

    bool ok = pl_decode_message_ex(&stream, &SimpleMsg_msg, &msg, PL_DECODE_NULLTERMINATED);
    CHECK("nullterm decode ok", ok);
    CHECK("nullterm value=42", msg.id == 42);
}

/* === Unsigned varint (uint32) === */
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

static void test_decode_uvarint(void)
{
    uint8_t data[] = {0x08, 0xAC, 0x02};
    pl_istream_t stream = pl_istream_from_buffer(data, sizeof(data));
    UvarintMsg msg;

    bool ok = pl_decode_message(&stream, &UvarintMsg_msg, &msg);
    CHECK("uvarint decode ok", ok);
    CHECK("uvarint value=300", msg.value == 300);
}

/* === Fixed-length bytes === */
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

static void test_decode_fixed_length_bytes(void)
{
    uint8_t data[] = {0x0A, 0x04, 0x01, 0x02, 0x03, 0x04};
    pl_istream_t stream = pl_istream_from_buffer(data, sizeof(data));
    FixedBytesMsg msg;

    bool ok = pl_decode_message(&stream, &FixedBytesMsg_msg, &msg);
    CHECK("fixed bytes ok", ok);
    CHECK("fixed bytes [0]", msg.hash[0] == 0x01);
    CHECK("fixed bytes [1]", msg.hash[1] == 0x02);
    CHECK("fixed bytes [2]", msg.hash[2] == 0x03);
    CHECK("fixed bytes [3]", msg.hash[3] == 0x04);
}

/* === Repeated packed fixed32 === */
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

static void test_decode_repeated_fixed32(void)
{
    RepeatedFixed32Msg src;
    memset(&src, 0, sizeof(src));
    src.values_count = 2;
    src.values[0] = 1.0f;
    src.values[1] = 2.0f;

    uint8_t buf[64];
    pl_ostream_t ostream = pl_ostream_from_buffer(buf, sizeof(buf));
    pl_encode_message(&ostream, &RepeatedFixed32Msg_msg, &src);

    pl_istream_t istream = pl_istream_from_buffer(buf, ostream.bytes_written);
    RepeatedFixed32Msg msg;
    bool ok = pl_decode_message(&istream, &RepeatedFixed32Msg_msg, &msg);
    CHECK("rep fixed32 ok", ok);
    CHECK("rep fixed32 count=2", msg.values_count == 2);
    CHECK("rep fixed32 [0]=1.0", msg.values[0] == 1.0f);
    CHECK("rep fixed32 [1]=2.0", msg.values[1] == 2.0f);
}

/* === Repeated strings (unpacked) === */
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

static void test_decode_repeated_strings(void)
{
    uint8_t data[] = {0x0A, 0x02, 'h', 'i', 0x0A, 0x02, 'l', 'o'};
    pl_istream_t stream = pl_istream_from_buffer(data, sizeof(data));
    RepeatedStringMsg msg;

    bool ok = pl_decode_message(&stream, &RepeatedStringMsg_msg, &msg);
    CHECK("rep strings ok", ok);
    CHECK("rep strings count=2", msg.names_count == 2);
    CHECK("rep strings [0]=hi", strcmp(msg.names[0], "hi") == 0);
    CHECK("rep strings [1]=lo", strcmp(msg.names[1], "lo") == 0);
}

/* === Full round-trip tests === */
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

static void test_roundtrip_multi(void)
{
    MultiMsg src;
    memset(&src, 0, sizeof(src));
    src.id = 42;
    src.has_label = true;
    strcpy(src.label, "test");
    src.tags_count = 3;
    src.tags[0] = 10;
    src.tags[1] = 20;
    src.tags[2] = 30;

    uint8_t buf[128];
    pl_ostream_t ostream = pl_ostream_from_buffer(buf, sizeof(buf));
    bool ok = pl_encode_message(&ostream, &MultiMsg_msg, &src);
    CHECK("rt encode ok", ok);

    pl_istream_t istream = pl_istream_from_buffer(buf, ostream.bytes_written);
    MultiMsg dst;
    ok = pl_decode_message(&istream, &MultiMsg_msg, &dst);
    CHECK("rt decode ok", ok);
    CHECK("rt id", dst.id == src.id);
    CHECK("rt has_label", dst.has_label == src.has_label);
    CHECK("rt label", strcmp(dst.label, src.label) == 0);
    CHECK("rt tags_count", dst.tags_count == src.tags_count);
    CHECK("rt tags[0]", dst.tags[0] == src.tags[0]);
    CHECK("rt tags[1]", dst.tags[1] == src.tags[1]);
    CHECK("rt tags[2]", dst.tags[2] == src.tags[2]);
}

static void test_roundtrip_submessage(void)
{
    PlaceMsg src;
    memset(&src, 0, sizeof(src));
    src.id = 7;
    src.location.x = 100;
    src.location.y = -200;

    uint8_t buf[128];
    pl_ostream_t ostream = pl_ostream_from_buffer(buf, sizeof(buf));
    bool ok = pl_encode_message(&ostream, &PlaceMsg_msg, &src);
    CHECK("rt sub encode ok", ok);

    pl_istream_t istream = pl_istream_from_buffer(buf, ostream.bytes_written);
    PlaceMsg dst;
    ok = pl_decode_message(&istream, &PlaceMsg_msg, &dst);
    CHECK("rt sub decode ok", ok);
    CHECK("rt sub id", dst.id == src.id);
    CHECK("rt sub x", dst.location.x == src.location.x);
    CHECK("rt sub y", dst.location.y == src.location.y);
}

static void test_roundtrip_oneof(void)
{
    OneofMsg src;
    memset(&src, 0, sizeof(src));
    src.which_value = 2;
    src.value.float_val = 2.5f;

    uint8_t buf[64];
    pl_ostream_t ostream = pl_ostream_from_buffer(buf, sizeof(buf));
    bool ok = pl_encode_message(&ostream, &OneofMsg_msg, &src);
    CHECK("rt oneof encode ok", ok);

    pl_istream_t istream = pl_istream_from_buffer(buf, ostream.bytes_written);
    OneofMsg dst;
    ok = pl_decode_message(&istream, &OneofMsg_msg, &dst);
    CHECK("rt oneof decode ok", ok);
    CHECK("rt oneof which", dst.which_value == src.which_value);
    CHECK("rt oneof float", dst.value.float_val == src.value.float_val);
}

static void test_roundtrip_bytes(void)
{
    BytesMsg src;
    memset(&src, 0, sizeof(src));
    src.data.size = 4;
    src.data.bytes[0] = 0xCA;
    src.data.bytes[1] = 0xFE;
    src.data.bytes[2] = 0xBA;
    src.data.bytes[3] = 0xBE;

    uint8_t buf[64];
    pl_ostream_t ostream = pl_ostream_from_buffer(buf, sizeof(buf));
    bool ok = pl_encode_message(&ostream, &BytesMsg_msg, &src);
    CHECK("rt bytes encode ok", ok);

    pl_istream_t istream = pl_istream_from_buffer(buf, ostream.bytes_written);
    BytesMsg dst;
    ok = pl_decode_message(&istream, &BytesMsg_msg, &dst);
    CHECK("rt bytes decode ok", ok);
    CHECK("rt bytes size", dst.data.size == src.data.size);
    CHECK("rt bytes match", memcmp(dst.data.bytes, src.data.bytes, 4) == 0);
}

/* === PL_BIND macro round-trip === */
typedef struct {
    int32_t value;
    bool has_label;
    char label[16];
} BindMsg;

#define BindMsg_FIELDLIST(X, a) \
    X(a, STATIC, REQUIRED, INT32, value, 1) \
    X(a, STATIC, OPTIONAL, STRING, label, 2)

#define BindMsg_DEFAULT NULL
#define BindMsg_CALLBACK NULL

PL_BIND(BindMsg, BindMsg, AUTO)

static void test_roundtrip_bind(void)
{
    BindMsg src;
    memset(&src, 0, sizeof(src));
    src.value = 99;
    src.has_label = true;
    strcpy(src.label, "bind");

    uint8_t buf[64];
    pl_ostream_t ostream = pl_ostream_from_buffer(buf, sizeof(buf));
    bool ok = pl_encode_message(&ostream, &BindMsg_msg, &src);
    CHECK("rt bind encode ok", ok);

    pl_istream_t istream = pl_istream_from_buffer(buf, ostream.bytes_written);
    BindMsg dst;
    ok = pl_decode_message(&istream, &BindMsg_msg, &dst);
    CHECK("rt bind decode ok", ok);
    CHECK("rt bind value", dst.value == src.value);
    CHECK("rt bind has_label", dst.has_label == true);
    CHECK("rt bind label", strcmp(dst.label, "bind") == 0);
}

/* === NOINIT flag === */
static void test_decode_noinit(void)
{
    OptionalMsg msg;
    msg.has_value = true;
    msg.value = 99;

    uint8_t data[] = {};
    pl_istream_t stream = pl_istream_from_buffer(data, 0);

    bool ok = pl_decode_message_ex(&stream, &OptionalMsg_msg, &msg, PL_DECODE_NOINIT);
    CHECK("noinit ok", ok);
    CHECK("noinit preserves has_value", msg.has_value == true);
    CHECK("noinit preserves value", msg.value == 99);
}

/* === Wire type mismatch === */
static void test_wrong_wire_type(void)
{
    /* Send field 1 with wire type 2 (string) but it expects varint */
    uint8_t data[] = {0x0A, 0x01, 0x42};
    pl_istream_t stream = pl_istream_from_buffer(data, sizeof(data));
    SimpleMsg msg;

    bool ok = pl_decode_message(&stream, &SimpleMsg_msg, &msg);
    CHECK("wrong wire type fails", !ok);
}

int main(void)
{
    printf("=== Message Decoding Tests ===\n");

    test_decode_simple_int32();
    test_decode_large_int32();
    test_decode_negative_int32();
    test_decode_optional_present();
    test_decode_optional_absent();
    test_decode_string();
    test_decode_empty_string();
    test_decode_repeated_packed();
    test_decode_repeated_empty();
    test_decode_submessage();
    test_decode_bool();
    test_decode_bool_false();
    test_decode_fixed32();
    test_decode_svarint();
    test_decode_svarint_positive();
    test_decode_bytes();
    test_missing_required();
    test_unknown_fields_skipped();
    test_decode_oneof_int();
    test_decode_oneof_float();
    test_decode_empty_message();
    test_defaults_initialized();
    test_decode_delimited();
    test_decode_nullterminated();
    test_decode_uvarint();
    test_decode_fixed_length_bytes();
    test_decode_repeated_fixed32();
    test_decode_repeated_strings();
    test_roundtrip_multi();
    test_roundtrip_submessage();
    test_roundtrip_oneof();
    test_roundtrip_bytes();
    test_roundtrip_bind();
    test_decode_noinit();
    test_wrong_wire_type();

    printf("\nResults: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
