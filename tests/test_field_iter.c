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

/* === Test message 1: Simple struct with int32 + string (1-word + 2-word descriptors) === */
typedef struct {
    int32_t id;
    char name[32];
} PersonMsg;

static const uint32_t PersonMsg_field_info[] = {
    /* Field 1: id (int32, required, static) - 1-word format */
    PL_FIELDINFO_1(1, PL_ALLOC_STATIC | PL_CARD_REQUIRED | PL_DTYPE_VARINT,
                   offsetof(PersonMsg, id), sizeof(int32_t), 0, 1)
    /* Field 2: name (string, required, static) - 2-word format needed for string */
    PL_FIELDINFO_2(2, PL_ALLOC_STATIC | PL_CARD_REQUIRED | PL_DTYPE_STRING,
                   offsetof(PersonMsg, name), sizeof(((PersonMsg*)0)->name), 0, 1)
    0
};

static const pl_msg_descriptor_t *const PersonMsg_submsg_info[] = { NULL };

static const pl_msg_descriptor_t PersonMsg_msg = {
    PersonMsg_field_info,
    PersonMsg_submsg_info,
    NULL,
    NULL,
    2,
    2,
    2
};

static void test_basic_iteration(void)
{
    PersonMsg person;
    memset(&person, 0, sizeof(person));
    person.id = 42;
    strcpy(person.name, "Alice");

    pl_field_cursor_t cursor;
    bool ok = pl_field_cursor_begin(&cursor, &PersonMsg_msg, &person);
    CHECK("begin returns true", ok);
    CHECK("first field tag=1", cursor.tag == 1);
    CHECK("first field type VARINT", PL_DTYPE(cursor.type) == PL_DTYPE_VARINT);
    CHECK("first field alloc STATIC", PL_ALLOC(cursor.type) == PL_ALLOC_STATIC);
    CHECK("first field card REQUIRED", PL_CARD(cursor.type) == PL_CARD_REQUIRED);
    CHECK("first field data_size=4", cursor.data_size == sizeof(int32_t));
    CHECK("first field pData points to id", cursor.pData == &person.id);
    CHECK("first field pData value=42", *(int32_t*)cursor.pData == 42);
    CHECK("first field no submsg", cursor.submsg_desc == NULL);

    ok = pl_field_cursor_next(&cursor);
    CHECK("next returns true", ok);
    CHECK("second field tag=2", cursor.tag == 2);
    CHECK("second field type STRING", PL_DTYPE(cursor.type) == PL_DTYPE_STRING);
    CHECK("second field data_size=32", cursor.data_size == 32);
    CHECK("second field pData points to name", cursor.pData == person.name);
    CHECK("second field pData starts with 'A'", *(char*)cursor.pData == 'A');

    ok = pl_field_cursor_next(&cursor);
    CHECK("next wraps returns false", !ok);
    CHECK("wrapped back to index 0", cursor.index == 0);
}

static void test_find_by_tag(void)
{
    PersonMsg person;
    memset(&person, 0, sizeof(person));
    person.id = 99;
    strcpy(person.name, "Bob");

    pl_field_cursor_t cursor;
    pl_field_cursor_begin(&cursor, &PersonMsg_msg, &person);

    bool found = pl_field_cursor_find(&cursor, 2);
    CHECK("find tag 2", found);
    CHECK("found field tag=2", cursor.tag == 2);
    CHECK("found field type STRING", PL_DTYPE(cursor.type) == PL_DTYPE_STRING);

    found = pl_field_cursor_find(&cursor, 1);
    CHECK("find tag 1 (backward)", found);
    CHECK("found field tag=1", cursor.tag == 1);

    found = pl_field_cursor_find(&cursor, 99);
    CHECK("find nonexistent tag returns false", !found);
}

/* === Test message 2: Optional field with has_xxx === */
typedef struct {
    bool has_temperature;
    float temperature;
    int32_t humidity;
} WeatherMsg;

static const uint32_t WeatherMsg_field_info[] = {
    /* Field 1: temperature (float/fixed32, optional, static) - 1-word */
    PL_FIELDINFO_1(1, PL_ALLOC_STATIC | PL_CARD_OPTIONAL | PL_DTYPE_FIXED32,
                   offsetof(WeatherMsg, temperature), sizeof(float),
                   pl_delta(WeatherMsg, temperature, has_temperature), 1)
    /* Field 2: humidity (int32, required, static) - 1-word */
    PL_FIELDINFO_1(2, PL_ALLOC_STATIC | PL_CARD_REQUIRED | PL_DTYPE_VARINT,
                   offsetof(WeatherMsg, humidity), sizeof(int32_t), 0, 1)
    0
};

static const pl_msg_descriptor_t *const WeatherMsg_submsg_info[] = { NULL };

