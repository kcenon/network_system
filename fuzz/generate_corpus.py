#!/usr/bin/env python3
"""
Generate seed corpus for network protocol fuzzing.

Creates valid and edge-case serialized data to seed the fuzzer
for better coverage of the protocol parsing code.
"""

import os
import struct
from pathlib import Path


def generate_http2_frame_corpus(output_dir: Path):
    """Generate seed corpus for HTTP/2 frame parsing."""
    output_dir.mkdir(parents=True, exist_ok=True)

    def make_frame_header(length, frame_type, flags, stream_id):
        """Create a 9-byte HTTP/2 frame header."""
        # Length: 24 bits (3 bytes, big-endian)
        # Type: 8 bits
        # Flags: 8 bits
        # Stream ID: 32 bits (MSB reserved, big-endian)
        return (
            struct.pack(">I", length)[1:]  # 3 bytes of big-endian uint32
            + bytes([frame_type, flags])
            + struct.pack(">I", stream_id & 0x7FFFFFFF)
        )

    # DATA frame (type 0x0)
    payload = b"Hello, World!"
    header = make_frame_header(len(payload), 0x0, 0x01, 1)  # END_STREAM
    with open(output_dir / "data_frame", "wb") as f:
        f.write(header + payload)

    # HEADERS frame (type 0x1)
    hpack_block = bytes([0x82, 0x86])  # :method GET, :path /
    header = make_frame_header(len(hpack_block), 0x1, 0x05, 1)  # END_STREAM | END_HEADERS
    with open(output_dir / "headers_frame", "wb") as f:
        f.write(header + hpack_block)

    # SETTINGS frame (type 0x4) with parameters
    settings_payload = struct.pack(">HI", 0x3, 100)  # MAX_CONCURRENT_STREAMS = 100
    header = make_frame_header(len(settings_payload), 0x4, 0x0, 0)
    with open(output_dir / "settings_frame", "wb") as f:
        f.write(header + settings_payload)

    # SETTINGS ACK (empty)
    header = make_frame_header(0, 0x4, 0x01, 0)  # ACK flag
    with open(output_dir / "settings_ack", "wb") as f:
        f.write(header)

    # RST_STREAM frame (type 0x3)
    rst_payload = struct.pack(">I", 0x0)  # NO_ERROR
    header = make_frame_header(len(rst_payload), 0x3, 0x0, 1)
    with open(output_dir / "rst_stream", "wb") as f:
        f.write(header + rst_payload)

    # PING frame (type 0x6)
    ping_data = b"\x00\x01\x02\x03\x04\x05\x06\x07"
    header = make_frame_header(8, 0x6, 0x0, 0)
    with open(output_dir / "ping_frame", "wb") as f:
        f.write(header + ping_data)

    # GOAWAY frame (type 0x7)
    goaway_payload = struct.pack(">II", 0, 0x0)  # last_stream_id=0, NO_ERROR
    header = make_frame_header(len(goaway_payload), 0x7, 0x0, 0)
    with open(output_dir / "goaway_frame", "wb") as f:
        f.write(header + goaway_payload)

    # WINDOW_UPDATE frame (type 0x8)
    wu_payload = struct.pack(">I", 65535)
    header = make_frame_header(len(wu_payload), 0x8, 0x0, 0)
    with open(output_dir / "window_update", "wb") as f:
        f.write(header + wu_payload)

    # Edge cases
    with open(output_dir / "empty", "wb") as f:
        f.write(b"")

    with open(output_dir / "truncated_header", "wb") as f:
        f.write(b"\x00\x00\x04")  # Only 3 bytes of 9-byte header

    with open(output_dir / "oversized_length", "wb") as f:
        f.write(make_frame_header(0xFFFFFF, 0x0, 0x0, 1))  # Max length, no payload

    with open(output_dir / "invalid_type", "wb") as f:
        f.write(make_frame_header(0, 0xFF, 0x0, 0))

    with open(output_dir / "all_flags_set", "wb") as f:
        f.write(make_frame_header(0, 0x0, 0xFF, 1))

    print(f"Generated {len(list(output_dir.iterdir()))} HTTP/2 frame seeds in {output_dir}")


