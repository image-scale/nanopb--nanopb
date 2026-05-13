#ifndef PROTOLITE_H_INCLUDED
#define PROTOLITE_H_INCLUDED

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>

#ifdef PL_ENABLE_MALLOC
#include <stdlib.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define PROTOLITE_VERSION "protolite-1.0.0"

#if defined(PL_NO_PACKED_STRUCTS)
#   define PL_PACKED_STRUCT_START
#   define PL_PACKED_STRUCT_END
#   define pl_packed
#elif defined(__GNUC__) || defined(__clang__)
#   define PL_PACKED_STRUCT_START
#   define PL_PACKED_STRUCT_END
#   define pl_packed __attribute__((packed))
#elif defined(_MSC_VER) && (_MSC_VER >= 1500)
#   define PL_PACKED_STRUCT_START __pragma(pack(push, 1))
#   define PL_PACKED_STRUCT_END __pragma(pack(pop))
#   define pl_packed
#else
#   define PL_PACKED_STRUCT_START
#   define PL_PACKED_STRUCT_END
#   define pl_packed
#endif

#ifndef PL_STATIC_ASSERT
#   if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#       define PL_STATIC_ASSERT(COND, MSG) _Static_assert(COND, #MSG);
#   elif defined(__cplusplus) && __cplusplus >= 201103L
#       define PL_STATIC_ASSERT(COND, MSG) static_assert(COND, #MSG);
#   else
#       define PL_STATIC_ASSERT(COND, MSG)
#   endif
#endif

#ifndef PL_UNUSED
#define PL_UNUSED(x) (void)(x)
#endif

#if !defined(CHAR_BIT) && defined(__CHAR_BIT__)
#define CHAR_BIT __CHAR_BIT__
#endif

#ifndef PL_LITTLE_ENDIAN_8BIT
#if ((defined(__BYTE_ORDER) && __BYTE_ORDER == __LITTLE_ENDIAN) || \
     (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) || \
      defined(__LITTLE_ENDIAN__) || defined(__ARMEL__) || \
      defined(__THUMBEL__) || defined(__AARCH64EL__) || defined(_MIPSEL) || \
      defined(_M_IX86) || defined(_M_X64) || defined(_M_ARM)) \
     && defined(CHAR_BIT) && CHAR_BIT == 8
#define PL_LITTLE_ENDIAN_8BIT 1
#endif
#endif

typedef uint8_t pl_byte_t;

typedef pl_byte_t pl_type_t;

#if defined(PL_FIELD_32BIT)
    typedef uint32_t pl_size_t;
    typedef int32_t pl_ssize_t;
#else
    typedef uint_least16_t pl_size_t;
    typedef int_least16_t pl_ssize_t;
#endif
#define PL_SIZE_MAX ((pl_size_t)-1)

typedef struct pl_istream_s pl_istream_t;
typedef struct pl_ostream_s pl_ostream_t;
typedef struct pl_field_cursor_s pl_field_cursor_t;

typedef struct pl_msg_descriptor_s pl_msg_descriptor_t;
struct pl_msg_descriptor_s {
    const uint32_t *field_info;
    const pl_msg_descriptor_t * const * submsg_info;
    const pl_byte_t *default_value;
    bool (*field_callback)(pl_istream_t *istream, pl_ostream_t *ostream, const pl_field_cursor_t *field);
    pl_size_t field_count;
    pl_size_t required_field_count;
    pl_size_t largest_tag;
};

struct pl_field_cursor_s {
    const pl_msg_descriptor_t *descriptor;
    void *message;

    pl_size_t index;
    pl_size_t field_info_index;
    pl_size_t required_field_index;
    pl_size_t submessage_index;

    pl_size_t tag;
    pl_size_t data_size;
    pl_size_t array_size;
    pl_type_t type;

    void *pField;
    void *pData;
    void *pSize;

    const pl_msg_descriptor_t *submsg_desc;
};

typedef pl_field_cursor_t pl_field_t;

#ifndef PL_WITHOUT_64BIT
PL_STATIC_ASSERT(sizeof(int64_t) == 2 * sizeof(int32_t), INT64_T_WRONG_SIZE)
PL_STATIC_ASSERT(sizeof(uint64_t) == 2 * sizeof(uint32_t), UINT64_T_WRONG_SIZE)
#endif