static const pl_msg_descriptor_t WeatherMsg_msg = {
    WeatherMsg_field_info,
    WeatherMsg_submsg_info,
    NULL,
    NULL,
    2,
    1,
    2
};

static void test_optional_field(void)
{
    WeatherMsg w;
    memset(&w, 0, sizeof(w));
    w.has_temperature = true;
    w.temperature = 23.5f;
    w.humidity = 65;

    pl_field_cursor_t cursor;
    pl_field_cursor_begin(&cursor, &WeatherMsg_msg, &w);

    CHECK("opt field tag=1", cursor.tag == 1);
    CHECK("opt field card OPTIONAL", PL_CARD(cursor.type) == PL_CARD_OPTIONAL);
    CHECK("opt field pSize points to has_temperature", cursor.pSize == &w.has_temperature);
    CHECK("opt field pData points to temperature", cursor.pData == &w.temperature);

    pl_field_cursor_next(&cursor);
    CHECK("req field tag=2", cursor.tag == 2);
    CHECK("req field card REQUIRED", PL_CARD(cursor.type) == PL_CARD_REQUIRED);
    CHECK("req field pSize is NULL", cursor.pSize == NULL);
}

/* === Test message 3: Repeated field with count === */
typedef struct {
    pl_size_t scores_count;
    int32_t scores[10];
} ScoresMsg;

static const uint32_t ScoresMsg_field_info[] = {
    /* Field 1: scores (int32, repeated, static) - 2-word format for arrays */
    PL_FIELDINFO_2(1, PL_ALLOC_STATIC | PL_CARD_REPEATED | PL_DTYPE_VARINT,
                   offsetof(ScoresMsg, scores), sizeof(int32_t),
                   pl_delta(ScoresMsg, scores, scores_count), 10)
    0
};

static const pl_msg_descriptor_t *const ScoresMsg_submsg_info[] = { NULL };

static const pl_msg_descriptor_t ScoresMsg_msg = {
    ScoresMsg_field_info,
    ScoresMsg_submsg_info,
    NULL,
    NULL,
    1,
    0,
    1
};

static void test_repeated_field(void)
{
    ScoresMsg s;
    memset(&s, 0, sizeof(s));
    s.scores_count = 3;
    s.scores[0] = 100;
    s.scores[1] = 95;
    s.scores[2] = 88;

    pl_field_cursor_t cursor;
    pl_field_cursor_begin(&cursor, &ScoresMsg_msg, &s);

    CHECK("rep field tag=1", cursor.tag == 1);
    CHECK("rep field card REPEATED", PL_CARD(cursor.type) == PL_CARD_REPEATED);
    CHECK("rep field array_size=10", cursor.array_size == 10);
    CHECK("rep field pSize points to count", cursor.pSize == &s.scores_count);
    CHECK("rep field pData points to array", cursor.pData == s.scores);
    CHECK("rep field data_size=4", cursor.data_size == sizeof(int32_t));
}

/* === Test message 4: Nested submessage === */
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
    PointMsg_field_info,
    PointMsg_submsg_info,
    NULL,
    NULL,
    2,
    2,
    2
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
    &PointMsg_msg,
    NULL
};

static const pl_msg_descriptor_t PlaceMsg_msg = {
    PlaceMsg_field_info,
    PlaceMsg_submsg_info,
    NULL,
    NULL,
    2,
    2,
    2
};

static void test_submessage_field(void)
{
    PlaceMsg place;
    memset(&place, 0, sizeof(place));
    place.id = 7;
    place.location.x = 10;
    place.location.y = 20;

    pl_field_cursor_t cursor;
    pl_field_cursor_begin(&cursor, &PlaceMsg_msg, &place);

    CHECK("place field 1 tag=1", cursor.tag == 1);
    CHECK("place field 1 dtype VARINT", PL_DTYPE(cursor.type) == PL_DTYPE_VARINT);
    CHECK("place field 1 no submsg", cursor.submsg_desc == NULL);

    pl_field_cursor_next(&cursor);
    CHECK("place field 2 tag=2", cursor.tag == 2);
    CHECK("place field 2 dtype SUBMESSAGE", PL_DTYPE(cursor.type) == PL_DTYPE_SUBMESSAGE);
    CHECK("place field 2 has submsg_desc", cursor.submsg_desc != NULL);
    CHECK("place field 2 submsg_desc is PointMsg", cursor.submsg_desc == &PointMsg_msg);
    CHECK("place field 2 pData points to location", cursor.pData == &place.location);
}

/* === Test message 5: PL_BIND macro === */
typedef struct {
    int32_t value;
    bool has_label;
    char label[16];
} BindTestMsg;

#define BindTestMsg_FIELDLIST(X, a) \
    X(a, STATIC, REQUIRED, INT32, value, 1) \
    X(a, STATIC, OPTIONAL, STRING, label, 2)

#define BindTestMsg_DEFAULT NULL
#define BindTestMsg_CALLBACK NULL

