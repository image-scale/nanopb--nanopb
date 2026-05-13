#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "../include/protolite.h"
#include "../include/protolite_encode.h"
#include "../include/protolite_decode.h"

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

static void test_istream_from_buffer(void)
{
    pl_byte_t data[] = {0x01, 0x02, 0x03};
    pl_istream_t stream = pl_istream_from_buffer(data, sizeof(data));

    CHECK("istream callback non-null", stream.callback != NULL);
    CHECK("istream bytes_left", stream.bytes_left == 3);
    CHECK("istream errmsg null", stream.errmsg == NULL);
}

static void test_read_basic(void)
{
    pl_byte_t data[] = {0xAA, 0xBB, 0xCC};
    pl_istream_t stream = pl_istream_from_buffer(data, sizeof(data));

    pl_byte_t buf[3];
    bool ok = pl_read(&stream, buf, 3);

    CHECK("read ok", ok);
    CHECK("read bytes_left=0", stream.bytes_left == 0);
    CHECK("read buf[0]=0xAA", buf[0] == 0xAA);
    CHECK("read buf[1]=0xBB", buf[1] == 0xBB);
    CHECK("read buf[2]=0xCC", buf[2] == 0xCC);
}

static void test_read_underflow(void)
{
    pl_byte_t data[] = {0x01, 0x02};
    pl_istream_t stream = pl_istream_from_buffer(data, sizeof(data));

    pl_byte_t buf[5];
    bool ok = pl_read(&stream, buf, 5);

    CHECK("read underflow returns false", !ok);
    CHECK("read underflow errmsg set", stream.errmsg != NULL);
}

static void test_read_empty(void)
{
    pl_byte_t data[] = {0x01};
    pl_istream_t stream = pl_istream_from_buffer(data, 1);

    bool ok = pl_read(&stream, NULL, 0);
    CHECK("zero-length read ok", ok);
    CHECK("bytes_left unchanged", stream.bytes_left == 1);
}

static void test_decode_varint_zero(void)
{
    pl_byte_t data[] = {0x00};
    pl_istream_t stream = pl_istream_from_buffer(data, sizeof(data));

    uint64_t val;
    bool ok = pl_decode_varint(&stream, &val);
    CHECK("varint(0) decode ok", ok);
    CHECK("varint(0) val=0", val == 0);
}

static void test_decode_varint_one(void)
{
    pl_byte_t data[] = {0x01};
    pl_istream_t stream = pl_istream_from_buffer(data, sizeof(data));

    uint64_t val;
    bool ok = pl_decode_varint(&stream, &val);
    CHECK("varint(1) decode ok", ok);
    CHECK("varint(1) val=1", val == 1);
}

static void test_decode_varint_127(void)
{
    pl_byte_t data[] = {0x7F};
    pl_istream_t stream = pl_istream_from_buffer(data, sizeof(data));

    uint64_t val;
    bool ok = pl_decode_varint(&stream, &val);
    CHECK("varint(127) decode ok", ok);
    CHECK("varint(127) val=127", val == 127);
}

static void test_decode_varint_128(void)
{
    pl_byte_t data[] = {0x80, 0x01};
    pl_istream_t stream = pl_istream_from_buffer(data, sizeof(data));

    uint64_t val;
    bool ok = pl_decode_varint(&stream, &val);
    CHECK("varint(128) decode ok", ok);
    CHECK("varint(128) val=128", val == 128);
}

static void test_decode_varint_300(void)
{
    pl_byte_t data[] = {0xAC, 0x02};
    pl_istream_t stream = pl_istream_from_buffer(data, sizeof(data));

    uint64_t val;
    bool ok = pl_decode_varint(&stream, &val);
    CHECK("varint(300) decode ok", ok);
    CHECK("varint(300) val=300", val == 300);
}

static void test_decode_varint_150(void)
{
    pl_byte_t data[] = {0x96, 0x01};
    pl_istream_t stream = pl_istream_from_buffer(data, sizeof(data));

    uint64_t val;
    bool ok = pl_decode_varint(&stream, &val);
    CHECK("varint(150) decode ok", ok);
    CHECK("varint(150) val=150", val == 150);
}

static void test_decode_varint32_basic(void)
{
    pl_byte_t data[] = {0xAC, 0x02};
    pl_istream_t stream = pl_istream_from_buffer(data, sizeof(data));

    uint32_t val;
    bool ok = pl_decode_varint32(&stream, &val);
    CHECK("varint32(300) ok", ok);
    CHECK("varint32(300) val=300", val == 300);
}