#define PL_BYTES_ARRAY_T(n) struct { pl_size_t size; pl_byte_t bytes[n]; }
#define PL_BYTES_ARRAY_T_ALLOCSIZE(n) ((size_t)n + offsetof(pl_bytes_array_t, bytes))

struct pl_bytes_array_s {
    pl_size_t size;
    pl_byte_t bytes[1];
};
typedef struct pl_bytes_array_s pl_bytes_array_t;

typedef struct pl_callback_s pl_callback_t;
struct pl_callback_s {
    union {
        bool (*decode)(pl_istream_t *stream, const pl_field_t *field, void **arg);
        bool (*encode)(pl_ostream_t *stream, const pl_field_t *field, void * const *arg);
    } funcs;
    void *arg;
};

extern bool pl_default_field_callback(pl_istream_t *istream, pl_ostream_t *ostream, const pl_field_t *field);

typedef enum {
    PL_WT_VARINT = 0,
    PL_WT_64BIT  = 1,
    PL_WT_STRING = 2,
    PL_WT_32BIT  = 5,
    PL_WT_PACKED = 255
} pl_wire_type_t;

typedef struct pl_extension_type_s pl_extension_type_t;
typedef struct pl_extension_s pl_extension_t;
struct pl_extension_type_s {
    bool (*decode)(pl_istream_t *stream, pl_extension_t *extension,
                   uint32_t tag, pl_wire_type_t wire_type);
    bool (*encode)(pl_ostream_t *stream, const pl_extension_t *extension);
    const void *arg;
};

struct pl_extension_s {
    const pl_extension_type_t *type;
    void *dest;
    pl_extension_t *next;
    bool found;
};

#define pl_extension_init_zero {NULL,NULL,NULL,false}

#ifdef PL_ENABLE_MALLOC
#   ifndef pl_realloc
#       define pl_realloc(ptr, size) realloc(ptr, size)
#   endif
#   ifndef pl_free
#       define pl_free(ptr) free(ptr)
#   endif
#endif

#define PL_PROTO_HEADER_VERSION 40

#define pl_membersize(st, m) (sizeof ((st*)0)->m)
#define pl_arraysize(st, m) (pl_membersize(st, m) / pl_membersize(st, m[0]))
#define pl_delta(st, m1, m2) ((int)offsetof(st, m1) - (int)offsetof(st, m2))

#define PL_EXPAND(x) x

/* Field data types - lowest 4 bits */
#define PL_DTYPE_BOOL       0x00U
#define PL_DTYPE_VARINT     0x01U
#define PL_DTYPE_UVARINT    0x02U
#define PL_DTYPE_SVARINT    0x03U
#define PL_DTYPE_FIXED32    0x04U
#define PL_DTYPE_FIXED64    0x05U
#define PL_DTYPE_LAST_PACKABLE 0x05U
#define PL_DTYPE_BYTES      0x06U
#define PL_DTYPE_STRING     0x07U
#define PL_DTYPE_SUBMESSAGE 0x08U
#define PL_DTYPE_SUBMSG_W_CB 0x09U
#define PL_DTYPE_EXTENSION  0x0AU
#define PL_DTYPE_FIXED_LENGTH_BYTES 0x0BU
#define PL_DTYPE_COUNT      0x0CU
#define PL_DTYPE_MASK       0x0FU

/* Field repetition rules - bits 4-5 */
#define PL_CARD_REQUIRED    0x00U
#define PL_CARD_OPTIONAL    0x10U
#define PL_CARD_SINGULAR    0x10U
#define PL_CARD_REPEATED    0x20U
#define PL_CARD_FIXARRAY    0x20U
#define PL_CARD_ONEOF       0x30U
#define PL_CARD_MASK        0x30U

/* Field allocation types - bits 6-7 */
#define PL_ALLOC_STATIC     0x00U
#define PL_ALLOC_POINTER    0x80U
#define PL_ALLOC_CALLBACK   0x40U
#define PL_ALLOC_MASK       0xC0U

#define PL_ALLOC(x) ((x) & PL_ALLOC_MASK)
#define PL_CARD(x)  ((x) & PL_CARD_MASK)
#define PL_DTYPE(x) ((x) & PL_DTYPE_MASK)
#define PL_DTYPE_IS_SUBMSG(x) (PL_DTYPE(x) == PL_DTYPE_SUBMESSAGE || \
                                PL_DTYPE(x) == PL_DTYPE_SUBMSG_W_CB)

