#include "../mmap_writer.hpp"
#include <cassert>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

void test_write_and_pwrite()
{
    const std::filesystem::path test_file = "test_file.txt";

    {
        mmap_writer writer(test_file.string(), true);

        std::string data = "Hello, mmap_writer!";
        writer.write(data);

        std::string more_data = " More data.";
        writer.pwrite(more_data, data.size());

        writer.seek(more_data.size(), mmap_writer::cur);

        writer.write(data);
        writer.write(more_data);
    }


    {
        int fd = ::open(test_file.string().c_str(), O_RDWR);

        assert(fd != -1);

        mmap_writer writer(test_file.string(), false);

        std::string data = "Hello, mmap_writer!";
        writer.write(data);

        std::string more_data = " More data.";
        writer.pwrite(more_data, writer.size());

        writer.seek(more_data.size(), mmap_writer::cur);

        writer.write(data);
        writer.write(more_data);

        assert(::close(fd) != -1);
    }


    std::ifstream ifs(test_file);

    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    std::string expected = "Hello, mmap_writer! More data.Hello, mmap_writer! More data.";
    expected += expected;
    assert(content == expected);

    std::filesystem::remove(test_file);
}

void test_seek_and_tell()
{
    const std::filesystem::path test_file = "test_file.txt";

    {
        mmap_writer writer(test_file.string(), true);

        std::string data = "Hello, mmap_writer!";
        writer.write(data);
        writer.flush();

        writer.seek(7);
        assert(writer.tell() == 7);

        writer.seek(10, mmap_writer::beg);
        assert(writer.tell() == 10);
        writer.seek(0, mmap_writer::beg);
        assert(writer.tell() == 0);
        writer.seek(-1, mmap_writer::beg);
        assert(writer.tell() == 0);


        writer.seek(5, mmap_writer::cur);
        assert(writer.tell() == 5);
        writer.seek(-10, mmap_writer::cur);
        assert(writer.tell() == 0);
        writer.seek(data.size(), mmap_writer::cur);
        assert(writer.tell() == data.size());

        writer.seek(-1024, mmap_writer::end);
        assert(writer.tell() == 0);
        writer.seek(-5, mmap_writer::end);
        assert(writer.tell() == data.size() - 5);
        writer.seek(10, mmap_writer::end);
        assert(writer.tell() == data.size() + 10);
    }

    std::filesystem::remove(test_file);
}

void test_expand_and_shrink()
{
    const std::filesystem::path test_file = "test_file.txt";

    {
        size_t initial_size = 100;
        mmap_writer writer(test_file.string(), true, initial_size);

        std::string data = "Hello, mmap_writer!";
        writer.write(data);

        assert(writer.tell() == data.size());
        writer.seek(0);
        assert(writer.size() == data.size());
        assert(writer.capacity() == initial_size);

        writer.shrink_to_fit();
        assert(writer.size() == data.size());
        assert(writer.capacity() == data.size());
    }

    std::filesystem::remove(test_file);
}

void test_flush()
{
    const std::filesystem::path test_file = "test_file.txt";

    {
        mmap_writer writer(test_file.string(), true, 1024);

        std::string data = "Hello, mmap_writer!";
        writer.write(data);
        writer.flush();
        writer.shrink_to_fit();

        std::ifstream ifs(test_file);
        std::string content((std::istreambuf_iterator<char>(ifs)),
                            std::istreambuf_iterator<char>());
        assert(content == "Hello, mmap_writer!");
    }

    std::filesystem::remove(test_file);
}

int main()
{
    test_write_and_pwrite();
    test_seek_and_tell();
    test_expand_and_shrink();
    test_flush();

    std::cout << "All tests passed!" << '\n';
}
