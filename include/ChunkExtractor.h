#ifndef CHUNK_EXTRACTOR_H
#define CHUNK_EXTRACTOR_H

#include "Record.h"

#include <climits>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <streambuf>

constexpr inline std::size_t DEFAULT_BUFFER_SIZE = 655360UL;
enum FileType : unsigned int;

template<class RecordType, class Extractor, class El,
         class Tr = std::char_traits<El>,
         std::size_t CHUNK_SIZE = DEFAULT_BUFFER_SIZE>
class ParsingInputStreambuf : public std::basic_streambuf<El, Tr>
{
  public:
    using traits_type = Tr;
    using char_type = typename traits_type::char_type;
    using int_type = typename traits_type::int_type;
    using off_type = typename traits_type::off_type;
    using buff_type =
        ParsingInputStreambuf<RecordType, Extractor, El, Tr, CHUNK_SIZE>;
    using pos_type = typename traits_type::pos_type;
    using istream_reference = std::basic_istream<El, Tr>&;
    using ExtractedType = typename Extractor::ExtractedType;

  private:
    std::streambuf* m_Source;
    Extractor m_Extractor;
    char_type m_Buffer[CHUNK_SIZE];

  public:
    explicit ParsingInputStreambuf(istream_reference istrm, std::string streamId,
                                   FileType strmType)
        : m_Source(istrm.rdbuf()), m_Extractor(streamId, strmType)
    {
        m_Source->pubsetbuf(0, 0);
        char_type* ptr = &m_Buffer[0];
        this->setg(ptr, ptr, ptr);
    }
    virtual ~ParsingInputStreambuf(){};

    ExtractedType extract()
    {
        // this will cause buffer fill
        auto* curr = this->gptr();
        auto* begin = curr;
        auto* end = this->egptr();
        auto* fixend = &m_Buffer[CHUNK_SIZE - 1];
        char* ptr = &m_Buffer[0];

        if(begin == end) {
            underflow(); //<--- yep
            curr = this->gptr();
            begin = curr;
            end = this->egptr();
        }

        char_type ch = *curr;

        while(ch != EOF) {
            while(ch != '\n' && curr < end && ch != EOF) {
                ch = *++curr;
            }
            if(ch == '\n' || ch == traits_type::eof()) {
                this->setg(ptr, ++curr, end);
                return m_Extractor(begin, curr - begin - 2);
            }
            if(curr < fixend) {
                this->setg(ptr, curr, curr);
                ;
                return m_Extractor(begin, curr - begin);
            } else {
                ++curr;
                memmove(ptr, begin, curr - begin - 1);
                end = ptr + (curr - begin - 1);
                this->setg(ptr, end, end);
            }
            ch = underflow();
            begin = ptr;
            curr = end; // this->gptr();
            end = this->egptr();
        }
        return m_Extractor(curr, 0);
    }
    char* current() const { return this->gptr(); }

  protected:
    virtual std::streamsize showmanyc() override
    {
        const auto* curr = this->gptr();
        const auto* end = this->egptr();
        return (curr <= end) ? (end - curr) : 0;
    }

    virtual int underflow() override
    {
        int result = traits_type::eof();
        if(this->gptr() < this->egptr())
            result = *this->gptr();
        else {
            auto ptr = &m_Buffer[0];
            auto curr = this->gptr();

            std::streamsize readCount = (1 + (&m_Buffer[CHUNK_SIZE - 1]) - curr);
            if(readCount == 0) {
                readCount = CHUNK_SIZE;
                curr = ptr;
            };
            readCount = xsgetn(curr, readCount);
            if(readCount) {
                this->setg(ptr, curr, curr + readCount); //+readCount-1);
                result = static_cast<int>(*this->gptr());
            }
        }
        return result;
    }
    virtual std::streamsize xsgetn(char_type* s, std::streamsize count)
        override
    {
        if(count == 0) {
            return 0;
        }
        auto toRead = count;
        toRead = m_Source->sgetn(s, count);
        return toRead;
    }
};

template<class RecordType, class El, class Tr = std::char_traits<El>,
         std::size_t CHUNK_SIZE = DEFAULT_BUFFER_SIZE>
class RecordExtractFunctor
{
  private:
    const std::string m_streamId;
    const FileType m_streamType;

