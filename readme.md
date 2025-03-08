# Memory-Mapped File I/O Library

A C++ library providing memory-mapped file operations through mmap_reader and mmap_writer classes.

This library is only compatible with Linux systems.

## Features

- Zero-copy operations through memory mapping
- RAII design. Move-only
- All operations that can fail throw `std::system_error` with appropriate error codes and messages

### mmap_reader
- Multiple reading modes (whole file, line-by-line, char-by-char)
- Iterator support for lines and characters
- Support direct position reading, random access reading, seek operations

### mmap_writer
- Support for both truncate and append modes
- Automatic file expansion. Configurable expansion size. Default expansion size: 8192 bytes
- Support direct position writing, random access writing, seek operations

## Basic Usage

### mmap_reader

```cpp
#include "mmap_reader.hpp"

mmap_reader reader("example.txt");

std::string_view content_view = reader.view();
std:: string content_str = reader.str();

for (std::string_view line : reader.lines())
{
    // Process line
}

reader.seek(0);  // Reset to beginning

for (char c : reader.chars())
{
    // Process character
}

reader.seek(0); // Reset to beginning

std::string str(10, '\0');
reader.read(str);
reader.pread(str, 6);
```

### mmap_writer
```cpp
mmap_writer writer("example.txt", true); // Create/open a file, truncate = true

std::string data = "Hello, World!";
writer.write(data);
```

## API Reference

#### Note
- The offset refers to the current read/write position.
- In `mmap_reader`, the methods `view()`, `str()`, and `pread()` do not modify the current offset.
- In `mmap_writer`, the method `pwrite()` does not modify the current offset.
- If you want to change the offset after calling these methods, you need to manually call `seek()`.


### mmap_reader

#### Constructor
- `mmap_reader()`
  - Default constructor, creates an empty reader.

- `explicit mmap_reader(const std::filesystem::path& path)`
  - Construct and open file at specified path.

- `explicit mmap_reader(int fd)`
  - Construct from existing file descriptor.

#### File Operations
- `void open(const std::filesystem::path& path)`
  - Open file by path.

- `void open(const int new_fd)`
  - Open file by descriptor.

- `void close() noexcept`
  - Close file and cleanup.

- `bool is_open() const noexcept`
  - Check if file is open.

- `explicit operator bool() const noexcept`
  - Check if file is open and readable.

#### Reading Operations
- `size_t read(std::span<char> buf) noexcept`
  - Read data into provided buffer, returns bytes read.

- `size_t pread(std::span<char> buf, size_t offset) const noexcept`
  - Read from specific offset into buffer.

- `std::optional<std::string_view> getline(char delimiter = '\n')`
  - Read next line until delimiter.

- `std::optional<char> getchar()`
  - Read next character.

#### View Operations
- `const char* data() const noexcept`
  - Get pointer to mapped memory.

- `size_t size() const noexcept`
  - Get size of the mapped file.

- `std::string_view view() const noexcept`
  - Get view of entire mapped file.

- `std::string str() const`
  - Get copy of entire file as string.

- `std::string_view view(size_t offset, size_t len) const noexcept`
  - Get view of file portion.

- `std::string str(size_t offset, size_t len) const`
  - Get partial string.

#### Position Management
- `size_t tell() const noexcept`
  - Get current position.

- `void seek(size_t pos) noexcept`
  - Seek to absolute position.

- `void seek(ptrdiff_t off, seekdir dir) noexcept`
  - Seeks relative to beginning (beg), current position (cur), or end (end).

#### Iterators
- `LineReader lines(char delimiter = '\n')`
  - Get line iterators.

- `CharReader chars()`
  - Get character iterators.

---

### mmap_writer

#### Constructor
- `mmap_writer(const std::filesystem::path& filename, bool truncate)`
  - Creates a new writer for the specified file.
  - `truncate`: If true, truncates existing file; if false, appends to it.

#### File Operations
- `void open(const std::filesystem::path& filename, bool truncate = true)`
  - Opens a file for writing.
  - Automatically closes any previously opened file.

- `void close() noexcept`
  - Closes the file and releases all resources.
  - Automatically called by destructor.

- `bool is_open() const noexcept`
  - Checks if the writer has an open file.

- `explicit operator bool() const noexcept`
  - Returns true if the writer has an open file.

#### Writing Operations
- `void write(std::span<const char> buf)`
  - Writes data at current position.
  - Automatically expands file if needed.

- `void pwrite(size_t offset, std::span<const char> buf)`
  - Writes data at specified offset.
  - Automatically expands file if needed.

#### Position Management
- `size_t tell() const noexcept`
  - Returns current write position.

- `void seek(size_t pos)`
  - Sets absolute write position.
  - Expands file if necessary.

- `void seek(ptrdiff_t off, seekdir dir)`
  - Seeks relative to beginning (beg), current position (cur), or end (end).
  - Supports negative offsets.
  - Expands file if necessary.

#### Memory Management
- `void reserve(size_t new_size)`
  - Pre-allocates space for writing.
  - Useful when final size is known.

- `void set_expand_size(size_t new_expand_size)`
  - Sets the increment size for automatic expansion.
  - Default is 8192 bytes.

- `void shrink_to_fit()`
  - Reduces mapped memory to actual file size.

#### Synchronization
- `void flush(bool async = false)`
  - Synchronizes mapped memory with disk.
  - `async`: If true, performs asynchronous sync.


## Benchmark

### File Characteristics

- 1 million lines of text
- Each line format: "This is line {number}\n"

### mmap_reader
Memory-mapped reading consistently outperforms fstream operations:

- Whole File Read
    - fstream: 8ms
    - mmap: 3ms
    - Performance ratio: 0.375x (3.75x faster)

- Line-by-Line Read
    - fstream: 32ms
    - mmap: 12ms
    - Performance ratio: 0.375x (3.75x faster)

- Character-by-Character Read
    - fstream: 116ms
    - mmap: 29ms
    - Performance ratio: 0.25x (4x faster)

### mmap_writer
For best write performance when using mmap_writer, call reserve() ONCE to pre-allocate ALL required space before writing any data.

- fstream: 40ms
- Without reserve: 94ms
- With reserve: 32ms
- Performance ratio: 0.8x
