#include "mmap_reader.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

using namespace std;

void generate_test_file(const string& filename, size_t size)
{
    ofstream out(filename);
    for (size_t i = 0; i < size; ++i) { out << "This is line " << i << "\n"; }
}

void test_fstream_read_whole(const string& filename)
{
    auto start = chrono::high_resolution_clock::now();

    ifstream in(filename, ios::binary);
    if (!in)
    {
        cerr << "Failed to open file: " << filename << endl;
        return;
    }

    // 获取文件大小
    in.seekg(0, ios::end);
    size_t file_size = in.tellg();
    in.seekg(0, ios::beg);

    string content(file_size, '\0');
    in.read(&content[0], file_size);

    auto end = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(end - start).count();
    cout << "fstream read whole file took " << duration << " ms\n";
}

void test_fstream_read_lines(const string& filename)
{
    auto start = chrono::high_resolution_clock::now();

    ifstream in(filename);
    string line;
    string content;

    while (getline(in, line)) { content += line + '\n'; }

    auto end = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(end - start).count();
    cout << "fstream read lines took " << duration << " ms\n";
}

void test_fstream_read_chars(const string& filename)
{
    auto start = chrono::high_resolution_clock::now();

    ifstream in(filename);
    char ch;
    string content;

    while (in.get(ch)) { content += ch; }

    auto end = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(end - start).count();
    cout << "fstream read chars took " << duration << " ms\n";
}

void test_mmap_read_whole(const string& filename)
{
    auto start = chrono::high_resolution_clock::now();

    MmapReader mmapReader(filename);
    string content = mmapReader.str();

    auto end = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(end - start).count();
    cout << "MmapReader read whole file took " << duration << " ms\n";
}

void test_mmap_read_lines(const string& filename)
{
    auto start = chrono::high_resolution_clock::now();

    MmapReader mmapReader(filename);
    MmapLineReader lineReader(mmapReader);
    string content;

    for (string_view line : lineReader)
    {
        content += line;
        content += '\n';
    }

    auto end = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(end - start).count();
    cout << "MmapReader read lines took " << duration << " ms\n";
}


void test_mmap_read_chars(const string& filename)
{
    auto start = chrono::high_resolution_clock::now();

    MmapReader mmapReader(filename);
    MmapCharReader charReader(mmapReader);
    string content;

    for (char ch : charReader) { content += ch; }

    auto end = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(end - start).count();
    cout << "MmapReader read chars took " << duration << " ms\n";
}

int main()
{
    const string filename = "large_test.txt";
    const size_t num_lines = 1000000; // 生成100万行

    generate_test_file(filename, num_lines);

    cout << "Testing fstream read whole file:\n";
    test_fstream_read_whole(filename);

    cout << "\nTesting fstream read lines:\n";
    test_fstream_read_lines(filename);

    cout << "\nTesting fstream read chars:\n";
    test_fstream_read_chars(filename);

    cout << "\nTesting MmapReader read whole file:\n";
    test_mmap_read_whole(filename);

    cout << "\nTesting MmapReader read lines:\n";
    test_mmap_read_lines(filename);

    cout << "\nTesting MmapReader read chars:\n";
    test_mmap_read_chars(filename);
}

// Testing fstream read whole file:
// fstream read whole file took 11 ms

// Testing fstream read lines:
// fstream read lines took 65 ms

// Testing fstream read chars:
// fstream read chars took 225 ms

// Testing MmapReader read whole file:
// MmapReader read whole file took 4 ms

// Testing MmapReader read lines:
// MmapReader read lines took 24 ms

// Testing MmapReader read chars:
// MmapReader read chars took 101 ms