  public:
    using ExtractedType = RecordType;
    RecordExtractFunctor(const std::string& streamId, FileType strmType)
        : m_streamId(streamId), m_streamType(strmType) {}
    RecordType operator()(const char* RecPtr, std::streamsize length)
    {
        return new(std::nothrow)
            typename ::std::remove_pointer<RecordType>::type{
                RecPtr, length, m_streamId, m_streamType};
    }
};

template<class RecordType, class Extractor, class El,
         class Tr = std::char_traits<El>,
         std::size_t CHUNK_SIZE = DEFAULT_BUFFER_SIZE>
class ParsingIstreamBase : virtual public std::basic_ios<El, Tr>
{
  public:
    using istream_reference = std::basic_istream<El, Tr>&;
    using parsing_buffer =
        ParsingInputStreambuf<RecordType, Extractor, El, Tr, CHUNK_SIZE>;
    // using RecordType        = typename parsing_buffer::RecordType;
  private:
    parsing_buffer m_Buffer;

  public:
    ParsingIstreamBase(istream_reference istrm, const std::string& strmId,
                       FileType strmType)
        : m_Buffer(istrm, strmId, strmType)
    {
        this->init(&m_Buffer);
    }

    parsing_buffer* rdbuf() { return &m_Buffer; }
};

template<class RecordType, class Extractor, class El,
         class Tr = std::char_traits<El>,
         std::size_t CHUNK_SIZE = DEFAULT_BUFFER_SIZE>
class BasicParsingIstream
    : public ParsingIstreamBase<RecordType, Extractor, El, Tr, CHUNK_SIZE>,
      public std::basic_istream<El, Tr>
{
  public:
    using ParsingIstreamBaseType =
        ParsingIstreamBase<RecordType, Extractor, El, Tr, CHUNK_SIZE>;
    using istream_type = std::basic_istream<El, Tr>;
    using istream_reference = istream_type&;
    using RecType = RecordType;
    BasicParsingIstream(istream_reference istrm, const std::string& strmId,
                        FileType strmType)
        : ParsingIstreamBaseType(istrm, strmId, strmType),
          istream_type(ParsingIstreamBaseType::rdbuf()) {}
    using ExtractedType = typename Extractor::ExtractedType;

    ExtractedType extract()
    {
        //  this->peek();
        ExtractedType record = this->rdbuf()->extract();
        this->peek();
        return record;
    }
};

template<class RecordType>
struct IFileReader
{
    virtual RecordType getRecord() = 0;
    virtual bool eof() const = 0;
    virtual bool good() const = 0;
    virtual std::string getId() const = 0;
    virtual ~IFileReader(){};
};

template<class RecordType, class Extractor, class El,
         class Tr = std::char_traits<El>,
         std::size_t CHUNK_SIZE = DEFAULT_BUFFER_SIZE>
class FileParser : public IFileReader<typename Extractor::ExtractedType>
{
    using ParsingInputStream =
        BasicParsingIstream<RecordType, Extractor, El, Tr, CHUNK_SIZE>;
    using ExtractedType = typename Extractor::ExtractedType;
    std::string m_readerId;
    FileType m_streamType;
    std::ifstream m_inputFileStream;
    ParsingInputStream m_ParserStream;

  public:
    FileParser(const std::string& fname, const std::string& Id,
               FileType strmType)
        : m_readerId(Id),
          m_streamType(strmType),
          m_inputFileStream(),
          m_ParserStream(m_inputFileStream, m_readerId, m_streamType)
    {
        std::ios_base::sync_with_stdio(false);
        m_inputFileStream.open(fname, std::ios::in | std::ios::binary);
    }

    ~FileParser()
    {
        //        m_inputFileStream.close();
    }

    FileParser(const FileParser&) = delete;
    FileParser& operator=(const FileParser&) = delete;

    explicit operator bool() const
    {
        return m_ParserStream.good() && m_inputFileStream.good();
    }

    virtual bool eof() const override { return m_ParserStream.eof(); }
    virtual bool good() const override
    {
        return m_ParserStream.good() && m_inputFileStream.good();
    }

    virtual std::string getId() const override { return m_readerId; }

    virtual ExtractedType getRecord() override
    {
        return m_ParserStream.extract();
    }
};
using RecordParser =
    FileParser<Record,
               RecordExtractFunctor<Record*, char, std::char_traits<char>,
                                    DEFAULT_BUFFER_SIZE>,
               char, std::char_traits<char>, DEFAULT_BUFFER_SIZE>;

// class FileParser
#endif // CHUNK_EXTRACTOR_H