static void test_decode_varint32_max(void)
{
    pl_byte_t data[] = {0xFF, 0xFF, 0xFF, 0xFF, 0x0F};
    pl_istream_t stream = pl_istream_from_buffer(data, sizeof(data));

    uint32_t val;
    bool ok = pl_decode_varint32(&stream, &val);
    CHECK("varint32(max) ok", ok);
    CHECK("varint32(max) val=0xFFFFFFFF", val == 0xFFFFFFFF);
}

static void test_decode_bool_true(void)
{
    pl_byte_t data[] = {0x01};
    pl_istream_t stream = pl_istream_from_buffer(data, sizeof(data));

    bool val;
    bool ok = pl_decode_bool(&stream, &val);
    CHECK("decode_bool(1) ok", ok);
    CHECK("decode_bool(1) true", val == true);
}

static void test_decode_bool_false(void)
{
    pl_byte_t data[] = {0x00};
    pl_istream_t stream = pl_istream_from_buffer(data, sizeof(data));

    bool val;
    bool ok = pl_decode_bool(&stream, &val);
    CHECK("decode_bool(0) ok", ok);
    CHECK("decode_bool(0) false", val == false);
}

static void test_decode_bool_nonzero(void)
{
    pl_byte_t data[] = {0x05};
    pl_istream_t stream = pl_istream_from_buffer(data, sizeof(data));

    bool val;
    bool ok = pl_decode_bool(&stream, &val);
    CHECK("decode_bool(5) ok", ok);
    CHECK("decode_bool(5) true", val == true);
}

static void test_decode_svarint(void)
{
    struct { pl_byte_t data[2]; size_t len; int64_t expected; const char *name; } cases[] = {
        {{0x00},       1, 0,   "svarint(0)=0"},
        {{0x01},       1, -1,  "svarint(1)=-1"},
        {{0x02},       1, 1,   "svarint(2)=1"},
        {{0x03},       1, -2,  "svarint(3)=-2"},
        {{0x04},       1, 2,   "svarint(4)=2"},
        {{0xFE, 0x01}, 2, 127, "svarint(254)=127"},
        {{0xFF, 0x01}, 2, -128,"svarint(255)=-128"},
    };

    for (size_t i = 0; i < sizeof(cases)/sizeof(cases[0]); i++)
    {
        pl_istream_t stream = pl_istream_from_buffer(cases[i].data, cases[i].len);
        int64_t val;
        bool ok = pl_decode_svarint(&stream, &val);
        CHECK(cases[i].name, ok && val == cases[i].expected);
    }
}

static void test_decode_fixed32(void)
{
    pl_byte_t data[] = {0x78, 0x56, 0x34, 0x12};
    pl_istream_t stream = pl_istream_from_buffer(data, sizeof(data));

    uint32_t val;
    bool ok = pl_decode_fixed32(&stream, &val);
    CHECK("fixed32 decode ok", ok);
    CHECK("fixed32 val=0x12345678", val == 0x12345678);
}

static void test_decode_fixed64(void)
{
    pl_byte_t data[] = {0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01};
    pl_istream_t stream = pl_istream_from_buffer(data, sizeof(data));

    uint64_t val;
    bool ok = pl_decode_fixed64(&stream, &val);
    CHECK("fixed64 decode ok", ok);
    CHECK("fixed64 val=0x0102030405060708", val == 0x0102030405060708ULL);
}

static void test_decode_tag(void)
{
    /* Field 1, VARINT: 0x08 = (1 << 3) | 0 */
    pl_byte_t data1[] = {0x08};
    pl_istream_t stream = pl_istream_from_buffer(data1, sizeof(data1));
    pl_wire_type_t wt;
    uint32_t tag;
    bool eof;

    bool ok = pl_decode_tag(&stream, &wt, &tag, &eof);
    CHECK("tag(1,VARINT) ok", ok);
    CHECK("tag(1,VARINT) tag=1", tag == 1);
    CHECK("tag(1,VARINT) wt=VARINT", wt == PL_WT_VARINT);
    CHECK("tag(1,VARINT) !eof", !eof);

    /* Field 1, STRING: 0x0A = (1 << 3) | 2 */
    pl_byte_t data2[] = {0x0A};
    stream = pl_istream_from_buffer(data2, sizeof(data2));
    ok = pl_decode_tag(&stream, &wt, &tag, &eof);
    CHECK("tag(1,STRING) ok", ok);
    CHECK("tag(1,STRING) tag=1", tag == 1);
    CHECK("tag(1,STRING) wt=STRING", wt == PL_WT_STRING);

    /* Field 16, VARINT: 0x80 0x01 */
    pl_byte_t data3[] = {0x80, 0x01};
    stream = pl_istream_from_buffer(data3, sizeof(data3));
    ok = pl_decode_tag(&stream, &wt, &tag, &eof);
    CHECK("tag(16,VARINT) ok", ok);
    CHECK("tag(16,VARINT) tag=16", tag == 16);
    CHECK("tag(16,VARINT) wt=VARINT", wt == PL_WT_VARINT);
}

