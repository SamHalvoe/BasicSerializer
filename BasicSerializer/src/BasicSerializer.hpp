#pragma once

#include <Arduino.h>

#include <type_traits>
#include <functional>
#include <cstring>

#include <expected.hpp>

// **** **** **** **** **** **** **** **** **** **** **** **** **** **** **** **** **** **** **** ****
//
// NOTE: Serializer, Deserializer and SerializerReference are only
//       valid until your underlying buffer (of unsigned char* / const unsigned char*) expires.
//       When the underlying buffer is gone, use of these types will cause undefined behaviour!
//
// **** **** **** **** **** **** **** **** **** **** **** **** **** **** **** **** **** **** **** ****

namespace halvoe
{
  template<typename SizeType>
  static constexpr bool isSizeType()
  {
    return std::is_same<SizeType, size_t>::value   ||
           std::is_same<SizeType, uint8_t>::value  ||
           std::is_same<SizeType, uint16_t>::value ||
           std::is_same<SizeType, uint32_t>::value ||
           std::is_same<SizeType, uint64_t>::value;
  }

  class StringFromBytes : public String
  {
    public:
      StringFromBytes() = delete;

      StringFromBytes(const unsigned char* in_bytes, unsigned int in_size) : String()
      {
        buffer = nullptr;
        len = 0;
        capacity = 0;

        if (reserve(in_size))
        {
          len = in_size;
          std::memcpy(buffer, in_bytes, in_size);
          buffer[len] = '\0';
        }
        else
        {
          if (buffer)
          {
            free(buffer);
            buffer = nullptr;
          }

          len = 0;
          capacity = 0;
        }
      }
  };

  enum class SerializerStatus : uint8_t
  {
    success = 0,
    writeOutOfRange,
    writeStringOutOfRange,
    writeStringSizeOutOfRange,
    writeStringIsNullptr
  };

  enum class DeserializerStatus : uint8_t
  {
    success = 0,
    readOutOfRange,
    readStringOutOfRange,
    readStringSizeOutOfRange,
    readStringOutIsNullptr,
    readStringOutOfMemory,
    stringAllocationFailed,
    readIsEnumFunIsNullptr,
    readIsEnumFunFalse
  };

  template<typename ExpectedType>
  SerializerStatus getStatusFromExpected(const tl::expected<ExpectedType, SerializerStatus>& in_expected)
  {
    return in_expected.has_value() ? SerializerStatus::success : in_expected.error();
  }

  template<typename ExpectedType>
  DeserializerStatus getStatusFromExpected(const tl::expected<ExpectedType, DeserializerStatus>& in_expected)
  {
    return in_expected.has_value() ? DeserializerStatus::success : in_expected.error();
  }

  class StatusPrinter
  {
    public:
      static const char* message(SerializerStatus in_code)
      {
        switch (in_code)
        {
          case SerializerStatus::success:                   return "operation successful";
          case SerializerStatus::writeOutOfRange:           return "write operation out of range";
          case SerializerStatus::writeStringOutOfRange:     return "write string operation out of range";
          case SerializerStatus::writeStringSizeOutOfRange: return "write string size operation out of range";
          case SerializerStatus::writeStringIsNullptr:      return "write string string is nullptr";
          default:                                          return "invalid SerializerStatus";
        }
      }

      static const char* message(DeserializerStatus in_code)
      {
        switch (in_code)
        {
          case DeserializerStatus::success:                  return "operation successful";
          case DeserializerStatus::readOutOfRange:           return "read operation out of range";
          case DeserializerStatus::readStringOutOfRange:     return "read string operation out of range";
          case DeserializerStatus::readStringSizeOutOfRange: return "read string size operation out of range";
          case DeserializerStatus::readStringOutIsNullptr:   return "read string out_parameter is nullptr";
          case DeserializerStatus::readStringOutOfMemory:    return "read string out of memory";
          case DeserializerStatus::stringAllocationFailed:   return "string allocation failed";
          case DeserializerStatus::readIsEnumFunIsNullptr:   return "read isEnum function is nullptr";
          case DeserializerStatus::readIsEnumFunFalse:       return "read isEnum function returned false";
          default:                                           return "invalid DeserializerStatus";
        }
      }
  };
  
