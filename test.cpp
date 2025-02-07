#include "mmap_file.hpp"
#include <iostream>

int main()
{
    MmapReader in {"../test.txt"};
    MmapLineReader reader {in};
    MmapCharReader char_reader {in};

    for (auto line : reader) { std::cout << line << '\n'; }

    cout << "====================\n";
    for (auto c : char_reader) { std::cout << c; }
    cout << '\n';
}
