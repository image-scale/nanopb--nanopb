# Acceptance Criteria

## Task 1: Protobuf wire format encoding primitives with output stream

### Acceptance Criteria
- [ ] Core types defined: pl_byte_t (uint8), pl_size_t (uint16 or uint32), pl_wire_type_t enum (VARINT=0, BITS64=1, LENGTH_DELIMITED=2, BITS32=5)
- [ ] Output stream struct (pl_ostream_t) with callback, state, max_size, bytes_written, errmsg fields
- [ ] pl_ostream_from_buffer() creates a buffer-backed output stream
- [ ] pl_write() writes bytes to stream and tracks bytes_written, returns false on overflow
- [ ] pl_encode_varint() encodes unsigned 64-bit integers in varint format (e.g., 0→0x00, 1→0x01, 127→0x7F, 128→0x80 0x01, 300→0xAC 0x02, 0xFFFFFFFFFFFFFFFF→10 bytes)
- [ ] pl_encode_svarint() encodes signed integers using zigzag encoding (0→0, -1→1, 1→2, -2→3, 2→4)
- [ ] pl_encode_fixed32() writes 4 bytes in little-endian order
- [ ] pl_encode_fixed64() writes 8 bytes in little-endian order
- [ ] pl_encode_tag() encodes field tag as (field_number << 3 | wire_type) varint
- [ ] pl_encode_string() writes length-prefixed bytes (varint length + raw bytes)
- [ ] Sizing stream: when callback is NULL, pl_write counts bytes but doesn't write, for calculating encoded size
- [ ] Stream overflow: writing past max_size returns false and sets error message
