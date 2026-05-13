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
- [ ] Input stream struct (pl_istream_t) with callback, state, bytes_left, errmsg fields
- [ ] pl_istream_from_buffer() creates a buffer-backed input stream
- [ ] pl_read() reads bytes from stream, decrements bytes_left, returns false on underflow
- [ ] pl_decode_varint() decodes unsigned 64-bit varint from stream (0x00→0, 0x7F→127, 0x80 0x01→128, 0xAC 0x02→300)
- [ ] pl_decode_varint32() decodes unsigned 32-bit varint from stream with overflow checking
- [ ] pl_decode_svarint() decodes zigzag-encoded signed varint (0x01→-1, 0x02→1, 0x03→-2)
- [ ] pl_decode_bool() decodes varint as boolean (0→false, non-zero→true)
- [ ] pl_decode_fixed32() reads 4 bytes in little-endian order and reassembles uint32_t
- [ ] pl_decode_fixed64() reads 8 bytes in little-endian order and reassembles uint64_t
- [ ] pl_decode_tag() reads a varint and splits into wire_type (low 3 bits) and tag (remaining bits), returns eof when stream empty
- [ ] pl_skip_field() skips field data based on wire type (varint, 32-bit, 64-bit, length-delimited)
- [ ] pl_make_string_substream() creates a length-limited substream for reading length-delimited fields
- [ ] pl_close_string_substream() closes substream and advances parent stream past consumed bytes
- [ ] Round-trip: encoding then decoding a value produces the original value for all types
