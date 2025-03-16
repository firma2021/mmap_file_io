# Memory-Mapped File I/O Library

A C++ library providing memory-mapped file operations through mmap_reader and mmap_writer classes.

Since most scenarios involve either reading or writing, and to ensure safety and simplicity, this library provides `mmap_reader` and `mmap_writer` classes without support for simultaneous read and write operations.

This library is only compatible with Linux systems.

## Features

- Zero-copy operations through memory mapping.
- RAII design. Neither copyable nor movabley.
- All IO operations that can fail throw `std::system_error` with appropriate messages.

### mmap_reader
- Multiple reading modes (whole file, line-by-line, char-by-char).
- Iterator support for lines and characters.
- Support direct position reading, random access reading, seek operations.

### mmap_writer
- Support for both truncate and append modes.
- Automatic file expansion. Configurable expansion size. Default expansion size: 8192 bytes.
- Support direct position writing, random access writing, seek operations.

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
#include "mmap_writer.hpp"

mmap_writer writer("example.txt", true, 1024 * 1024); // Create/open a file, truncate = true, reserved size = 1024 * 1024 bytes

std::string data = "Hello, World!";
writer.write(data);
```

## API Reference

#### Note
- The offset refers to the current read/write position.
- In `mmap_reader`, the methods `view()`, `str()`, and `pread()` do not modify the current offset.
- In `mmap_writer`, the method `pwrite()` does not modify the current offset.
- If you want to change the offset after calling these methods, you need to manually call `seek()`.
- Both classes are neither copyable nor movable.
- Neither class provides open() or close() methods - files are opened in the constructor and closed in the destructor.
- When constructing with a filename, the file is automatically closed in the destructor. When constructing with a file descriptor, the file descriptor is not closed in the destructor.

### mmap_reader

#### Constructor
- `explicit mmap_reader(std::string_view path)`
  - Constructs a reader and opens the file at the specified path.
  - Throws std::system_error if file cannot be opened or mapped.

- `explicit mmap_reader(int fd)`
  - Constructs a reader from an existing file descriptor.
  - The file descriptor will not be closed in the destructor.
  - Throws std::invalid_argument if fd is invalid.

#### Reading Operations
- `std::span<char> read(std::span<char> buf) noexcept`
  - Reads data into the provided buffer starting from current position.
  - Returns a span representing the portion of the buffer that was filled.
  - Advances the current position by the number of bytes read.

- `size_t pread(std::span<char> buf, size_t offset) const noexcept`
  - Reads data into the provided buffer starting from the specified offset.
  - Returns the number of bytes read.
  - Does not change the current position.

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
  - Sets the read position to the specified absolute position.
  - If pos is beyond the file size, sets position to end of file.

- `void seek(ptrdiff_t off, seekdir dir) noexcept`
  - Sets the read position relative to a reference point.
  - dir can be seekdir::beg (beginning), seekdir::cur (current position), or seekdir::end (end of file).
  - Supports negative offsets.
  - Clamps to file boundaries.

#### Iterators
- `LineReader lines(char delimiter = '\n')`
  - Returns an iterator range for reading lines.
  - Each iteration returns a line as string_view (excluding delimiter).

- `CharReader chars()`
  - Returns an iterator range for reading characters.
  - Each iteration returns a single character.

---

### mmap_writer

#### Constructor
- `mmap_writer(std::string_view filename, bool truncate, size_t reserved_size = 0)`
  - Creates a new writer for the specified file.
  - truncate: If true, truncates existing file; if false, appends to it.
  - reserved_size: Pre-allocates space for writing.
  - Throws std::system_error if file cannot be opened or mapped.

- `mmap_writer(int fd, bool truncate, size_t reserved_size = 0)`
  - Creates a new writer for the specified file descriptor.
  - The file descriptor will not be closed in the destructor.
  - Throws std::invalid_argument if fd is invalid.

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
  - Sets the write position relative to a reference point.
  - dir can be seekdir::beg (beginning), seekdir::cur (current position), or seekdir::end (end of file).
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

-  `size_t size() const noexcept`
    - Returns the current size of the file (the maximum position that has been written to)
    - This represents the actual data size, not the allocated capacity

- `size_t capacity() const noexcept`
    - Returns the current capacity of the mapped memory region
    - This is the total space allocated, which may be larger than the actual data size
    - Useful for determining how much more data can be written before expansion occurs

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
