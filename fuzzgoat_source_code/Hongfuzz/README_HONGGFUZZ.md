# HonggFuzz Fuzzing Setup

## Overview

HonggFuzz là một fuzzer hiện đại với các tính năng:
- **Hardware-based coverage**: Sử dụng Intel PT (Processor Trace) cho feedback cực nhanh
- **Persistent fuzzing**: Giống AFL++ persistent mode
- **Multi-threaded**: Native support cho parallel fuzzing
- **Sanitizer integration**: ASAN, UBSAN, MSAN
- **Corpus minimization**: Tự động minimize corpus

## Installation

### Ubuntu/Debian
```bash
sudo apt-get install honggfuzz
```

### Build from source (recommended for latest features)
```bash
git clone https://github.com/google/honggfuzz.git
cd honggfuzz
make
sudo make install
```

## Compilation

### Quick compile
```bash
chmod +x compile_honggfuzz.sh
./compile_honggfuzz.sh
```

### Manual compile
```bash
# Basic
hfuzz-cc -O3 main_honggfuzz.c fuzzgoat.c -o main_honggfuzz

# With ASAN (recommended)
hfuzz-cc -O3 -fsanitize=address main_honggfuzz.c fuzzgoat.c -o main_honggfuzz_asan

# With UBSAN
hfuzz-cc -O3 -fsanitize=undefined main_honggfuzz.c fuzzgoat.c -o main_honggfuzz_ubsan
```

## Running

### Quick start
```bash
chmod +x run_honggfuzz.sh
./run_honggfuzz.sh
```

### Manual run

**Basic fuzzing:**
```bash
honggfuzz -i seeds/strategy2_structured/ -o hfuzz_out/ -- ./main_honggfuzz_asan
```

**With dictionary:**
```bash
honggfuzz -i seeds/strategy2_structured/ -o hfuzz_out/ \
    -w seeds/json.dict \
    -- ./main_honggfuzz_asan
```

**Multi-threaded (8 threads):**
```bash
honggfuzz -i seeds/strategy2_structured/ -o hfuzz_out/ \
    -w seeds/json.dict \
    -n 8 \
    -- ./main_honggfuzz_asan
```

**With Intel PT (hardware coverage - fastest):**
```bash
honggfuzz -i seeds/strategy2_structured/ -o hfuzz_out/ \
    -w seeds/json.dict \
    -n 8 \
    --linux_perf_ipt_block \
    -- ./main_honggfuzz_asan
```

**Exit on first crash:**
```bash
honggfuzz -i seeds/strategy2_structured/ -o hfuzz_out/ \
    --exit_upon_crash \
    -- ./main_honggfuzz_asan
```

## Important Options

| Option | Description |
|--------|-------------|
| `-i DIR` | Input corpus directory |
| `-o DIR` | Output/workspace directory (REQUIRED) |
| `-w FILE` | Dictionary file (lowercase w!) |
| `-n N` | Number of parallel threads |
| `-N N` | Max iterations (0 = infinite) |
| `-t N` | Timeout per run (seconds) |
| `--linux_perf_ipt_block` | Intel PT mode (best coverage) |
| `--linux_perf_branch` | Branch coverage (fallback) |
| `--exit_upon_crash` | Stop on first crash |
| `-v` | Verbose mode |

## Output

HonggFuzz tạo các file trong output directory:

- **HONGGFUZZ.REPORT.TXT**: Báo cáo tổng hợp crashes
- **SIG*.PC.*.STACK.*.CODE.*.ADDR.*.INSTR.*.fuzz**: Crash files với metadata
- **.honggfuzz.input.PID**: Persistent input files

## Performance Comparison

### HonggFuzz vs AFL++

**HonggFuzz advantages:**
- Faster với Intel PT (hardware coverage)
- Native multi-threading
- Không cần master-slave setup
- Tự động corpus minimization

**AFL++ advantages:**
- Mature ecosystem
- Nhiều power schedules
- Tốt hơn cho targets không có Intel PT
- Custom mutators dễ dàng hơn

### Expected Performance

- **Without Intel PT**: 5k-20k exec/sec per thread
- **With Intel PT**: 50k-200k exec/sec per thread
- **ASAN overhead**: ~2-5x slowdown (nhưng detect bugs tốt hơn)

## Tips

1. **Sử dụng Intel PT nếu có**:
   ```bash
   # Check Intel PT support
   grep intel_pt /proc/cpuinfo
   
   # Enable với sudo nếu cần
   sudo sysctl -w kernel.perf_event_paranoid=-1
   ```

2. **Tối ưu threads**: Dùng số cores CPU
   ```bash
   honggfuzz -n $(nproc) ...
   ```

3. **Kết hợp với AFL++**: Import corpus từ AFL++
   ```bash
   honggfuzz -i afl_output/default/queue/ -o hfuzz_out/ ...
   ```

4. **Monitor crashes real-time**:
   ```bash
   tail -f hfuzz_out/HONGGFUZZ.REPORT.TXT
   ```

## Troubleshooting

### Permission issues với perf
```bash
sudo sysctl -w kernel.perf_event_paranoid=-1
sudo sysctl -w kernel.kptr_restrict=0
```

### Slow execution
- Check ASAN overhead (thử version không ASAN)
- Verify Intel PT hoạt động
- Reduce timeout: `-t 1`

### No new coverage
- Try different input corpus (strategy1 vs strategy2)
- Verify target có instrumentation
- Check dictionary quality

## Reproduce Crashes

```bash
# Extract crash input
CRASH_FILE="hfuzz_out/SIG*.fuzz"

# Run with crash input
./main_honggfuzz_asan < "$CRASH_FILE"

# Debug với gdb
gdb --args ./main_honggfuzz_asan < "$CRASH_FILE"
```

## References

- [HonggFuzz GitHub](https://github.com/google/honggfuzz)
- [HonggFuzz Docs](https://github.com/google/honggfuzz/tree/master/docs)
- [Intel PT Guide](https://github.com/google/honggfuzz/blob/master/docs/PersistentFuzzing.md)
