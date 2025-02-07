#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <stdexcept>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
using namespace std;

class mmap_writer
{
private:
    char* mapped_ptr {};
    int fd {-1};

    size_t size {};
    size_t capacity {};
    size_t expand_size {8192};

    void expand(size_t new_size)
    {
        size_t new_capacity = capacity;
        while (new_capacity < new_size) { new_capacity += expand_size; }

        if (ftruncate(fd, static_cast<off_t>(new_capacity)) == -1)
        {
            throw system_error {errno, system_category(), "ftruncate failed"};
        }

        void* new_ptr = mremap(mapped_ptr, capacity, new_capacity, MREMAP_MAYMOVE);
        if (new_ptr == MAP_FAILED)
        {
            throw system_error {errno, system_category(), "mremap failed"};
        }

        mapped_ptr = static_cast<char*>(new_ptr);
        capacity = new_capacity;
    }

    void open_file(const filesystem::path& filename, bool truncate)
    {
        const int flag = truncate ? O_TRUNC : O_APPEND;

        fd = ::open(filename.c_str(), O_WRONLY | O_CREAT | flag, S_IRUSR | S_IWUSR);
        if (fd == -1) { throw system_error {errno, system_category(), "Failed to open file"}; }

        if (!truncate)
        {
            struct stat sb;
            if (fstat(fd, &sb) == -1)
            {
                throw system_error {errno, system_category(), "fstat failed"};
            }
            size = static_cast<size_t>(sb.st_size);
        }

        capacity = size + expand_size;

        if (ftruncate(fd, static_cast<off_t>(capacity)) == -1)
        {
            throw system_error {errno, system_category(), "ftruncate failed"};
        }

        void* addr = mmap(nullptr, capacity, PROT_WRITE, MAP_SHARED, fd, 0);
        if (addr == MAP_FAILED) { throw system_error {errno, system_category(), "mmap failed"}; }
        mapped_ptr = static_cast<char*>(addr);
    }

public:
    mmap_writer(const filesystem::path& filename, bool truncate) { open(filename, truncate); }

    ~mmap_writer() { close(); }

    [[nodiscard]]
    bool is_open() const
    {
        return mapped_ptr != nullptr;
    }

    void close() noexcept
    {
        if (is_open())
        {
            ::munmap(mapped_ptr, capacity);
            ftruncate(fd, static_cast<off_t>(size));
            ::close(fd);
            mapped_ptr = nullptr;
            fd = -1;
            size = 0;
            capacity = 0;
        }
    }

    void open(const filesystem::path& filename, bool truncate)
    {
        close();
        open_file(filename, truncate);
    }

    void write(span<const char> buf)
    {
        if (size + buf.size() > capacity) { expand(size + buf.size()); }
        memcpy(mapped_ptr + size, buf.data(), buf.size());
        size += buf.size();
    }

    void flush(bool async = false)
    {
        if (msync(mapped_ptr, size, async ? MS_ASYNC : MS_SYNC) == -1)
        {
            throw system_error {errno, system_category(), "msync failed"};
        }
    }

    void reserve(size_t new_size) { expand(new_size); }
    void set_expand_size(size_t new_expand_size) { expand_size = new_expand_size; }
};

int main()
{
    mmap_writer writer("example.dat", true);
    const char* data1 = "Hello, mmap!";
    writer.write(std::span<const char>(data1, strlen(data1)));

    writer.flush();
}
