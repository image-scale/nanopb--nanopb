#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "../include/protolite.h"
#include "../include/protolite_encode.h"

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

static void test_ostream_from_buffer(void)
{
    pl_byte_t buf[64];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));

    CHECK("ostream callback non-null", stream.callback != NULL);
    CHECK("ostream state is buf", stream.state == buf);
    CHECK("ostream max_size", stream.max_size == 64);
    CHECK("ostream bytes_written zero", stream.bytes_written == 0);
    CHECK("ostream errmsg null", stream.errmsg == NULL);
}

static void test_write_basic(void)
{
    pl_byte_t buf[64];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));

    pl_byte_t data[] = {0x01, 0x02, 0x03};
    bool ok = pl_write(&stream, data, 3);

    CHECK("write returns true", ok);
    CHECK("bytes_written is 3", stream.bytes_written == 3);
    CHECK("buf[0] == 0x01", buf[0] == 0x01);
    CHECK("buf[1] == 0x02", buf[1] == 0x02);
    CHECK("buf[2] == 0x03", buf[2] == 0x03);
}

static void test_write_overflow(void)
{
    pl_byte_t buf[4];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));

    pl_byte_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    bool ok = pl_write(&stream, data, 5);

    CHECK("write overflow returns false", !ok);
    CHECK("errmsg set on overflow", stream.errmsg != NULL);
}

static void test_write_empty(void)
{
    pl_byte_t buf[8];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));

    bool ok = pl_write(&stream, NULL, 0);

    CHECK("zero-length write succeeds", ok);
    CHECK("bytes_written still 0", stream.bytes_written == 0);
}

static void test_sizing_stream(void)
{
    pl_ostream_t stream = PL_OSTREAM_SIZING;

    pl_byte_t data[] = {0xAA, 0xBB, 0xCC};
    bool ok = pl_write(&stream, data, 3);

    CHECK("sizing stream write succeeds", ok);
    CHECK("sizing stream counts bytes", stream.bytes_written == 3);

    ok = pl_write(&stream, data, 2);
    CHECK("sizing stream accumulates", ok);
    CHECK("sizing stream total 5", stream.bytes_written == 5);
}

static void test_varint_zero(void)
{
    pl_byte_t buf[16];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));

    bool ok = pl_encode_varint(&stream, 0);
    CHECK("varint(0) ok", ok);
    CHECK("varint(0) size=1", stream.bytes_written == 1);
    CHECK("varint(0) byte=0x00", buf[0] == 0x00);
}

static void test_varint_one(void)
{
    pl_byte_t buf[16];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));

    bool ok = pl_encode_varint(&stream, 1);
    CHECK("varint(1) ok", ok);
    CHECK("varint(1) size=1", stream.bytes_written == 1);
    CHECK("varint(1) byte=0x01", buf[0] == 0x01);
}

static void test_varint_127(void)
{
    pl_byte_t buf[16];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));

    bool ok = pl_encode_varint(&stream, 127);
    CHECK("varint(127) ok", ok);
    CHECK("varint(127) size=1", stream.bytes_written == 1);
    CHECK("varint(127) byte=0x7F", buf[0] == 0x7F);
}

static void test_varint_128(void)
{
    pl_byte_t buf[16];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));

    bool ok = pl_encode_varint(&stream, 128);
    CHECK("varint(128) ok", ok);
    CHECK("varint(128) size=2", stream.bytes_written == 2);
    CHECK("varint(128) byte0=0x80", buf[0] == 0x80);
    CHECK("varint(128) byte1=0x01", buf[1] == 0x01);
}

static void test_varint_300(void)
{
    pl_byte_t buf[16];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));

    bool ok = pl_encode_varint(&stream, 300);
    CHECK("varint(300) ok", ok);
    CHECK("varint(300) size=2", stream.bytes_written == 2);
    CHECK("varint(300) byte0=0xAC", buf[0] == 0xAC);
    CHECK("varint(300) byte1=0x02", buf[1] == 0x02);
}

static void test_varint_large(void)
{
    pl_byte_t buf[16];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));

    bool ok = pl_encode_varint(&stream, 0xFFFFFFFF);
    CHECK("varint(0xFFFFFFFF) ok", ok);
    CHECK("varint(0xFFFFFFFF) size=5", stream.bytes_written == 5);
    CHECK("varint(0xFFFFFFFF) byte0=0xFF", buf[0] == 0xFF);
    CHECK("varint(0xFFFFFFFF) byte1=0xFF", buf[1] == 0xFF);
    CHECK("varint(0xFFFFFFFF) byte2=0xFF", buf[2] == 0xFF);
    CHECK("varint(0xFFFFFFFF) byte3=0xFF", buf[3] == 0xFF);
    CHECK("varint(0xFFFFFFFF) byte4=0x0F", buf[4] == 0x0F);
}