static void test_decode_tag_eof(void)
{
    pl_istream_t stream = pl_istream_from_buffer(NULL, 0);
    pl_wire_type_t wt;
    uint32_t tag;
    bool eof;

    bool ok = pl_decode_tag(&stream, &wt, &tag, &eof);
    CHECK("tag eof returns false", !ok);
    CHECK("tag eof flag set", eof == true);
}

static void test_skip_field_varint(void)
{
    pl_byte_t data[] = {0x96, 0x01};
    pl_istream_t stream = pl_istream_from_buffer(data, sizeof(data));

    bool ok = pl_skip_field(&stream, PL_WT_VARINT);
    CHECK("skip varint ok", ok);
    CHECK("skip varint bytes_left=0", stream.bytes_left == 0);
}

static void test_skip_field_64bit(void)
{
    pl_byte_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    pl_istream_t stream = pl_istream_from_buffer(data, sizeof(data));

    bool ok = pl_skip_field(&stream, PL_WT_64BIT);
    CHECK("skip 64bit ok", ok);
    CHECK("skip 64bit bytes_left=0", stream.bytes_left == 0);
}

static void test_skip_field_32bit(void)
{
    pl_byte_t data[] = {0x01, 0x02, 0x03, 0x04};
    pl_istream_t stream = pl_istream_from_buffer(data, sizeof(data));

    bool ok = pl_skip_field(&stream, PL_WT_32BIT);
    CHECK("skip 32bit ok", ok);
    CHECK("skip 32bit bytes_left=0", stream.bytes_left == 0);
}

static void test_skip_field_string(void)
{
    pl_byte_t data[] = {0x03, 0xAA, 0xBB, 0xCC};
    pl_istream_t stream = pl_istream_from_buffer(data, sizeof(data));

    bool ok = pl_skip_field(&stream, PL_WT_STRING);
    CHECK("skip string ok", ok);
    CHECK("skip string bytes_left=0", stream.bytes_left == 0);
}

static void test_string_substream(void)
{
    /* Simulate a length-delimited field: 3 bytes of data, then more data */
    pl_byte_t data[] = {0x03, 0xAA, 0xBB, 0xCC, 0xDD};
    pl_istream_t stream = pl_istream_from_buffer(data, sizeof(data));
    pl_istream_t sub;

    bool ok = pl_make_string_substream(&stream, &sub);
    CHECK("make_substream ok", ok);
    CHECK("substream bytes_left=3", sub.bytes_left == 3);
    CHECK("parent bytes_left=1", stream.bytes_left == 1);

    pl_byte_t buf[3];
    ok = pl_read(&sub, buf, 3);
    CHECK("substream read ok", ok);
    CHECK("substream buf[0]=0xAA", buf[0] == 0xAA);
    CHECK("substream buf[1]=0xBB", buf[1] == 0xBB);
    CHECK("substream buf[2]=0xCC", buf[2] == 0xCC);

    ok = pl_close_string_substream(&stream, &sub);
    CHECK("close_substream ok", ok);

    /* Read remaining byte from parent */
    pl_byte_t last;
    ok = pl_read(&stream, &last, 1);
    CHECK("parent read after sub ok", ok);
    CHECK("parent remaining=0xDD", last == 0xDD);
}

static void test_roundtrip_varint(void)
{
    uint64_t test_values[] = {0, 1, 127, 128, 255, 256, 300, 10000,
                               0xFFFFFFFF, 0x100000000ULL, UINT64_MAX};

    for (size_t i = 0; i < sizeof(test_values)/sizeof(test_values[0]); i++)
    {
        pl_byte_t buf[16];
        pl_ostream_t ostream = pl_ostream_from_buffer(buf, sizeof(buf));
        pl_encode_varint(&ostream, test_values[i]);

        pl_istream_t istream = pl_istream_from_buffer(buf, ostream.bytes_written);
        uint64_t decoded;
        bool ok = pl_decode_varint(&istream, &decoded);
        CHECK("roundtrip varint decode ok", ok);
        CHECK("roundtrip varint value match", decoded == test_values[i]);
    }
}

