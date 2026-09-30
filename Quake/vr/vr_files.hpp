#pragma once

// vr_files.hpp -- the VR code's file access, on Zancle strings and vectors: UTF-8 paths (as the engine's: com_gamedir)
// through the engine's Sys_* calls (wide paths on Windows). It replaced std::filesystem and the file streams (the
// Zancle migration, ROUND21.md): each call does what the std:: one it replaced did (named on each).

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/StringView.hpp"
#include "Zancle/Vocabulary/FunctionRef.hpp"

namespace qvr::files
{

// The whole file, its bytes as they are (std::ifstream, std::ios::binary). False: it could not be opened or read.
[[nodiscard]] bool readBytes(const char* path, za::Vector<unsigned char>& out);
[[nodiscard]] bool readBytes(const char* path, za::Vector<char>& out);

// The whole file as text: as a text-mode stream reads it (std::ifstream: on Windows CR LF read as LF).
[[nodiscard]] bool readText(const char* path, za::String& out);

// The text's pieces as std::getline(stream, piece, delimiter) gives them: each up to the delimiter (not included), the
// last one without it when it is not empty. f(za::StringView piece).
template <typename F>
void forPieces(const za::StringView text, const char delimiter, F&& f)
{
    za::SizeT start = 0;
    while(start < text.size())
    {
        za::SizeT end = text.find(delimiter, start);
        if(end == za::StringView::nPos)
        {
            end = text.size();
        }
        f(text.substrByPosLen(start, end - start));
        start = end + 1;
    }
}

// Its lines (std::getline's): forPieces with LF.
template <typename F>
void forLines(const za::StringView text, F&& f)
{
    forPieces(text, '\n', static_cast<F&&>(f));
}

// A text's words, read in turn as an std::istringstream's >> reads them (separated by the "C" locale's white space):
// a word, or a number (the whole word read by strtof, strtod or strtol, as a stream's number is for the words the game
// writes). Once a read fails (no word left, or not a number) every later one fails too, as a stream's.
class Words
{
public:
    explicit Words(za::StringView text) : text_{text} {}

    bool next(za::StringView& word);
    bool next(za::String& word);
    bool next(float& value);
    bool next(double& value);
    bool next(int& value);

    [[nodiscard]] explicit operator bool() const { return !failed_; }

    // `>>` chains, as a stream's.
    template <typename T>
    Words& operator>>(T& value)
    {
        next(value);
        return *this;
    }

private:
    za::StringView text_;
    bool failed_{false};
};

// Written whole, the file replaced (std::ofstream, std::ios::trunc; text: LF written as CR LF on Windows). False: it
// could not be opened or written (a partly written file is left).
[[nodiscard]] bool writeBytes(const char* path, const void* data, za::SizeT size);
[[nodiscard]] bool writeText(const char* path, za::StringView text);

// std::filesystem::create_directories: every missing directory of the path made. True: it is a directory now.
bool createDirectories(const char* path);

[[nodiscard]] bool exists(const char* path);      // a file or a directory (std::filesystem::exists)
[[nodiscard]] bool isDirectory(const char* path); // (std::filesystem::is_directory)
[[nodiscard]] bool isFile(const char* path);      // (std::filesystem::is_regular_file)

bool remove(const char* path);    // a file or an empty directory (std::filesystem::remove); true: it was removed
bool removeAll(const char* path); // a file, or a directory and all it holds (std::filesystem::remove_all)
bool rename(const char* from, const char* to); // `to` replaced when it is there (std::filesystem::rename)

// The entries of a directory (not "." and ".."), in the order the system lists them (std::filesystem::
// directory_iterator): f(name, isDirectory).
void forEachEntry(const char* dir, za::FunctionRef<void(const char* name, bool isDirectory)> f);

// The last write time: a stamp to compare (std::filesystem::last_write_time's resolution); 0 when it is not there.
[[nodiscard]] za::I64 lastWriteTime(const char* path);

// The part after the last '/' or '\' (std::filesystem::path::filename).
[[nodiscard]] za::StringView fileName(za::StringView path);

} // namespace qvr::files
