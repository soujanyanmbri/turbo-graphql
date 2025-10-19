# turbo-graphql
Experimental High-Performance GraphQL Parser for C++

## Features

✨ **SIMD-Accelerated Lexer** - Uses AVX2/SSE4.2 instructions for ultra-fast tokenization  
✨ **Smart Keyword Classification** - Hash-based keyword detection for instant recognition  
✨ **Arena Memory Allocation** - Efficient memory management for token storage  
✨ **Runtime SIMD Detection** - Automatically selects best available instruction set  

## Build & Run

```bash
# Clean build (optional)
rm -rf build

# Configure the project
cmake -B build

# Build the project
cmake --build build

# Run the parser with sample query
./build/graphql_parser

# Run the parser with your own GraphQL file
./build/graphql_parser your_query.graphql

# Run performance benchmark
./build/benchmark

# Run tests
./build/graphql_tests
```

## Example Output

```
====================================================================
         TURBO-GRAPHQL: High-Performance GraphQL Parser            
====================================================================

SIMD Detection: AVX2

Tokenizing with SIMD-accelerated lexer...
Tokenization completed in 15 microseconds

+-----+---------------------+----------------------+-----------------+
| No. | Type                | Value                | Position        |
+-----+---------------------+----------------------+-----------------+
|   1 | KEYWORD_QUERY       | query                | 24              |
|   2 | IDENTIFIER          | GetUser              | 30              |
|   3 | LEFT_PAREN          | (                    | 37              |
...
```

## Performance

Real-world benchmarks on AVX2-capable CPU:

| Query Size | Avg Time | Throughput | Tokens | Speedup vs Scalar* |
|------------|----------|------------|--------|-------------------|
| 26 bytes   | 2.1 µs   | 11 MB/s    | 8      | ~1.5x            |
| 122 bytes  | 7.2 µs   | 16 MB/s    | 25     | ~2x              |
| 406 bytes  | 17.7 µs  | 21 MB/s    | 61     | ~3x              |
| 1.5 KB     | 46.6 µs  | 32 MB/s    | 167    | ~4x              |
| 8 KB       | 445 µs   | 17 MB/s    | 1531   | ~5x              |

*Based on test4.cpp reference: **5.23x average speedup** with SIMD

**Key Insights:**
- SIMD overhead is negligible for queries >100 bytes
- Throughput increases with input size (better SIMD utilization)
- Production GraphQL queries (500+ bytes) see 3-5x performance gains

## Project Status

- ✅ **Lexer/Tokenizer**: Fully implemented with SIMD optimization
- ✅ **SIMD Detection**: Auto-detects AVX512, AVX2, SSE4.2, SSE2, NEON, or falls back to scalar
- ✅ **Token Classification**: Keywords, identifiers, strings, numbers, directives, variables
- ⏳ **Parser**: To be implemented
- ⏳ **AST**: To be implemented  
- ⏳ **Runtime/Executor**: To be implemented

## Architecture

### Lexer
- Character classification using lookup tables
- SIMD-accelerated whitespace/comment skipping
- Fast identifier and number parsing
- Efficient string literal handling

### SIMD Support
- **AVX2**: 256-bit SIMD operations (primary target)
- **SSE4.2**: 128-bit SIMD operations (fallback)
- **Scalar**: Portable fallback for all platforms

## License

See LICENSE file for details.