def generate_quic_varint_corpus(output_dir: Path):
    """Generate seed corpus for QUIC varint decoding."""
    output_dir.mkdir(parents=True, exist_ok=True)

    # 1-byte encoding (6-bit, prefix 0b00)
    with open(output_dir / "varint_0", "wb") as f:
        f.write(bytes([0x00]))  # value = 0

    with open(output_dir / "varint_63", "wb") as f:
        f.write(bytes([0x3F]))  # value = 63 (max 1-byte)

    # 2-byte encoding (14-bit, prefix 0b01)
    with open(output_dir / "varint_64", "wb") as f:
        f.write(bytes([0x40, 0x40]))  # value = 64

    with open(output_dir / "varint_16383", "wb") as f:
        f.write(bytes([0x7F, 0xFF]))  # value = 16383 (max 2-byte)

    # 4-byte encoding (30-bit, prefix 0b10)
    with open(output_dir / "varint_16384", "wb") as f:
        f.write(bytes([0x80, 0x00, 0x40, 0x00]))  # value = 16384

    with open(output_dir / "varint_1073741823", "wb") as f:
        f.write(bytes([0xBF, 0xFF, 0xFF, 0xFF]))  # value = 1073741823 (max 4-byte)

    # 8-byte encoding (62-bit, prefix 0b11)
    with open(output_dir / "varint_8byte", "wb") as f:
        f.write(bytes([0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00]))

    with open(output_dir / "varint_max", "wb") as f:
        f.write(bytes([0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF]))  # max 62-bit

    # Multiple varints concatenated
    with open(output_dir / "multi_varint", "wb") as f:
        f.write(bytes([0x00, 0x3F, 0x40, 0x40]))  # 0, 63, 64

    # Edge cases
    with open(output_dir / "empty", "wb") as f:
        f.write(b"")

    with open(output_dir / "truncated_2byte", "wb") as f:
        f.write(bytes([0x40]))  # 2-byte prefix but only 1 byte

    with open(output_dir / "truncated_4byte", "wb") as f:
        f.write(bytes([0x80, 0x00]))  # 4-byte prefix but only 2 bytes

    with open(output_dir / "truncated_8byte", "wb") as f:
        f.write(bytes([0xC0, 0x00, 0x00]))  # 8-byte prefix but only 3 bytes

    print(f"Generated {len(list(output_dir.iterdir()))} QUIC varint seeds in {output_dir}")


def generate_quic_frame_corpus(output_dir: Path):
    """Generate seed corpus for QUIC frame parsing."""
    output_dir.mkdir(parents=True, exist_ok=True)

    def varint_encode(value):
        """Encode a value as a QUIC varint."""
        if value <= 63:
            return bytes([value])
        elif value <= 16383:
            return bytes([0x40 | (value >> 8), value & 0xFF])
        elif value <= 1073741823:
            return bytes([
                0x80 | ((value >> 24) & 0x3F),
                (value >> 16) & 0xFF,
                (value >> 8) & 0xFF,
                value & 0xFF,
            ])
        else:
            return bytes([
                0xC0 | ((value >> 56) & 0x3F),
                (value >> 48) & 0xFF,
                (value >> 40) & 0xFF,
                (value >> 32) & 0xFF,
                (value >> 24) & 0xFF,
                (value >> 16) & 0xFF,
                (value >> 8) & 0xFF,
                value & 0xFF,
            ])

    # PADDING frame (type 0x00) - just zero bytes
    with open(output_dir / "padding_frame", "wb") as f:
        f.write(bytes([0x00, 0x00, 0x00]))

    # PING frame (type 0x01) - single byte
    with open(output_dir / "ping_frame", "wb") as f:
        f.write(varint_encode(0x01))

    # ACK frame (type 0x02)
    with open(output_dir / "ack_frame", "wb") as f:
        data = varint_encode(0x02)  # type
        data += varint_encode(100)  # largest_acked
        data += varint_encode(0)    # ack_delay
        data += varint_encode(0)    # ack_range_count
        data += varint_encode(0)    # first_ack_range
        f.write(data)

    # RESET_STREAM frame (type 0x04)
    with open(output_dir / "reset_stream", "wb") as f:
        data = varint_encode(0x04)  # type
        data += varint_encode(1)    # stream_id
        data += varint_encode(0)    # app_error_code
        data += varint_encode(100)  # final_size
        f.write(data)

    # CRYPTO frame (type 0x06)
    with open(output_dir / "crypto_frame", "wb") as f:
        crypto_data = b"TLS handshake data"
        data = varint_encode(0x06)              # type
        data += varint_encode(0)                # offset
        data += varint_encode(len(crypto_data)) # length
        data += crypto_data
        f.write(data)

    # STREAM frame (type 0x08)
    with open(output_dir / "stream_frame", "wb") as f:
        stream_data = b"Hello stream"
        data = varint_encode(0x08)               # type (stream_base)
        data += varint_encode(4)                 # stream_id
        data += stream_data
        f.write(data)

    # CONNECTION_CLOSE frame (type 0x1c)
    with open(output_dir / "connection_close", "wb") as f:
        reason = b"no error"
        data = varint_encode(0x1c)           # type
        data += varint_encode(0)             # error_code
        data += varint_encode(0)             # frame_type
        data += varint_encode(len(reason))   # reason_phrase_length
        data += reason
        f.write(data)

    # Edge cases
    with open(output_dir / "empty", "wb") as f:
        f.write(b"")

    with open(output_dir / "unknown_type", "wb") as f:
        f.write(varint_encode(0xFF))  # Unknown frame type

    with open(output_dir / "truncated", "wb") as f:
        f.write(varint_encode(0x04) + varint_encode(1))  # Incomplete RESET_STREAM

    print(f"Generated {len(list(output_dir.iterdir()))} QUIC frame seeds in {output_dir}")