static void test_varint_max_uint64(void)
{
    pl_byte_t buf[16];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));

    uint64_t max_val = UINT64_MAX;
    bool ok = pl_encode_varint(&stream, max_val);
    CHECK("varint(UINT64_MAX) ok", ok);
    CHECK("varint(UINT64_MAX) size=10", stream.bytes_written == 10);
    for (int i = 0; i < 9; i++) {
        CHECK("varint(UINT64_MAX) byte all 0xFF", buf[i] == 0xFF);
    }
    CHECK("varint(UINT64_MAX) last byte=0x01", buf[9] == 0x01);
}

static void test_svarint_zero(void)
{
    pl_byte_t buf[16];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));

    bool ok = pl_encode_svarint(&stream, 0);
    CHECK("svarint(0) ok", ok);
    CHECK("svarint(0) val=0x00", buf[0] == 0x00);
}

static void test_svarint_neg_one(void)
{
    pl_byte_t buf[16];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));

    bool ok = pl_encode_svarint(&stream, -1);
    CHECK("svarint(-1) ok", ok);
    CHECK("svarint(-1) val=0x01", buf[0] == 0x01);
}

static void test_svarint_one(void)
{
    pl_byte_t buf[16];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));

    bool ok = pl_encode_svarint(&stream, 1);
    CHECK("svarint(1) ok", ok);
    CHECK("svarint(1) val=0x02", buf[0] == 0x02);
}

static void test_svarint_neg_two(void)
{
    pl_byte_t buf[16];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));

    bool ok = pl_encode_svarint(&stream, -2);
    CHECK("svarint(-2) ok", ok);
    CHECK("svarint(-2) val=0x03", buf[0] == 0x03);
}

static void test_svarint_two(void)
{
    pl_byte_t buf[16];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));

    bool ok = pl_encode_svarint(&stream, 2);
    CHECK("svarint(2) ok", ok);
    CHECK("svarint(2) val=0x04", buf[0] == 0x04);
}

static void test_fixed32(void)
{
    pl_byte_t buf[16];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));

    uint32_t val = 0x12345678;
    bool ok = pl_encode_fixed32(&stream, &val);
    CHECK("fixed32 ok", ok);
    CHECK("fixed32 size=4", stream.bytes_written == 4);
    CHECK("fixed32 byte0=0x78", buf[0] == 0x78);
    CHECK("fixed32 byte1=0x56", buf[1] == 0x56);
    CHECK("fixed32 byte2=0x34", buf[2] == 0x34);
    CHECK("fixed32 byte3=0x12", buf[3] == 0x12);
}

static void test_fixed64(void)
{
    pl_byte_t buf[16];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));

    uint64_t val = 0x0102030405060708ULL;
    bool ok = pl_encode_fixed64(&stream, &val);
    CHECK("fixed64 ok", ok);
    CHECK("fixed64 size=8", stream.bytes_written == 8);
    CHECK("fixed64 byte0=0x08", buf[0] == 0x08);
    CHECK("fixed64 byte1=0x07", buf[1] == 0x07);
    CHECK("fixed64 byte2=0x06", buf[2] == 0x06);
    CHECK("fixed64 byte3=0x05", buf[3] == 0x05);
    CHECK("fixed64 byte4=0x04", buf[4] == 0x04);
    CHECK("fixed64 byte5=0x03", buf[5] == 0x03);
    CHECK("fixed64 byte6=0x02", buf[6] == 0x02);
    CHECK("fixed64 byte7=0x01", buf[7] == 0x01);
}

