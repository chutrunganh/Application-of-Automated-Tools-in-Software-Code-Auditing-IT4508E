#!/usr/bin/env python3
"""
generate_full_json_seeds.py

Creates:
 - seeds/strategy1_non_structured
 - seeds/strategy2_structured

Run: python3 generate_seeds.py
"""
import os, json, random, string, sys, codecs

BASE = "./seeds"
S1 = os.path.join(BASE, "strategy1_non_structured")
S2 = os.path.join(BASE, "strategy2_structured")

os.makedirs(S1, exist_ok=True)
os.makedirs(S2, exist_ok=True)

def write_text(path, s, mode="w"):
    with open(path, mode, encoding="utf-8", errors="surrogateescape") as f:
        f.write(s)

def write_bin(path, b):
    with open(path, "wb") as f:
        f.write(b)

### ----------------------
### Strategy 1: malformed / non-structured / binary
### ----------------------

# 1) Empty file
write_bin(os.path.join(S1, "000_empty.bin"), b"")

# 2) Lots of whitespace
write_text(os.path.join(S1, "001_spaces.txt"), " " * 200_000)

# 3) Lots of newlines / CRLF / CR
write_text(os.path.join(S1, "002_newlines_unix.json"), "\n" * 100_000)
write_text(os.path.join(S1, "003_newlines_windows.json"), ("\r\n" * 80_000))
write_text(os.path.join(S1, "004_newlines_mac.json"), ("\r" * 80_000))

# 4) Random binary blobs / high-entropy
write_bin(os.path.join(S1, "005_random_256k.bin"), os.urandom(256 * 1024))
write_bin(os.path.join(S1, "006_zero_bytes.bin"), b"\x00" * (128 * 1024))
write_bin(os.path.join(S1, "007_ff_bytes.bin"), b"\xff" * (128 * 1024))

# 5) Truncated JSON snippets (unterminated strings, arrays, objects)
write_text(os.path.join(S1, "010_truncated_unterminated_string.json"), '{"key":"unterminated')
write_text(os.path.join(S1, "011_truncated_unterminated_array.json"), '["abc", 123, ')
write_text(os.path.join(S1, "012_truncated_unterminated_object.json"), '{"a": {"b": {"c": 1')

# 6) Malformed separators / repeated punctuation
write_text(os.path.join(S1, "020_repeated_braces.json"), "}" * 50000)
write_text(os.path.join(S1, "021_repeated_brackets.json"), "]" * 50000)
write_text(os.path.join(S1, "022_comma_spam.json"), "," * 100000)

# 7) Broken escape sequences (invalid \x, incomplete \u)
write_text(os.path.join(S1, "030_invalid_escapes.json"), '"\\xZZ\\u12"')
write_text(os.path.join(S1, "031_broken_unicode_escape.json"), '"\\uD800"')  # lone surrogate half

# 8) Non-UTF8 sequences / invalid multibyte split across buffer
# produce a valid two-byte UTF-8 char and then cut between bytes
valid_multibyte = "€".encode("utf-8")  # 3 bytes actually
b = b'"' + valid_multibyte[:1]  # incomplete multibyte sequence after opening quote
write_bin(os.path.join(S1, "032_incomplete_multibyte.bin"), b)

# 9) BOM-only and BOM + junk
write_bin(os.path.join(S1, "040_bom_only.bin"), codecs.BOM_UTF8)
write_bin(os.path.join(S1, "041_bom_junk.bin"), codecs.BOM_UTF8 + b"A" * 1024)

# 10) Interleaved garbage + JSON-like tokens
write_text(os.path.join(S1, "050_garbage_with_json_fragments.txt"),
           "AAAA{" + "\"key\"" + ": " + '"v' + ("\x00" * 1000) + '"}BBBB')

# 11) Non-standard comments / trailing commas (to test lenient parsers)
write_text(os.path.join(S1, "060_c_style_comments.json"), "/* comment */ { \"a\": 1 } // end")
write_text(os.path.join(S1, "061_trailing_comma.json"), '{"a":1,}')

# 12) Huge ASCII run (1MB)
write_text(os.path.join(S1, "070_long_A_1MB.txt"), "A" * (1024 * 1024))

# 13) Many small fragments concatenated (stream-like, invalid for strict json)
chunks = ['{"a":1}', '{"b":2}', '[1,2,3]', 'null', 'true', '"x"']
write_text(os.path.join(S1, "080_concatenated_fragments.txt"), "".join(chunks * 1000))