  template<typename Type>
  class SerializerReference
  {
    static_assert(std::is_arithmetic<Type>::value, "Type must be arithmetic!");
    static_assert(not std::is_enum<Type>::value, "Type must not be an enum!");
    
    private:
      unsigned char* m_destination; // ToDo: Maybe change to "unsigned char* const"?!?
    
    private:
      // in_destination must NOT be nullptr!
      SerializerReference(unsigned char* in_destination) : m_destination(in_destination)
      {}

    public:
      SerializerReference() = delete;
      
      void write(Type in_value)
      {
        std::memcpy(m_destination, &in_value, sizeof(Type));
      }

      template<size_t tc_bufferSize>
      friend class Serializer;
  };

  template<size_t tc_bufferSize>
  class Serializer
  {
    private:
      size_t m_cursor = 0;
      unsigned char* m_begin;
      SerializerStatus m_status = SerializerStatus::success; // contains last error status

    private:
      inline SerializerStatus error(SerializerStatus in_error)
      {
        m_status = in_error;
        return m_status;
      }
      
      inline tl::unexpected<SerializerStatus> make_error(SerializerStatus in_error)
      {
        m_status = in_error;
        return tl::make_unexpected(in_error);
      }

    public:
      Serializer() = delete;
      Serializer(unsigned char* out_begin) : m_begin(out_begin)
      {}
      
      void reset()
      {
        m_cursor = 0;
        m_status = SerializerStatus::success;
      }

      void resetStatus()
      {
        m_status = SerializerStatus::success;
      }
      
      SerializerStatus getStatus() const
      {
        return m_status;
      }

      constexpr size_t getBufferSize() const
      {
        return tc_bufferSize;
      }

      size_t getBytesWritten() const
      {
        return m_cursor;
      }
      
      size_t getBytesLeft() const
      {
        return tc_bufferSize - m_cursor;
      }

      unsigned char* getBuffer()
      {
        return m_begin;
      }

      const unsigned char* getBuffer() const
      {
        return m_begin;
      }

      unsigned char* getBufferWithOffset()
      {
        return m_begin + m_cursor;
      }

      const unsigned char* getBufferWithOffset() const
      {
        return m_begin + m_cursor;
      }

      bool fitsInBuffer(size_t in_size) const
      {
        return m_cursor + in_size <= tc_bufferSize;
      }
      
      template<typename Type>
      bool fitsInBuffer() const
      {
        static_assert(std::is_arithmetic<Type>::value, "Type must be arithmetic!");
        return m_cursor + sizeof(Type) <= tc_bufferSize;
      }

      template<typename Type>
      tl::expected<SerializerReference<Type>, SerializerStatus> reserve()
      {
        static_assert(std::is_arithmetic<Type>::value, "Type must be arithmetic!");
        if (m_cursor + sizeof(Type) > tc_bufferSize) { return make_error(SerializerStatus::writeOutOfRange); }
        
        SerializerReference<Type> element(m_begin + m_cursor);
        m_cursor = m_cursor + sizeof(Type);
        return element;
      }

      template<typename Type>
      SerializerStatus write(Type in_value)
      {
        static_assert(std::is_arithmetic<Type>::value, "Type must be arithmetic!");
        if (m_cursor + sizeof(Type) > tc_bufferSize) { return error(SerializerStatus::writeOutOfRange); }

        std::memcpy(m_begin + m_cursor, &in_value, sizeof(Type));
        m_cursor = m_cursor + sizeof(Type);
        return SerializerStatus::success;
      }

