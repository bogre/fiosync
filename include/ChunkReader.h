#ifndef CHUNK_READER_H
#define CHUNK_READER_H

#include "Record.h"

#include <climits>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <streambuf>
#include <string>

constexpr inline std::size_t DEFAULT_BUFFER_SIZE = 655360UL;
enum FileType : unsigned int;

template<class Extractor, class El, class Tr = std::char_traits<El>,
        std::size_t BUFFER_SIZE = DEFAULT_BUFFER_SIZE>
class ExtractingStreambuf : public std::basic_streambuf<El, Tr>
{
  public:
    using traits_type = Tr;
    using char_type = El;
    using int_type = typename traits_type::int_type;
    using off_type = typename traits_type::off_type;
    using record_type = typename Extractor::record_type;
    using buff_type =
        ExtractingStreambuf<Extractor, El, Tr, BUFFER_SIZE>;
    using pos_type = typename traits_type::pos_type;
    using istream_reference = std::basic_istream<El, Tr>&;
    typedef std::vector<char_type> TBuffer;
    typedef ring_buffer<int32_t> TJobQueue;

  private:
    static constexpr auto MAX_PUTBACK = 4U;

    // Allows serialized access to the underlying buffer.
    struct Serializer
    {
        istream_reference istream;
        std::mutex lock;
        // io_error            *error;
        off_type fileOfs;

        Serializer(istream_reference istream)
            : istream(istream),
              // error(NULL),
              fileOfs(0u)
        {
        }

        ~Serializer()
        {
            // delete error;
        }
    };
};

#endif