PL_BIND(BindTestMsg, BindTestMsg, AUTO)

static void test_pl_bind_macro(void)
{
    BindTestMsg msg;
    memset(&msg, 0, sizeof(msg));
    msg.value = 123;
    msg.has_label = true;
    strcpy(msg.label, "test");

    CHECK("bind msg field_count=2", BindTestMsg_msg.field_count == 2);
    CHECK("bind msg required_count=1", BindTestMsg_msg.required_field_count == 1);
    CHECK("bind msg largest_tag=2", BindTestMsg_msg.largest_tag == 2);

    pl_field_cursor_t cursor;
    bool ok = pl_field_cursor_begin(&cursor, &BindTestMsg_msg, &msg);
    CHECK("bind begin ok", ok);
    CHECK("bind field 1 tag=1", cursor.tag == 1);
    CHECK("bind field 1 pData=&value", cursor.pData == &msg.value);
    CHECK("bind field 1 value=123", *(int32_t*)cursor.pData == 123);

    pl_field_cursor_next(&cursor);
    CHECK("bind field 2 tag=2", cursor.tag == 2);
    CHECK("bind field 2 pData=label", cursor.pData == msg.label);
    CHECK("bind field 2 pSize=&has_label", cursor.pSize == &msg.has_label);
}

/* === Test message 6: 4-word format for large offsets === */
typedef struct {
    char padding[300];
    int32_t far_field;
} LargeOffsetMsg;

static const uint32_t LargeOffsetMsg_field_info[] = {
    PL_FIELDINFO_4(1, PL_ALLOC_STATIC | PL_CARD_REQUIRED | PL_DTYPE_VARINT,
                   offsetof(LargeOffsetMsg, far_field), sizeof(int32_t), 0, 1)
    0
};

static const pl_msg_descriptor_t *const LargeOffsetMsg_submsg_info[] = { NULL };

static const pl_msg_descriptor_t LargeOffsetMsg_msg = {
    LargeOffsetMsg_field_info,
    LargeOffsetMsg_submsg_info,
    NULL,
    NULL,
    1,
    1,
    1
};

static void test_4word_descriptor(void)
{
    LargeOffsetMsg msg;
    memset(&msg, 0, sizeof(msg));
    msg.far_field = 999;

    pl_field_cursor_t cursor;
    bool ok = pl_field_cursor_begin(&cursor, &LargeOffsetMsg_msg, &msg);
    CHECK("4word begin ok", ok);
    CHECK("4word tag=1", cursor.tag == 1);
    CHECK("4word pData points to far_field", cursor.pData == &msg.far_field);
    CHECK("4word value=999", *(int32_t*)cursor.pData == 999);
    CHECK("4word data_size=4", cursor.data_size == sizeof(int32_t));
}

/* === Test message 7: NULL message pointer (for sizing) === */
static void test_null_message(void)
{
    pl_field_cursor_t cursor;
    bool ok = pl_field_cursor_begin(&cursor, &PersonMsg_msg, NULL);
    CHECK("null msg begin ok", ok);
    CHECK("null msg tag=1", cursor.tag == 1);
    CHECK("null msg pField is NULL", cursor.pField == NULL);
    CHECK("null msg pData is NULL", cursor.pData == NULL);
    CHECK("null msg pSize is NULL", cursor.pSize == NULL);
}

/* === Test: empty message (zero field_count) === */
static const uint32_t EmptyMsg_field_info[] = { 0 };
static const pl_msg_descriptor_t *const EmptyMsg_submsg_info[] = { NULL };
static const pl_msg_descriptor_t EmptyMsg_msg = {
    EmptyMsg_field_info,
    EmptyMsg_submsg_info,
    NULL,
    NULL,
    0,
    0,
    0
};

static void test_empty_message(void)
{
    int dummy;
    pl_field_cursor_t cursor;
    bool ok = pl_field_cursor_begin(&cursor, &EmptyMsg_msg, &dummy);
    CHECK("empty msg begin returns false", !ok);
}

/* === Test: const begin === */
static void test_const_begin(void)
{
    const PersonMsg person = {42, "Charlie"};

    pl_field_cursor_t cursor;
    bool ok = pl_field_cursor_begin_const(&cursor, &PersonMsg_msg, &person);
    CHECK("const begin ok", ok);
    CHECK("const tag=1", cursor.tag == 1);
    CHECK("const pData value=42", *(int32_t*)cursor.pData == 42);
}

int main(void)
{
    printf("=== Field Iteration Tests ===\n");

    test_basic_iteration();
    test_find_by_tag();
    test_optional_field();
    test_repeated_field();
    test_submessage_field();
    test_pl_bind_macro();
    test_4word_descriptor();
    test_null_message();
    test_empty_message();
    test_const_begin();

    printf("\nResults: %d passed, %d failed\n", tests_passed, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