      template<typename Type>
      SerializerStatus writeEnum(Type in_value)
      {
        using UnderlyingType = typename std::underlying_type<Type>::type;
        static_assert(std::is_enum<Type>::value && std::is_arithmetic<UnderlyingType>::value, "Type must be an enum and underlying type must be arithmetic!");
        if (m_cursor + sizeof(UnderlyingType) > tc_bufferSize) { return error(SerializerStatus::writeOutOfRange); }

        std::memcpy(m_begin + m_cursor, &in_value, sizeof(UnderlyingType));
        m_cursor = m_cursor + sizeof(UnderlyingType);
        return SerializerStatus::success;
      }
      
      template<typename SizeType>
      SerializerStatus writeStr(const char* in_string)
      {
        static_assert(isSizeType<SizeType>(), "SizeType must be an unsigned int!");
        if (in_string == nullptr) { return error(SerializerStatus::writeStringIsNullptr); }
        size_t size = strnlen(in_string, getBytesLeft());
        if (m_cursor + sizeof(SizeType) + size > tc_bufferSize) { return error(SerializerStatus::writeStringOutOfRange); }
        
        if (write<SizeType>(size) != SerializerStatus::success) { return error(SerializerStatus::writeStringSizeOutOfRange); }
        std::memcpy(m_begin + m_cursor, in_string, size);
        m_cursor = m_cursor + size;
        return SerializerStatus::success;
      }

      template<typename SizeType>
      SerializerStatus writeStr(const char* in_string, SizeType in_size)
      {
        static_assert(isSizeType<SizeType>(), "SizeType must be an unsigned int!");
        if (in_string == nullptr) { return error(SerializerStatus::writeStringIsNullptr); }
        if (m_cursor + sizeof(SizeType) + in_size > tc_bufferSize) { return error(SerializerStatus::writeStringOutOfRange); }

        if (write<SizeType>(in_size) != SerializerStatus::success) { return error(SerializerStatus::writeStringSizeOutOfRange); }
        std::memcpy(m_begin + m_cursor, in_string, in_size);
        m_cursor = m_cursor + in_size;
        return SerializerStatus::success;
      }
      
      template<typename SizeType>
      SerializerStatus writeStr(const String& in_string)
      {
        return writeStr<SizeType>(in_string.c_str(), in_string.length());
      }
  };
  
  template<size_t tc_bufferSize>
  class Deserializer
  {
    private:
      size_t m_cursor = 0;
      const unsigned char* m_begin;
      DeserializerStatus m_status = DeserializerStatus::success; // contains last error status

    private:
      inline DeserializerStatus error(DeserializerStatus in_error)
      {
        m_status = in_error;
        return m_status;
      }

      inline tl::unexpected<DeserializerStatus> make_error(DeserializerStatus in_error)
      {
        m_status = in_error;
        return tl::make_unexpected(in_error);
      }

    public:
      Deserializer() = delete;
      Deserializer(const unsigned char* in_begin) : m_begin(in_begin)
      {}
      
      void reset()
      {
        m_cursor = 0;
        m_status = DeserializerStatus::success;
      }

      void resetStatus()
      {
        m_status = DeserializerStatus::success;
      }

      DeserializerStatus getStatus() const
      {
        return m_status;
      }

      constexpr size_t getBufferSize() const
      {
        return tc_bufferSize;
      }

      size_t getBytesRead() const
      {
        return m_cursor;
      }
      
      size_t getBytesLeft() const
      {
        return tc_bufferSize - m_cursor;
      }

      const unsigned char* getBuffer() const
      {
        return m_begin;
      }

      const unsigned char* getBufferWithOffset() const
      {
        return m_begin + m_cursor;
      }

      bool fitsInBuffer(size_t in_size) const
      {
        return m_cursor + in_size <= tc_bufferSize;
      }

      template<typename Type>
      bool fitsInBuffer() const
      {
        static_assert(std::is_arithmetic<Type>::value, "Type must be arithmetic!");
        return m_cursor + sizeof(Type) <= tc_bufferSize;
      }

