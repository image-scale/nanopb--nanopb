# Goal

## Project
nanopb — a c project.

## Description
A lightweight Protocol Buffers implementation in ANSI C, designed for embedded systems and memory-constrained environments. The library encodes and decodes protobuf wire format data from C structs using compact message descriptors. Key features include buffer-based and stream-based I/O, support for all protobuf scalar types (varint, fixed32, fixed64, zigzag, bytes, strings), submessages, repeated/optional/required/oneof fields, callback-based dynamic fields, extension fields, and optional dynamic memory allocation (pointer-type fields with malloc/free).

## Scope
- 4 production header files to implement (main types header, encode header, decode header, common header)
- 4 production source files to implement (encode, decode, common, plus the main header is macro-only)
- 6 test files to write
- Reproduce the core C library: wire format encoding/decoding, message descriptor field iteration, automatic struct encoding/decoding, callback fields, extensions, and dynamic memory support