static void test_roundtrip_svarint(void)
{
    int64_t test_values[] = {0, 1, -1, 2, -2, 127, -128, 10000, -10000,
                              INT64_MAX, INT64_MIN};

    for (size_t i = 0; i < sizeof(test_values)/sizeof(test_values[0]); i++)
    {
        pl_byte_t buf[16];
        pl_ostream_t ostream = pl_ostream_from_buffer(buf, sizeof(buf));
        pl_encode_svarint(&ostream, test_values[i]);

        pl_istream_t istream = pl_istream_from_buffer(buf, ostream.bytes_written);
        int64_t decoded;
        bool ok = pl_decode_svarint(&istream, &decoded);
        CHECK("roundtrip svarint decode ok", ok);
        CHECK("roundtrip svarint value match", decoded == test_values[i]);
    }
}

static void test_roundtrip_fixed32(void)
{
    uint32_t test_values[] = {0, 1, 0x12345678, 0xFFFFFFFF, 42};

    for (size_t i = 0; i < sizeof(test_values)/sizeof(test_values[0]); i++)
    {
        pl_byte_t buf[8];
        pl_ostream_t ostream = pl_ostream_from_buffer(buf, sizeof(buf));
        pl_encode_fixed32(&ostream, &test_values[i]);

        pl_istream_t istream = pl_istream_from_buffer(buf, 4);
        uint32_t decoded;
        bool ok = pl_decode_fixed32(&istream, &decoded);
        CHECK("roundtrip fixed32 decode ok", ok);
        CHECK("roundtrip fixed32 value match", decoded == test_values[i]);
    }
}

static void test_roundtrip_fixed64(void)
{
    uint64_t test_values[] = {0, 1, 0x0102030405060708ULL, UINT64_MAX, 42};

    for (size_t i = 0; i < sizeof(test_values)/sizeof(test_values[0]); i++)
    {
        pl_byte_t buf[16];
        pl_ostream_t ostream = pl_ostream_from_buffer(buf, sizeof(buf));
        pl_encode_fixed64(&ostream, &test_values[i]);

        pl_istream_t istream = pl_istream_from_buffer(buf, 8);
        uint64_t decoded;
        bool ok = pl_decode_fixed64(&istream, &decoded);
        CHECK("roundtrip fixed64 decode ok", ok);
        CHECK("roundtrip fixed64 value match", decoded == test_values[i]);
    }
}

static void test_roundtrip_tag(void)
{
    struct { uint32_t field_num; pl_wire_type_t wt; } cases[] = {
        {1, PL_WT_VARINT}, {1, PL_WT_STRING}, {1, PL_WT_32BIT}, {1, PL_WT_64BIT},
        {2, PL_WT_VARINT}, {15, PL_WT_VARINT}, {16, PL_WT_VARINT}, {100, PL_WT_STRING},
    };

    for (size_t i = 0; i < sizeof(cases)/sizeof(cases[0]); i++)
    {
        pl_byte_t buf[16];
        pl_ostream_t ostream = pl_ostream_from_buffer(buf, sizeof(buf));
        pl_encode_tag(&ostream, cases[i].wt, cases[i].field_num);

        pl_istream_t istream = pl_istream_from_buffer(buf, ostream.bytes_written);
        pl_wire_type_t wt;
        uint32_t tag;
        bool eof;
        bool ok = pl_decode_tag(&istream, &wt, &tag, &eof);
        CHECK("roundtrip tag ok", ok);
        CHECK("roundtrip tag num match", tag == cases[i].field_num);
        CHECK("roundtrip tag wt match", wt == cases[i].wt);
    }
}

int main(void)
{
    printf("=== Wire Format Decoding Tests ===\n");

    test_istream_from_buffer();
    test_read_basic();
    test_read_underflow();
    test_read_empty();
    test_decode_varint_zero();
    test_decode_varint_one();
    test_decode_varint_127();
    test_decode_varint_128();
    test_decode_varint_300();
    test_decode_varint_150();
    test_decode_varint32_basic();
    test_decode_varint32_max();
    test_decode_bool_true();
    test_decode_bool_false();
    test_decode_bool_nonzero();
    test_decode_svarint();
    test_decode_fixed32();
    test_decode_fixed64();
    test_decode_tag();
    test_decode_tag_eof();
    test_skip_field_varint();
    test_skip_field_64bit();
    test_skip_field_32bit();
    test_skip_field_string();
    test_string_substream();
    test_roundtrip_varint();
    test_roundtrip_svarint();
    test_roundtrip_fixed32();
    test_roundtrip_fixed64();
    test_roundtrip_tag();

    printf("\nResults: %d passed, %d failed\n", tests_passed, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