#ifndef PL_MAX_REQUIRED_FIELDS
#define PL_MAX_REQUIRED_FIELDS 64
#endif

#if PL_MAX_REQUIRED_FIELDS < 64
#error PL_MAX_REQUIRED_FIELDS must be >= 64
#endif

/* PL_BIND macro and supporting macros for binding message field lists to C structs */
#define PL_BIND(msgname, structname, width) \
    const uint32_t structname ## _field_info[] = \
    { \
        msgname ## _FIELDLIST(PL_GEN_FIELD_INFO_ ## width, structname) \
        0 \
    }; \
    const pl_msg_descriptor_t* const structname ## _submsg_info[] = \
    { \
        msgname ## _FIELDLIST(PL_GEN_SUBMSG_INFO, structname) \
        NULL \
    }; \
    const pl_msg_descriptor_t structname ## _msg = \
    { \
       structname ## _field_info, \
       structname ## _submsg_info, \
       msgname ## _DEFAULT, \
       msgname ## _CALLBACK, \
       0 msgname ## _FIELDLIST(PL_GEN_FIELD_COUNT, structname), \
       0 msgname ## _FIELDLIST(PL_GEN_REQ_FIELD_COUNT, structname), \
       0 msgname ## _FIELDLIST(PL_GEN_LARGEST_TAG, structname), \
    };

