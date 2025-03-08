#include "../mmap_writer.hpp"
#include <cassert>
#include <fstream>
#include <iostream>

void test_open_and_close()
{
    const std::filesystem::path test_file = "test_file.txt";

    mmap_writer writer(test_file, true);
    assert(writer.is_open());
    writer.close();
    assert(!writer.is_open());

    std::filesystem::remove(test_file);
}

void test_write_and_pwrite()
{
    const std::filesystem::path test_file = "test_file.txt";

    mmap_writer writer(test_file, true);
    assert(writer.is_open());

    std::string data = "Hello, mmap_writer!";
    writer.write(data);

    std::string more_data = " More data.";
    writer.pwrite(data.size(), more_data);

    writer.seek(more_data.size(), mmap_writer::seekdir::cur);

    writer.write(data);
    writer.write(more_data);

    writer.close();

    std::ifstream ifs(test_file);

    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    assert(content == "Hello, mmap_writer! More data.Hello, mmap_writer! More data.");

    std::filesystem::remove(test_file);
}

void test_seek_and_tell()
{
    const std::filesystem::path test_file = "test_file.txt";

    mmap_writer writer(test_file, true);
    assert(writer.is_open());

    std::string data = "Hello, mmap_writer!";
    writer.write(data);
    writer.flush();

    writer.seek(7);
    assert(writer.tell() == 7);

    writer.seek(10, mmap_writer::seekdir::beg);
    assert(writer.tell() == 10);
    writer.seek(0, mmap_writer::seekdir::beg);
    assert(writer.tell() == 0);
    writer.seek(-1, mmap_writer::seekdir::beg);
    assert(writer.tell() == 0);


    writer.seek(5, mmap_writer::seekdir::cur);
    assert(writer.tell() == 5);
    writer.seek(-10, mmap_writer::seekdir::cur);
    assert(writer.tell() == 0);
    writer.seek(data.size(), mmap_writer::seekdir::cur);
    assert(writer.tell() == data.size());

    writer.seek(-1024, mmap_writer::seekdir::end);
    assert(writer.tell() == 0);
    writer.seek(-5, mmap_writer::seekdir::end);
    assert(writer.tell() == data.size() - 5);
    writer.seek(10, mmap_writer::seekdir::end);
    assert(writer.tell() == data.size() + 10);

    writer.close();

    std::filesystem::remove(test_file);
}

void test_expand_and_shrink()
{
    const std::filesystem::path test_file = "test_file.txt";

    mmap_writer writer(test_file, true);
    assert(writer.is_open());

    std::string data = "Hello, mmap_writer!";
    writer.reserve(100);
    writer.write(data);

    assert(writer.tell() == data.size());

    writer.shrink_to_fit();
    assert(writer.tell() == data.size());


    writer.close();

    std::filesystem::remove(test_file);
}

void test_flush()
{
    const std::filesystem::path test_file = "test_file.txt";

    mmap_writer writer(test_file, true);
    assert(writer.is_open());

    std::string data = "Hello, mmap_writer!";
    writer.write(data);
    writer.flush();
    writer.shrink_to_fit();

    std::ifstream ifs(test_file);
    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    assert(content == "Hello, mmap_writer!");

    writer.close();

    std::filesystem::remove(test_file);
}

int main()
{
    test_open_and_close();
    test_write_and_pwrite();
    test_seek_and_tell();
    test_expand_and_shrink();
    test_flush();

    std::cout << "All tests passed!" << std::endl;
    return 0;
}