# 14) Binary inserted into JSON wrapper
write_bin(os.path.join(S1, "090_binary_in_json.bin"),
          b'{"data":"'+ os.urandom(4096) + b'"}')

# 15) Invalid numeric forms that some parsers might choke on
write_text(os.path.join(S1, "100_invalid_numbers.json"),
           '["+1", "01", "1.", ".5", "1e", "1e+9999999999999"]')

# 16) Repeated similar keys but malformed glue
write_text(os.path.join(S1, "110_similar_keys_malformed.txt"),
           '{"key001": "v",' + '"key002" "missing_colon",' + '"key003": "v"}')

# 17) Mixed control characters inside (control bytes inside supposed string)
ctrl = "".join(chr(i) for i in range(0,32))
write_text(os.path.join(S1, "120_control_chars_in_string.json"),
           '"' + ctrl + '"')

# 18) Extremely long line without separators (to test input buffering)
write_text(os.path.join(S1, "130_long_run_no_ws.txt"), "x" * (2 * 1024 * 1024))

### ----------------------
### Strategy 2: valid JSON seeds + many boundary/edge cases
### ----------------------

# helper to write valid json file
def write_json_file(path, obj):
    s = json.dumps(obj, ensure_ascii=False)
    write_text(path, s)

# 1) Basic valid seeds
write_text(os.path.join(S2, "000_empty_object.json"), "{}")
write_text(os.path.join(S2, "001_empty_array.json"), "[]")
write_text(os.path.join(S2, "002_integer_zero.json"), "0")
write_text(os.path.join(S2, "003_integer_pos.json"), "123")
write_text(os.path.join(S2, "004_double.json"), "3.141592653589793")
write_text(os.path.join(S2, "005_string_empty.json"), '""')
write_text(os.path.join(S2, "006_true.json"), "true")
write_text(os.path.join(S2, "007_false.json"), "false")
write_text(os.path.join(S2, "008_null.json"), "null")

# 2) Deep nesting (object) - depth configurable but safe default; we also create ultra deep small file
def make_deep_object(depth):
    s = ""
    for i in range(depth):
        s += '{"a":'
    s += '"END"'
    s += "}" * depth
    return s

# moderate deep (2000)
write_text(os.path.join(S2, "010_deep_object_2000.json"), make_deep_object(2000))
# extreme deep (10000) - warning: file will be big but we include it (user can adjust)
write_text(os.path.join(S2, "011_deep_object_10000.json"), make_deep_object(10000))

# 3) Deep nesting (array of arrays)
def make_deep_array(depth):
    s = "[" * depth + "0" + "]" * depth
    return s

write_text(os.path.join(S2, "020_deep_array_2000.json"), make_deep_array(2000))
write_text(os.path.join(S2, "021_deep_array_8000.json"), make_deep_array(8000))

# 4) Large object with many keys (many similar prefixes to exercise cmp-log)
def make_many_keys(n_keys=5000, key_len=8, val_len=256):
    o = {}
    base = "key"
    for i in range(n_keys):
        # keys with similar prefixes
        key = f"{base}{i:06d}"
        o[key] = "V" * val_len
    return o

# create a somewhat large object (5000 keys, ~1.25MB)
big_obj = make_many_keys(5000, 8, 256)
write_json_file(os.path.join(S2, "030_many_keys_5000.json"), big_obj)

# 5) Very long keys & values
long_key = "k" * 10000
long_val = "v" * 200000
write_text(os.path.join(S2, "040_long_key.json"), json.dumps({long_key: "short"}))
write_text(os.path.join(S2, "041_long_value_200k.json"), json.dumps({"k": long_val}))

# 6) Huge string seeds (1MB, 5MB) - may be large on disk
write_text(os.path.join(S2, "050_string_1MB.json"), '"' + ("A" * (1 * 1024 * 1024)) + '"')
# 5MB string (be cautious - big!)
write_text(os.path.join(S2, "051_string_5MB.json"), '"' + ("B" * (5 * 1024 * 1024)) + '"')

# 7) Numeric boundary values
# very long integer digits (10k digits)
write_text(os.path.join(S2, "060_big_integer_10k.json"), "1" * 10000)
write_text(os.path.join(S2, "061_big_negative_integer_10k.json"), "-" + "9" * 10000)
# floating extremes
write_text(os.path.join(S2, "062_float_large_exp.json"), "1e308")
write_text(os.path.join(S2, "063_float_overflow_exp.json"), "1e9999")
write_text(os.path.join(S2, "064_float_many_decimal.json"), "0." + "1"*10000)