#define PL_GEN_FIELD_COUNT(structname, atype, htype, ltype, fieldname, tag) +1
#define PL_GEN_REQ_FIELD_COUNT(structname, atype, htype, ltype, fieldname, tag) \
    + (PL_CARD_ ## htype == PL_CARD_REQUIRED)
#define PL_GEN_LARGEST_TAG(structname, atype, htype, ltype, fieldname, tag) \
    * 0 + tag

/* Field info generation macros for different widths */
#define PL_GEN_FIELD_INFO_1(structname, atype, htype, ltype, fieldname, tag) \
    PL_FIELDINFO_1(tag, PL_ALLOC_ ## atype | PL_CARD_ ## htype | PL_DTYPE_MAP_ ## ltype, \
                   PL_DATA_OFFSET_ ## atype(_PL_CARD_ ## htype, structname, fieldname), \
                   PL_DATA_SIZE_ ## atype(_PL_CARD_ ## htype, structname, fieldname), \
                   PL_SIZE_OFFSET_ ## atype(_PL_CARD_ ## htype, structname, fieldname), \
                   PL_ARRAY_SIZE_ ## atype(_PL_CARD_ ## htype, structname, fieldname))

#define PL_GEN_FIELD_INFO_2(structname, atype, htype, ltype, fieldname, tag) \
    PL_FIELDINFO_2(tag, PL_ALLOC_ ## atype | PL_CARD_ ## htype | PL_DTYPE_MAP_ ## ltype, \
                   PL_DATA_OFFSET_ ## atype(_PL_CARD_ ## htype, structname, fieldname), \
                   PL_DATA_SIZE_ ## atype(_PL_CARD_ ## htype, structname, fieldname), \
                   PL_SIZE_OFFSET_ ## atype(_PL_CARD_ ## htype, structname, fieldname), \
                   PL_ARRAY_SIZE_ ## atype(_PL_CARD_ ## htype, structname, fieldname))

#define PL_GEN_FIELD_INFO_4(structname, atype, htype, ltype, fieldname, tag) \
    PL_FIELDINFO_4(tag, PL_ALLOC_ ## atype | PL_CARD_ ## htype | PL_DTYPE_MAP_ ## ltype, \
                   PL_DATA_OFFSET_ ## atype(_PL_CARD_ ## htype, structname, fieldname), \
                   PL_DATA_SIZE_ ## atype(_PL_CARD_ ## htype, structname, fieldname), \
                   PL_SIZE_OFFSET_ ## atype(_PL_CARD_ ## htype, structname, fieldname), \
                   PL_ARRAY_SIZE_ ## atype(_PL_CARD_ ## htype, structname, fieldname))

#define PL_GEN_FIELD_INFO_8(structname, atype, htype, ltype, fieldname, tag) \
    PL_FIELDINFO_8(tag, PL_ALLOC_ ## atype | PL_CARD_ ## htype | PL_DTYPE_MAP_ ## ltype, \
                   PL_DATA_OFFSET_ ## atype(_PL_CARD_ ## htype, structname, fieldname), \
                   PL_DATA_SIZE_ ## atype(_PL_CARD_ ## htype, structname, fieldname), \
                   PL_SIZE_OFFSET_ ## atype(_PL_CARD_ ## htype, structname, fieldname), \
                   PL_ARRAY_SIZE_ ## atype(_PL_CARD_ ## htype, structname, fieldname))

#define PL_GEN_FIELD_INFO_AUTO(structname, atype, htype, ltype, fieldname, tag) \
    PL_FIELDINFO_AUTO2(PL_FIELDINFO_WIDTH_AUTO(_PL_ALLOC_ ## atype, _PL_CARD_ ## htype, _PL_DTYPE_ ## ltype), \
                   tag, PL_ALLOC_ ## atype | PL_CARD_ ## htype | PL_DTYPE_MAP_ ## ltype, \
                   PL_DATA_OFFSET_ ## atype(_PL_CARD_ ## htype, structname, fieldname), \
                   PL_DATA_SIZE_ ## atype(_PL_CARD_ ## htype, structname, fieldname), \
                   PL_SIZE_OFFSET_ ## atype(_PL_CARD_ ## htype, structname, fieldname), \
                   PL_ARRAY_SIZE_ ## atype(_PL_CARD_ ## htype, structname, fieldname))

#define PL_FIELDINFO_AUTO2(width, tag, type, data_offset, data_size, size_offset, array_size) \
    PL_FIELDINFO_AUTO3(width, tag, type, data_offset, data_size, size_offset, array_size)

#define PL_FIELDINFO_AUTO3(width, tag, type, data_offset, data_size, size_offset, array_size) \
    PL_FIELDINFO_ ## width(tag, type, data_offset, data_size, size_offset, array_size)

/* Submsg info generation */
#define PL_GEN_SUBMSG_INFO(structname, atype, htype, ltype, fieldname, tag) \
    PL_SUBMSG_INFO_ ## htype(_PL_DTYPE_ ## ltype, structname, fieldname)

#define PL_SUBMSG_INFO_REQUIRED(ltype, structname, fieldname) PL_SI ## ltype(structname ## _ ## fieldname ## _MSGTYPE)
#define PL_SUBMSG_INFO_SINGULAR(ltype, structname, fieldname) PL_SI ## ltype(structname ## _ ## fieldname ## _MSGTYPE)
#define PL_SUBMSG_INFO_OPTIONAL(ltype, structname, fieldname) PL_SI ## ltype(structname ## _ ## fieldname ## _MSGTYPE)
#define PL_SUBMSG_INFO_ONEOF(ltype, structname, fieldname) PL_SUBMSG_INFO_ONEOF2(ltype, structname, PL_ONEOF_NAME(UNION, fieldname), PL_ONEOF_NAME(MEMBER, fieldname))
#define PL_SUBMSG_INFO_ONEOF2(ltype, structname, unionname, membername) PL_SUBMSG_INFO_ONEOF3(ltype, structname, unionname, membername)
#define PL_SUBMSG_INFO_ONEOF3(ltype, structname, unionname, membername) PL_SI ## ltype(structname ## _ ## unionname ## _ ## membername ## _MSGTYPE)
#define PL_SUBMSG_INFO_REPEATED(ltype, structname, fieldname) PL_SI ## ltype(structname ## _ ## fieldname ## _MSGTYPE)
#define PL_SUBMSG_INFO_FIXARRAY(ltype, structname, fieldname) PL_SI ## ltype(structname ## _ ## fieldname ## _MSGTYPE)

#define PL_SI_PL_DTYPE_BOOL(t)
#define PL_SI_PL_DTYPE_BYTES(t)
#define PL_SI_PL_DTYPE_DOUBLE(t)
#define PL_SI_PL_DTYPE_ENUM(t)
#define PL_SI_PL_DTYPE_UENUM(t)
#define PL_SI_PL_DTYPE_FIXED32(t)
#define PL_SI_PL_DTYPE_FIXED64(t)
#define PL_SI_PL_DTYPE_FLOAT(t)
#define PL_SI_PL_DTYPE_INT32(t)
#define PL_SI_PL_DTYPE_INT64(t)
#define PL_SI_PL_DTYPE_MESSAGE(t)  PL_SUBMSG_DESCRIPTOR(t)
#define PL_SI_PL_DTYPE_MSG_W_CB(t) PL_SUBMSG_DESCRIPTOR(t)
#define PL_SI_PL_DTYPE_SFIXED32(t)
#define PL_SI_PL_DTYPE_SFIXED64(t)
#define PL_SI_PL_DTYPE_SINT32(t)
#define PL_SI_PL_DTYPE_SINT64(t)
#define PL_SI_PL_DTYPE_STRING(t)
#define PL_SI_PL_DTYPE_UINT32(t)
#define PL_SI_PL_DTYPE_UINT64(t)
#define PL_SI_PL_DTYPE_EXTENSION(t)
#define PL_SI_PL_DTYPE_FIXED_LENGTH_BYTES(t)
#define PL_SUBMSG_DESCRIPTOR(t)    &(t ## _msg),

/* Field info binary format:
 * 1 word:  [2-bit len=0] [6-bit tag] [8-bit type] [8-bit data_offset] [4-bit size_offset] [4-bit data_size]
 * 2 words: [2-bit len=1] [6-bit tag] [8-bit type] [12-bit array_size] [4-bit size_offset]
 *          [16-bit data_offset] [12-bit data_size] [4-bit tag>>6]
 * 4 words: [2-bit len=2] [6-bit tag] [8-bit type] [16-bit array_size]
 *          [8-bit size_offset] [24-bit tag>>6]
 *          [32-bit data_offset]
 *          [32-bit data_size]
 * 8 words: [2-bit len=3] [6-bit tag] [8-bit type] [16-bit reserved]
 *          [8-bit size_offset] [24-bit tag>>6]
 *          [32-bit data_offset]
 *          [32-bit data_size]
 *          [32-bit array_size]
 *          [32-bit reserved] x3
 */

#define PL_FIELDINFO_1(tag, type, data_offset, data_size, size_offset, array_size) \
    (0 | (((uint32_t)(tag) << 2) & 0xFF) | ((type) << 8) | (((uint32_t)(data_offset) & 0xFF) << 16) | \
     (((uint32_t)(size_offset) & 0x0F) << 24) | (((uint32_t)(data_size) & 0x0F) << 28)),

#define PL_FIELDINFO_2(tag, type, data_offset, data_size, size_offset, array_size) \
    (1 | (((uint32_t)(tag) << 2) & 0xFF) | ((type) << 8) | (((uint32_t)(array_size) & 0xFFF) << 16) | (((uint32_t)(size_offset) & 0x0F) << 28)), \
    (((uint32_t)(data_offset) & 0xFFFF) | (((uint32_t)(data_size) & 0xFFF) << 16) | (((uint32_t)(tag) & 0x3c0) << 22)),

#define PL_FIELDINFO_4(tag, type, data_offset, data_size, size_offset, array_size) \
    (2 | (((uint32_t)(tag) << 2) & 0xFF) | ((type) << 8) | (((uint32_t)(array_size) & 0xFFFF) << 16)), \
    ((uint32_t)(int_least8_t)(size_offset) | (((uint32_t)(tag) << 2) & 0xFFFFFF00)), \
    (data_offset), (data_size),

#define PL_FIELDINFO_8(tag, type, data_offset, data_size, size_offset, array_size) \
    (3 | (((uint32_t)(tag) << 2) & 0xFF) | ((type) << 8)), \
    ((uint32_t)(int_least8_t)(size_offset) | (((uint32_t)(tag) << 2) & 0xFFFFFF00)), \
    (data_offset), (data_size), (array_size), 0, 0, 0,

/* Auto-width selection */
#define PL_FIELDINFO_WIDTH_AUTO(atype, htype, ltype) PL_FI_WIDTH ## atype(htype, ltype)
#define PL_FI_WIDTH_PL_ALLOC_STATIC(htype, ltype)   PL_FI_WIDTH ## htype(ltype)
#define PL_FI_WIDTH_PL_ALLOC_POINTER(htype, ltype)  PL_FI_WIDTH ## htype(ltype)
#define PL_FI_WIDTH_PL_ALLOC_CALLBACK(htype, ltype) 2
#define PL_FI_WIDTH_PL_CARD_REQUIRED(ltype) PL_FI_WIDTH ## ltype
#define PL_FI_WIDTH_PL_CARD_SINGULAR(ltype) PL_FI_WIDTH ## ltype
#define PL_FI_WIDTH_PL_CARD_OPTIONAL(ltype) PL_FI_WIDTH ## ltype
#define PL_FI_WIDTH_PL_CARD_ONEOF(ltype)    PL_FI_WIDTH ## ltype
#define PL_FI_WIDTH_PL_CARD_REPEATED(ltype) 2
#define PL_FI_WIDTH_PL_CARD_FIXARRAY(ltype) 2
#define PL_FI_WIDTH_PL_DTYPE_BOOL      1
#define PL_FI_WIDTH_PL_DTYPE_BYTES     2
#define PL_FI_WIDTH_PL_DTYPE_DOUBLE    1
#define PL_FI_WIDTH_PL_DTYPE_ENUM      1
#define PL_FI_WIDTH_PL_DTYPE_UENUM     1
#define PL_FI_WIDTH_PL_DTYPE_FIXED32   1
#define PL_FI_WIDTH_PL_DTYPE_FIXED64   1
#define PL_FI_WIDTH_PL_DTYPE_FLOAT     1
#define PL_FI_WIDTH_PL_DTYPE_INT32     1
#define PL_FI_WIDTH_PL_DTYPE_INT64     1
#define PL_FI_WIDTH_PL_DTYPE_MESSAGE   2
#define PL_FI_WIDTH_PL_DTYPE_MSG_W_CB  2
#define PL_FI_WIDTH_PL_DTYPE_SFIXED32  1
#define PL_FI_WIDTH_PL_DTYPE_SFIXED64  1
#define PL_FI_WIDTH_PL_DTYPE_SINT32    1
#define PL_FI_WIDTH_PL_DTYPE_SINT64    1
#define PL_FI_WIDTH_PL_DTYPE_STRING    2
#define PL_FI_WIDTH_PL_DTYPE_UINT32    1
#define PL_FI_WIDTH_PL_DTYPE_UINT64    1
#define PL_FI_WIDTH_PL_DTYPE_EXTENSION 1
#define PL_FI_WIDTH_PL_DTYPE_FIXED_LENGTH_BYTES 2

/* Mapping from protobuf types to data types */
#define PL_DTYPE_MAP_BOOL               PL_DTYPE_BOOL
#define PL_DTYPE_MAP_BYTES              PL_DTYPE_BYTES
#define PL_DTYPE_MAP_DOUBLE             PL_DTYPE_FIXED64
#define PL_DTYPE_MAP_ENUM               PL_DTYPE_VARINT
#define PL_DTYPE_MAP_UENUM              PL_DTYPE_UVARINT
#define PL_DTYPE_MAP_FIXED32            PL_DTYPE_FIXED32
#define PL_DTYPE_MAP_FIXED64            PL_DTYPE_FIXED64
#define PL_DTYPE_MAP_FLOAT              PL_DTYPE_FIXED32
#define PL_DTYPE_MAP_INT32              PL_DTYPE_VARINT
#define PL_DTYPE_MAP_INT64              PL_DTYPE_VARINT
#define PL_DTYPE_MAP_MESSAGE            PL_DTYPE_SUBMESSAGE
#define PL_DTYPE_MAP_MSG_W_CB           PL_DTYPE_SUBMSG_W_CB
#define PL_DTYPE_MAP_SFIXED32           PL_DTYPE_FIXED32
#define PL_DTYPE_MAP_SFIXED64           PL_DTYPE_FIXED64
#define PL_DTYPE_MAP_SINT32             PL_DTYPE_SVARINT
#define PL_DTYPE_MAP_SINT64             PL_DTYPE_SVARINT
#define PL_DTYPE_MAP_STRING             PL_DTYPE_STRING
#define PL_DTYPE_MAP_UINT32             PL_DTYPE_UVARINT
#define PL_DTYPE_MAP_UINT64             PL_DTYPE_UVARINT
#define PL_DTYPE_MAP_EXTENSION          PL_DTYPE_EXTENSION
#define PL_DTYPE_MAP_FIXED_LENGTH_BYTES PL_DTYPE_FIXED_LENGTH_BYTES

/* Data offset macros */
#define PL_DATA_OFFSET_STATIC(htype, structname, fieldname)   PL_DO ## htype(structname, fieldname)
#define PL_DATA_OFFSET_POINTER(htype, structname, fieldname)  PL_DO ## htype(structname, fieldname)
#define PL_DATA_OFFSET_CALLBACK(htype, structname, fieldname) PL_DO ## htype(structname, fieldname)
#define PL_DO_PL_CARD_REQUIRED(structname, fieldname) offsetof(structname, fieldname)
#define PL_DO_PL_CARD_SINGULAR(structname, fieldname) offsetof(structname, fieldname)
#define PL_DO_PL_CARD_ONEOF(structname, fieldname) offsetof(structname, PL_ONEOF_NAME(FULL, fieldname))
#define PL_DO_PL_CARD_OPTIONAL(structname, fieldname) offsetof(structname, fieldname)
#define PL_DO_PL_CARD_REPEATED(structname, fieldname) offsetof(structname, fieldname)
#define PL_DO_PL_CARD_FIXARRAY(structname, fieldname) offsetof(structname, fieldname)

/* Size offset macros */
#define PL_SIZE_OFFSET_STATIC(htype, structname, fieldname)   PL_SO ## htype(structname, fieldname)
#define PL_SIZE_OFFSET_POINTER(htype, structname, fieldname)  PL_SO_PTR ## htype(structname, fieldname)
#define PL_SIZE_OFFSET_CALLBACK(htype, structname, fieldname) PL_SO_CB ## htype(structname, fieldname)
#define PL_SO_PL_CARD_REQUIRED(structname, fieldname) 0
#define PL_SO_PL_CARD_SINGULAR(structname, fieldname) 0
#define PL_SO_PL_CARD_ONEOF(structname, fieldname) PL_SO_PL_CARD_ONEOF2(structname, PL_ONEOF_NAME(FULL, fieldname), PL_ONEOF_NAME(UNION, fieldname))
#define PL_SO_PL_CARD_ONEOF2(structname, fullname, unionname) PL_SO_PL_CARD_ONEOF3(structname, fullname, unionname)
#define PL_SO_PL_CARD_ONEOF3(structname, fullname, unionname) pl_delta(structname, fullname, which_ ## unionname)
#define PL_SO_PL_CARD_OPTIONAL(structname, fieldname) pl_delta(structname, fieldname, has_ ## fieldname)
#define PL_SO_PL_CARD_REPEATED(structname, fieldname) pl_delta(structname, fieldname, fieldname ## _count)
#define PL_SO_PL_CARD_FIXARRAY(structname, fieldname) 0
#define PL_SO_PTR_PL_CARD_REQUIRED(structname, fieldname) 0
#define PL_SO_PTR_PL_CARD_SINGULAR(structname, fieldname) 0
#define PL_SO_PTR_PL_CARD_ONEOF(structname, fieldname) PL_SO_PL_CARD_ONEOF(structname, fieldname)
#define PL_SO_PTR_PL_CARD_OPTIONAL(structname, fieldname) 0
#define PL_SO_PTR_PL_CARD_REPEATED(structname, fieldname) PL_SO_PL_CARD_REPEATED(structname, fieldname)
#define PL_SO_PTR_PL_CARD_FIXARRAY(structname, fieldname) 0
#define PL_SO_CB_PL_CARD_REQUIRED(structname, fieldname) 0
#define PL_SO_CB_PL_CARD_SINGULAR(structname, fieldname) 0
#define PL_SO_CB_PL_CARD_ONEOF(structname, fieldname) PL_SO_PL_CARD_ONEOF(structname, fieldname)
#define PL_SO_CB_PL_CARD_OPTIONAL(structname, fieldname) 0
#define PL_SO_CB_PL_CARD_REPEATED(structname, fieldname) 0
#define PL_SO_CB_PL_CARD_FIXARRAY(structname, fieldname) 0

/* Array size macros */
#define PL_ARRAY_SIZE_STATIC(htype, structname, fieldname)   PL_AS ## htype(structname, fieldname)
#define PL_ARRAY_SIZE_POINTER(htype, structname, fieldname)  PL_AS_PTR ## htype(structname, fieldname)
#define PL_ARRAY_SIZE_CALLBACK(htype, structname, fieldname) 1
#define PL_AS_PL_CARD_REQUIRED(structname, fieldname) 1
#define PL_AS_PL_CARD_SINGULAR(structname, fieldname) 1
#define PL_AS_PL_CARD_OPTIONAL(structname, fieldname) 1
#define PL_AS_PL_CARD_ONEOF(structname, fieldname) 1
#define PL_AS_PL_CARD_REPEATED(structname, fieldname) pl_arraysize(structname, fieldname)
#define PL_AS_PL_CARD_FIXARRAY(structname, fieldname) pl_arraysize(structname, fieldname)
#define PL_AS_PTR_PL_CARD_REQUIRED(structname, fieldname) 1
#define PL_AS_PTR_PL_CARD_SINGULAR(structname, fieldname) 1
#define PL_AS_PTR_PL_CARD_OPTIONAL(structname, fieldname) 1
#define PL_AS_PTR_PL_CARD_ONEOF(structname, fieldname) 1
#define PL_AS_PTR_PL_CARD_REPEATED(structname, fieldname) 1
#define PL_AS_PTR_PL_CARD_FIXARRAY(structname, fieldname) pl_arraysize(structname, fieldname[0])

/* Data size macros */
#define PL_DATA_SIZE_STATIC(htype, structname, fieldname)   PL_DS ## htype(structname, fieldname)
#define PL_DATA_SIZE_POINTER(htype, structname, fieldname)  PL_DS_PTR ## htype(structname, fieldname)
#define PL_DATA_SIZE_CALLBACK(htype, structname, fieldname) PL_DS_CB ## htype(structname, fieldname)
#define PL_DS_PL_CARD_REQUIRED(structname, fieldname) pl_membersize(structname, fieldname)
#define PL_DS_PL_CARD_SINGULAR(structname, fieldname) pl_membersize(structname, fieldname)
#define PL_DS_PL_CARD_OPTIONAL(structname, fieldname) pl_membersize(structname, fieldname)
#define PL_DS_PL_CARD_ONEOF(structname, fieldname) pl_membersize(structname, PL_ONEOF_NAME(FULL, fieldname))
#define PL_DS_PL_CARD_REPEATED(structname, fieldname) pl_membersize(structname, fieldname[0])
#define PL_DS_PL_CARD_FIXARRAY(structname, fieldname) pl_membersize(structname, fieldname[0])
#define PL_DS_PTR_PL_CARD_REQUIRED(structname, fieldname) pl_membersize(structname, fieldname[0])
#define PL_DS_PTR_PL_CARD_SINGULAR(structname, fieldname) pl_membersize(structname, fieldname[0])
#define PL_DS_PTR_PL_CARD_OPTIONAL(structname, fieldname) pl_membersize(structname, fieldname[0])
#define PL_DS_PTR_PL_CARD_ONEOF(structname, fieldname) pl_membersize(structname, PL_ONEOF_NAME(FULL, fieldname)[0])
#define PL_DS_PTR_PL_CARD_REPEATED(structname, fieldname) pl_membersize(structname, fieldname[0])
#define PL_DS_PTR_PL_CARD_FIXARRAY(structname, fieldname) pl_membersize(structname, fieldname[0][0])
#define PL_DS_CB_PL_CARD_REQUIRED(structname, fieldname) pl_membersize(structname, fieldname)
#define PL_DS_CB_PL_CARD_SINGULAR(structname, fieldname) pl_membersize(structname, fieldname)
#define PL_DS_CB_PL_CARD_OPTIONAL(structname, fieldname) pl_membersize(structname, fieldname)
#define PL_DS_CB_PL_CARD_ONEOF(structname, fieldname) pl_membersize(structname, PL_ONEOF_NAME(FULL, fieldname))
#define PL_DS_CB_PL_CARD_REPEATED(structname, fieldname) pl_membersize(structname, fieldname)
#define PL_DS_CB_PL_CARD_FIXARRAY(structname, fieldname) pl_membersize(structname, fieldname)

#define PL_ONEOF_NAME(type, tuple) PL_EXPAND(PL_ONEOF_NAME_ ## type tuple)
#define PL_ONEOF_NAME_UNION(unionname,membername,fullname) unionname
#define PL_ONEOF_NAME_MEMBER(unionname,membername,fullname) membername
#define PL_ONEOF_NAME_FULL(unionname,membername,fullname) fullname

/* Error handling macros */
#ifdef PL_NO_ERRMSG
#define PL_SET_ERROR(stream, msg) PL_UNUSED(stream)
#define PL_GET_ERROR(stream) "(errmsg disabled)"
#else
#define PL_SET_ERROR(stream, msg) (stream->errmsg = (stream)->errmsg ? (stream)->errmsg : (msg))
#define PL_GET_ERROR(stream) ((stream)->errmsg ? (stream)->errmsg : "(none)")
#endif

#define PL_RETURN_ERROR(stream, msg) return PL_SET_ERROR(stream, msg), false

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