def generate_grpc_frame_corpus(output_dir: Path):
    """Generate seed corpus for gRPC message parsing."""
    output_dir.mkdir(parents=True, exist_ok=True)

    def make_grpc_message(compressed, payload):
        """Create a gRPC length-prefixed message."""
        header = bytes([1 if compressed else 0])
        header += struct.pack(">I", len(payload))
        return header + payload

    # Simple uncompressed message
    with open(output_dir / "simple_msg", "wb") as f:
        f.write(make_grpc_message(False, b"\x0a\x05Hello"))

    # Compressed flag set
    with open(output_dir / "compressed_msg", "wb") as f:
        f.write(make_grpc_message(True, b"\x0a\x05Hello"))

    # Empty message
    with open(output_dir / "empty_msg", "wb") as f:
        f.write(make_grpc_message(False, b""))

    # Larger payload
    with open(output_dir / "large_msg", "wb") as f:
        f.write(make_grpc_message(False, b"\x00" * 1024))

    # Timeout strings
    for name, value in [
        ("timeout_seconds", b"10S"),
        ("timeout_millis", b"500m"),
        ("timeout_hours", b"1H"),
        ("timeout_minutes", b"30M"),
        ("timeout_micros", b"1000u"),
        ("timeout_nanos", b"999999n"),
    ]:
        with open(output_dir / name, "wb") as f:
            f.write(value)

    # Edge cases
    with open(output_dir / "empty", "wb") as f:
        f.write(b"")

    with open(output_dir / "truncated_header", "wb") as f:
        f.write(b"\x00\x00\x00")  # Only 3 of 5 header bytes

    with open(output_dir / "oversized_length", "wb") as f:
        f.write(b"\x00" + struct.pack(">I", 0x00FFFFFF))  # Large length, no payload

    with open(output_dir / "invalid_compressed_flag", "wb") as f:
        f.write(b"\xFF" + struct.pack(">I", 0) + b"")  # Non-standard compressed flag

    print(f"Generated {len(list(output_dir.iterdir()))} gRPC frame seeds in {output_dir}")


def generate_hpack_corpus(output_dir: Path):
    """Generate seed corpus for HPACK decoding."""
    output_dir.mkdir(parents=True, exist_ok=True)

    # Indexed header field (prefix 1xxxxxxx)
    # Static table index 2 = :method GET
    with open(output_dir / "indexed_get", "wb") as f:
        f.write(bytes([0x82]))

    # Indexed header field - :path /
    with open(output_dir / "indexed_path", "wb") as f:
        f.write(bytes([0x84]))

    # Multiple indexed headers
    with open(output_dir / "multi_indexed", "wb") as f:
        f.write(bytes([0x82, 0x86, 0x84]))  # GET, scheme https, /

    # Literal header with incremental indexing (prefix 01xxxxxx)
    # New name and value
    with open(output_dir / "literal_new", "wb") as f:
        name = b"x-custom"
        value = b"test-value"
        data = bytes([0x40])  # Literal with incremental indexing, new name
        data += bytes([len(name)]) + name
        data += bytes([len(value)]) + value
        f.write(data)

    # Literal header with indexed name (prefix 01xxxxxx, name index > 0)
    with open(output_dir / "literal_indexed_name", "wb") as f:
        value = b"bar"
        data = bytes([0x41])  # Incremental indexing, name index 1 (:authority)
        data += bytes([len(value)]) + value
        f.write(data)

    # Literal header without indexing (prefix 0000xxxx)
    with open(output_dir / "literal_no_index", "wb") as f:
        name = b"cache-control"
        value = b"no-cache"
        data = bytes([0x00])  # No indexing, new name
        data += bytes([len(name)]) + name
        data += bytes([len(value)]) + value
        f.write(data)

    # Dynamic table size update (prefix 001xxxxx)
    with open(output_dir / "table_size_update", "wb") as f:
        f.write(bytes([0x20 | 0x1F, 0x00]))  # Update to size 31

    # Edge cases
    with open(output_dir / "empty", "wb") as f:
        f.write(b"")

    with open(output_dir / "max_index", "wb") as f:
        f.write(bytes([0xFF, 0x00]))  # Index 127 (static table only has 61)

    with open(output_dir / "truncated_string", "wb") as f:
        f.write(bytes([0x40, 0x05, 0x41]))  # Claims 5-byte name but only 1 byte

    with open(output_dir / "huffman_encoded", "wb") as f:
        # Huffman flag set (bit 7 of string length byte)
        f.write(bytes([0x82]))  # Indexed, then some huffman data
        f.write(bytes([0x40, 0x85, 0xF2, 0xB2, 0x4A, 0x84, 0xFF]))  # Huffman-encoded name

    print(f"Generated {len(list(output_dir.iterdir()))} HPACK decode seeds in {output_dir}")


def main():
    base_dir = Path("corpus")

    generate_http2_frame_corpus(base_dir / "http2_frame")
    generate_quic_varint_corpus(base_dir / "quic_varint")
    generate_quic_frame_corpus(base_dir / "quic_frame")
    generate_grpc_frame_corpus(base_dir / "grpc_frame")
    generate_hpack_corpus(base_dir / "hpack_decode")

    print(f"\nCorpus generation complete!")
    print(f"Total files: {sum(1 for _ in base_dir.rglob('*') if _.is_file())}")


if __name__ == "__main__":
    main()