# 8) Leading zeros / plus sign / weird numeric formats
write_text(os.path.join(S2, "070_leading_zero.json"), "0000123")
write_text(os.path.join(S2, "071_plus_sign.json"), "+123")
write_text(os.path.join(S2, "072_decimal_without_leading.json"), ".5")

# 9) Escape-heavy strings (many backslashes & unicode escapes)
write_text(os.path.join(S2, "080_many_backslashes.json"), '"' + ('\\' * 200000) + '"')
write_text(os.path.join(S2, "081_many_unicode_escapes.json"), '"' + ("\\uFFFF" * 5000) + '"')
write_text(os.path.join(S2, "082_surrogate_pair_half.json"), '"' + "\\uD800" * 1000 + '"')

# 10) Duplicate keys (same key repeated) - JSON allows but semantics differ by parser
write_text(os.path.join(S2, "090_duplicate_keys.json"),
           '{"k":1, "k":2, "k":3}')

# 11) Keys that differ by one character (helps cmplog)
o = {("key_prefix_" + ("A"*10) + chr(65+i)): i for i in range(20)}
write_json_file(os.path.join(S2, "100_similar_keys.json"), o)

# 12) Mixed-type chaos big file (arrays, objects, nested)
chaos = {
    "nums": [0, -1, 2**31, 3.14, 1e100, -1e100],
    "strings": ["s"*1000, "\u2603", "emoji😀"*100],
    "nulls": [None]*100,
    "bools": [True, False]*50,
    "nested": [{"a": [{"b": ["x"*500, None, {"deep":"y"*1000}]} for _ in range(5)]} for _ in range(10)]
}
write_json_file(os.path.join(S2, "110_mixed_chaos.json"), chaos)

# 13) Trailing commas / comments (lenient JSON) included for fuzzers that target lenient parsers
write_text(os.path.join(S2, "120_trailing_commas.json"), '{"a":1,"b":2,}')
write_text(os.path.join(S2, "121_comments_allowed.json"), '/* comment */ {"a":1} // tail comment')

# 14) UTF-8 boundary tests: split valid multibyte sequences across buffers
# build a JSON string whose bytes when chunked will split multibyte chars; store as binary
s = "a" + "€" * 1000 + "b"  # euro sign is multibyte
b = ('"' + s + '"').encode('utf-8')
# chop into two parts and write first part only (simulate stream cut)
write_bin(os.path.join(S2, "130_utf8_split_prefix.bin"), b[:len(b)//2])

# 15) Very large arrays with repeated patterns (to explode memory)
arr = ["x"*100] * 200000  # repeated references; but json.dumps will expand
# to avoid creating crazy memory, write a streaming representation
with open(os.path.join(S2, "140_big_array_stream.json"), "w", encoding="utf-8") as f:
    f.write("[")
    for i in range(20000):
        if i: f.write(",")
        f.write(json.dumps("x"*200))
    f.write("]")

# 16) Seeds to help reach conditional comparisons (keys + values)
# create seeds where many keys exist and values at specific keys appear
cmp_obj = {}
for i in range(300):
    cmp_obj[f"field_{i:04d}"] = "val" if i==137 else ("val"+str(i))
write_json_file(os.path.join(S2, "150_cmp_target.json"), cmp_obj)

# 17) JSON with embedded binary encoded as base64-like string
import base64
b64 = base64.b64encode(os.urandom(4096)).decode('ascii')
write_text(os.path.join(S2, "160_base64_blob.json"), json.dumps({"blob": b64}))

# 18) Minor variants: CRLF endings, trailing spaces
write_text(os.path.join(S2, "170_crlf_json.json"), '{"a":1, "b":2}\r\n')
write_text(os.path.join(S2, "171_trailing_space.json"), '{"a":1}   ')

# 19) Schema-like minimal examples (to guide parser paths)
write_json_file(os.path.join(S2, "180_schema_like_small.json"),
                {"type": "object", "properties": {"a": {"type": "string"}, "b": {"type": "number"}}})

# 20) Small corpus of targeted real-world style JSONs (configs, logs)
write_json_file(os.path.join(S2, "190_realistic_config.json"),
                {"version": "1.2.3", "services": [{"name":"svc1","port":8080},{"name":"svc2","port":9090}], "debug": False})

print("[+] Seed generation complete.")
print("  strategy1 files:", len(os.listdir(S1)))
print("  strategy2 files:", len(os.listdir(S2)))