      template<typename Type>
      DeserializerStatus discard()
      {
        static_assert(std::is_arithmetic<Type>::value, "Type must be arithmetic!");
        if (m_cursor + sizeof(Type) > tc_bufferSize) { return error(DeserializerStatus::readOutOfRange); }

        m_cursor = m_cursor + sizeof(Type);
        return DeserializerStatus::success;
      }

      template<typename Type>
      tl::expected<Type, DeserializerStatus> read()
      {
        static_assert(std::is_arithmetic<Type>::value, "Type must be arithmetic!");
        if (m_cursor + sizeof(Type) > tc_bufferSize) { return make_error(DeserializerStatus::readOutOfRange); }
        
        Type value = 0;
        std::memcpy(&value, m_begin + m_cursor, sizeof(Type));
        m_cursor = m_cursor + sizeof(Type);
        return value;
      }

      template<typename Type, typename UnderlyingType>
      tl::expected<Type, DeserializerStatus> readEnum(const std::function<bool(UnderlyingType)>& fun_isEnumValue)
      {
        static_assert(std::is_same<UnderlyingType, typename std::underlying_type_t<Type>>::value, "UnderlyingType is not underlying type of Type!");
        static_assert(std::is_enum<Type>::value && std::is_arithmetic<UnderlyingType>::value, "Type must be an enum and underlying type must be arithmetic!");
        if (fun_isEnumValue == nullptr) { return make_error(DeserializerStatus::readIsEnumFunIsNullptr); }
        if (m_cursor + sizeof(UnderlyingType) > tc_bufferSize) { make_error(DeserializerStatus::readOutOfRange); }
        
        UnderlyingType value = 0;
        std::memcpy(&value, m_begin + m_cursor, sizeof(UnderlyingType));
        if (not fun_isEnumValue(value)) { return make_error(DeserializerStatus::readIsEnumFunFalse); }
        m_cursor = m_cursor + sizeof(UnderlyingType);
        return static_cast<Type>(value);
      }
      
      template<typename SizeType>
      tl::expected<SizeType, DeserializerStatus> readStr(char* out_string, SizeType in_maxStringSize)
      {
        static_assert(isSizeType<SizeType>(), "Type must be an unsigned int!");
        if (out_string == nullptr) { return make_error(DeserializerStatus::readStringOutIsNullptr); }
        if (m_cursor + sizeof(SizeType) + in_maxStringSize > tc_bufferSize) { return make_error(DeserializerStatus::readStringOutOfRange); }
        
        auto sizeElement = read<SizeType>();
        if (not sizeElement.has_value()) { return make_error(DeserializerStatus::readStringSizeOutOfRange); }
        const SizeType size = sizeElement.value() < in_maxStringSize ? sizeElement.value() : in_maxStringSize - 1;
        
        std::memcpy(out_string, m_begin + m_cursor, size);
        out_string[size] = '\0';
        m_cursor = m_cursor + size;
        return size;
      }

      template<typename SizeType>
      tl::expected<SizeType, DeserializerStatus> readStr(char* out_string)
      {
        return readStr<SizeType>(out_string, getBytesLeft());
      }

      template<typename SizeType>
      tl::expected<String, DeserializerStatus> readStr(SizeType in_maxStringSize)
      {
        static_assert(isSizeType<SizeType>(), "Type must be an unsigned int!");
        if (m_cursor + sizeof(SizeType) + in_maxStringSize > tc_bufferSize) { return make_error(DeserializerStatus::readStringOutOfRange); }
        
        auto sizeElement = read<SizeType>();
        if (not sizeElement.has_value()) { return make_error(DeserializerStatus::readStringSizeOutOfRange); }
        const SizeType size = sizeElement.value() < in_maxStringSize ? sizeElement.value() : in_maxStringSize;

        StringFromBytes string(m_begin + m_cursor, size);
        if (not string) { return make_error(DeserializerStatus::readStringOutOfMemory); }

        m_cursor = m_cursor + size;
        return string;
      }

      template<typename SizeType>
      tl::expected<String, DeserializerStatus> readStr()
      {
        return readStr<SizeType>(getBytesLeft());
      }
  };
}
