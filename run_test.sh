#!/bin/bash
set -eo pipefail
cd "$(dirname "$0")"

echo "Compiling and running test_wire_encode..."
gcc -Wall -Werror -I include -o tests/test_wire_encode tests/test_wire_encode.c src/pl_encode.c
./tests/test_wire_encode

echo "Compiling and running test_wire_decode..."
gcc -Wall -Werror -I include -o tests/test_wire_decode tests/test_wire_decode.c src/pl_decode.c src/pl_encode.c
./tests/test_wire_decode

echo "Compiling and running test_field_iter..."
gcc -Wall -Werror -I include -o tests/test_field_iter tests/test_field_iter.c src/pl_common.c src/pl_encode.c src/pl_decode.c
./tests/test_field_iter

echo "All tests passed."