static void test_encode_tag(void)
{
    pl_byte_t buf[16];
    pl_ostream_t stream;

    /* Field 1, VARINT wire type: (1 << 3) | 0 = 8 = 0x08 */
    stream = pl_ostream_from_buffer(buf, sizeof(buf));
    bool ok = pl_encode_tag(&stream, PL_WT_VARINT, 1);
    CHECK("tag(1,VARINT) ok", ok);
    CHECK("tag(1,VARINT) = 0x08", buf[0] == 0x08);

    /* Field 1, STRING wire type: (1 << 3) | 2 = 10 = 0x0A */
    stream = pl_ostream_from_buffer(buf, sizeof(buf));
    ok = pl_encode_tag(&stream, PL_WT_STRING, 1);
    CHECK("tag(1,STRING) ok", ok);
    CHECK("tag(1,STRING) = 0x0A", buf[0] == 0x0A);

    /* Field 2, VARINT wire type: (2 << 3) | 0 = 16 = 0x10 */
    stream = pl_ostream_from_buffer(buf, sizeof(buf));
    ok = pl_encode_tag(&stream, PL_WT_VARINT, 2);
    CHECK("tag(2,VARINT) ok", ok);
    CHECK("tag(2,VARINT) = 0x10", buf[0] == 0x10);

    /* Field 15, VARINT: (15 << 3) | 0 = 120 = 0x78 */
    stream = pl_ostream_from_buffer(buf, sizeof(buf));
    ok = pl_encode_tag(&stream, PL_WT_VARINT, 15);
    CHECK("tag(15,VARINT) ok", ok);
    CHECK("tag(15,VARINT) = 0x78", buf[0] == 0x78);

    /* Field 16, VARINT: (16 << 3) | 0 = 128 → 0x80 0x01 */
    stream = pl_ostream_from_buffer(buf, sizeof(buf));
    ok = pl_encode_tag(&stream, PL_WT_VARINT, 16);
    CHECK("tag(16,VARINT) ok", ok);
    CHECK("tag(16,VARINT) byte0=0x80", buf[0] == 0x80);
    CHECK("tag(16,VARINT) byte1=0x01", buf[1] == 0x01);

    /* Field 1, 32BIT wire type: (1 << 3) | 5 = 13 = 0x0D */
    stream = pl_ostream_from_buffer(buf, sizeof(buf));
    ok = pl_encode_tag(&stream, PL_WT_32BIT, 1);
    CHECK("tag(1,32BIT) ok", ok);
    CHECK("tag(1,32BIT) = 0x0D", buf[0] == 0x0D);

    /* Field 1, 64BIT wire type: (1 << 3) | 1 = 9 = 0x09 */
    stream = pl_ostream_from_buffer(buf, sizeof(buf));
    ok = pl_encode_tag(&stream, PL_WT_64BIT, 1);
    CHECK("tag(1,64BIT) ok", ok);
    CHECK("tag(1,64BIT) = 0x09", buf[0] == 0x09);
}

static void test_encode_string(void)
{
    pl_byte_t buf[64];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));

    const char *text = "Hello";
    bool ok = pl_encode_string(&stream, (const pl_byte_t*)text, 5);
    CHECK("string ok", ok);
    CHECK("string total bytes=6", stream.bytes_written == 6);
    CHECK("string length byte=0x05", buf[0] == 0x05);
    CHECK("string char 'H'", buf[1] == 'H');
    CHECK("string char 'e'", buf[2] == 'e');
    CHECK("string char 'l'", buf[3] == 'l');
    CHECK("string char 'l'", buf[4] == 'l');
    CHECK("string char 'o'", buf[5] == 'o');
}

static void test_encode_empty_string(void)
{
    pl_byte_t buf[16];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));

    bool ok = pl_encode_string(&stream, NULL, 0);
    CHECK("empty string ok", ok);
    CHECK("empty string total bytes=1", stream.bytes_written == 1);
    CHECK("empty string length byte=0x00", buf[0] == 0x00);
}

static void test_multiple_writes(void)
{
    pl_byte_t buf[64];
    pl_ostream_t stream = pl_ostream_from_buffer(buf, sizeof(buf));

    pl_encode_varint(&stream, 150);
    CHECK("150 encoded size=2", stream.bytes_written == 2);
    CHECK("150 byte0=0x96", buf[0] == 0x96);
    CHECK("150 byte1=0x01", buf[1] == 0x01);

    size_t prev_written = stream.bytes_written;
    pl_encode_fixed32(&stream, &(uint32_t){42});
    CHECK("additional fixed32 size=4", stream.bytes_written == prev_written + 4);
}

static void test_varint_known_values(void)
{
    pl_byte_t buf[16];
    pl_ostream_t stream;

    /* 150 = 0x96 0x01 (from protobuf spec) */
    stream = pl_ostream_from_buffer(buf, sizeof(buf));
    pl_encode_varint(&stream, 150);
    CHECK("varint(150) size=2", stream.bytes_written == 2);
    CHECK("varint(150) byte0=0x96", buf[0] == 0x96);
    CHECK("varint(150) byte1=0x01", buf[1] == 0x01);

    /* 10000 = 0x90 0x4E */
    stream = pl_ostream_from_buffer(buf, sizeof(buf));
    pl_encode_varint(&stream, 10000);
    CHECK("varint(10000) size=2", stream.bytes_written == 2);
    CHECK("varint(10000) byte0=0x90", buf[0] == 0x90);
    CHECK("varint(10000) byte1=0x4E", buf[1] == 0x4E);
}

int main(void)
{
    printf("=== Wire Format Encoding Tests ===\n");

    test_ostream_from_buffer();
    test_write_basic();
    test_write_overflow();
    test_write_empty();
    test_sizing_stream();
    test_varint_zero();
    test_varint_one();
    test_varint_127();
    test_varint_128();
    test_varint_300();
    test_varint_large();
    test_varint_max_uint64();
    test_svarint_zero();
    test_svarint_neg_one();
    test_svarint_one();
    test_svarint_neg_two();
    test_svarint_two();
    test_fixed32();
    test_fixed64();
    test_encode_tag();
    test_encode_string();
    test_encode_empty_string();
    test_multiple_writes();
    test_varint_known_values();

    printf("\nResults: %d passed, %d failed\n", tests_passed, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
