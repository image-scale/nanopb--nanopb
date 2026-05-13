# Todo

## Plan
Implement the protobuf C library bottom-up by wire format layer: first the encoding/decoding primitives and stream abstraction, then the message descriptor and field iteration system, followed by automatic message encoding and decoding, and finally callback fields, extensions, and dynamic memory support.

## Tasks
- [x] Task 1: Implement protobuf wire format encoding primitives with output stream abstraction (include/protolite.h types + include/protolite_encode.h + src/pl_encode.c + tests/test_wire_encode.c)
- [x] Task 2: Implement protobuf wire format decoding primitives with input stream abstraction (include/protolite_decode.h + src/pl_decode.c + tests/test_wire_decode.c)
- [x] Task 3: Implement message descriptor system with field iteration and tag-based lookup (include/protolite_common.h + src/pl_common.c + tests/test_field_iter.c)
- [>] Task 4: Implement automatic message encoding that iterates struct fields and produces wire format output (src/pl_encode.c additions + tests/test_msg_encode.c)
- [ ] Task 5: Implement automatic message decoding that reads wire format data and fills struct fields with required field validation (src/pl_decode.c additions + tests/test_msg_decode.c)
- [ ] Task 6: Implement callback fields, extension field handling, delimited/null-terminated message modes, and encode size calculation (src additions + tests/test_advanced.c)
- [ ] Task 7: Implement dynamic memory allocation support for pointer-type fields with automatic memory management and release (src additions + tests/test_dynalloc.c)
