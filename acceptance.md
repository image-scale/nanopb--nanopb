# Acceptance Criteria

## Task 1: Protobuf wire format encoding primitives with output stream

### Acceptance Criteria
- [x] Core types defined: pl_byte_t (uint8), pl_size_t (uint16 or uint32), pl_wire_type_t enum (VARINT=0, BITS64=1, LENGTH_DELIMITED=2, BITS32=5)
- [x] Output stream struct (pl_ostream_t) with callback, state, max_size, bytes_written, errmsg fields
- [x] pl_ostream_from_buffer() creates a buffer-backed output stream
- [x] pl_write() writes bytes to stream and tracks bytes_written, returns false on overflow
- [x] pl_encode_varint() encodes unsigned 64-bit integers in varint format
- [x] pl_encode_svarint() encodes signed integers using zigzag encoding
- [x] pl_encode_fixed32() writes 4 bytes in little-endian order
- [x] pl_encode_fixed64() writes 8 bytes in little-endian order
- [x] pl_encode_tag() encodes field tag as (field_number << 3 | wire_type) varint
- [x] pl_encode_string() writes length-prefixed bytes (varint length + raw bytes)
- [x] Sizing stream: when callback is NULL, pl_write counts bytes but doesn't write
- [x] Stream overflow: writing past max_size returns false and sets error message

## Task 2: Protobuf wire format decoding primitives with input stream

### Acceptance Criteria
- [x] Input stream struct (pl_istream_t) with callback, state, bytes_left, errmsg fields
- [x] pl_istream_from_buffer() creates a buffer-backed input stream
- [x] pl_read() reads bytes from stream, decrements bytes_left, returns false on underflow
- [x] pl_decode_varint() decodes unsigned 64-bit varint from stream (0x00→0, 0x7F→127, 0x80 0x01→128, 0xAC 0x02→300)
- [x] pl_decode_varint32() decodes unsigned 32-bit varint from stream with overflow checking
- [x] pl_decode_svarint() decodes zigzag-encoded signed varint (0x01→-1, 0x02→1, 0x03→-2)
- [x] pl_decode_bool() decodes varint as boolean (0→false, non-zero→true)
- [x] pl_decode_fixed32() reads 4 bytes in little-endian order and reassembles uint32_t
- [x] pl_decode_fixed64() reads 8 bytes in little-endian order and reassembles uint64_t
- [x] pl_decode_tag() reads a varint and splits into wire_type (low 3 bits) and tag (remaining bits), returns eof when stream empty
- [x] pl_skip_field() skips field data based on wire type (varint, 32-bit, 64-bit, length-delimited)
- [x] pl_make_string_substream() creates a length-limited substream for reading length-delimited fields
- [x] pl_close_string_substream() closes substream and advances parent stream past consumed bytes
- [x] Round-trip: encoding then decoding a value produces the original value for all types

## Task 3: Message descriptor system with field iteration (DONE)

### Acceptance Criteria
- [x] pl_field_cursor_begin() initializes a field cursor to the first field of a message descriptor and loads field metadata (tag, type, data_size, etc.)
- [x] pl_field_cursor_next() advances the cursor to the next field and returns false when wrapping back to the start
- [x] pl_field_cursor_find() locates a field by tag number, returning true if found and positioning the cursor on it
- [x] 1-word field descriptor format correctly encodes/decodes tag (6 bits), type (8 bits), data_offset (8 bits), size_offset (4 bits), data_size (4 bits)
- [x] 2-word field descriptor format correctly handles larger data_offset (16 bits), data_size (12 bits), array_size (12 bits), extended tag (10 bits)
- [x] 4-word field descriptor format correctly handles 32-bit data_offset, data_size, 16-bit array_size, 30-bit tag
- [x] Field cursor correctly computes pField, pData, and pSize pointers from a struct base address and field descriptor offsets
- [x] For submessage fields, the cursor loads the submsg_desc pointer from the submsg_info array
- [x] PL_BIND macro generates correct field_info and submsg_info arrays from a FIELDLIST macro definition
- [x] Field iterator works with a message containing required, optional, and repeated fields

## Task 4: Automatic message encoding

### Acceptance Criteria
- [x] pl_encode_message() iterates all fields using pl_field_cursor and encodes each non-default field
- [x] Required int32 field encodes correctly as tag varint + value varint
- [x] Optional field with has_xxx=true encodes; has_xxx=false skips the field
- [x] Repeated packable field encodes as packed array (tag + length + concatenated values)
- [x] String field encodes as tag + varint length + UTF-8 bytes
- [x] Bytes field (pl_bytes_array_t) encodes as tag + varint length + raw bytes
- [x] Nested submessage encodes as tag + varint length + recursively encoded submessage
- [x] Bool field encodes as tag + varint (0 or 1)
- [x] Fixed32/float field encodes as tag + 4 little-endian bytes
- [x] Signed varint (svarint/sint32) field encodes with zigzag encoding
- [x] Proto3 singular fields (OPTIONAL without has_field) skip encoding when value is zero/empty
- [x] pl_encode_message_ex with PL_ENCODE_DELIMITED wraps message in length-delimited framing
- [x] pl_encode_message_ex with PL_ENCODE_NULLTERMINATED appends a zero byte after the message
- [x] pl_get_encoded_size returns the correct byte count matching actual encoded output
- [x] Empty message (zero fields) encodes to zero bytes successfully
- [x] Oneof field encodes only when the which_tag selector matches the field tag

## Task 5: Automatic message decoding with required field validation

### Acceptance Criteria
- [x] pl_decode_message() reads wire format tags, finds matching fields, and decodes each value into the struct
- [x] Required int32 field decodes correctly from tag+varint wire format
- [x] Optional field sets has_xxx=true when present; field remains at default when absent
- [x] Repeated packed array decodes from length-delimited wire format into array with count
- [x] String field decodes from tag+length+bytes into null-terminated char array
- [x] Bytes field decodes into pl_bytes_array_t with size and data
- [x] Nested submessage decodes recursively from tag+length+submessage wire data
- [x] Bool field decodes from varint (0→false, non-zero→true)
- [x] Fixed32/float field decodes from 4 little-endian bytes
- [x] Signed varint (svarint) decodes with zigzag decoding
- [x] Missing required field causes pl_decode_message to return false with error
- [x] Unknown fields (unrecognized tags) are silently skipped
- [x] pl_decode_message_ex with PL_DECODE_DELIMITED reads length prefix then decodes
- [x] pl_decode_message_ex with PL_DECODE_NULLTERMINATED stops at zero tag byte
- [x] Struct fields are initialized to defaults before decoding (unless PL_DECODE_NOINIT)
- [x] Oneof field sets which_tag and decodes the value
- [x] Full round-trip: encode then decode produces identical struct contents
