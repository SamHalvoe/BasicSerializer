#pragma once

#include <type_traits>
#include <functional>
#include <cstring>

#include <expected.hpp>

// **** **** **** **** **** **** **** **** **** **** **** **** **** **** **** **** **** **** **** ****
//
// NOTE: Serializer, Deserializer, SerializerReference and DeserializerReference are only
//       valid until your underlying buffer (of uint8_t*) expires.
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

  enum class SerializerStatus : uint8_t
  {
    success = 0,
    writeOutOfRange,
    writeStringSizeOutOfRange
  };

  enum class DeserializerStatus : uint8_t
  {
    success = 0,
    readOutOfRange,
    readStringOutOfRange,
    readStringSizeOutOfRange,
    readStringOutIsNullptr,
    stringAllocationFailed,
    readIsEnumFunIsNullptr,
    readIsEnumFunFalse
  };

  class StatusPrinter
  {
    public:
      static const char* message(SerializerStatus in_code)
      {
        switch (in_code)
        {
          case SerializerStatus::success:                   return "operation successful";
          case SerializerStatus::writeOutOfRange:           return "write operation out of range";
          case SerializerStatus::writeStringSizeOutOfRange: return "write string size operation out of range";
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
      Type* m_element; // ToDo: Maybe change to "Type* const"?!?
    
    private:
      // in_element must NOT be nullptr!
      SerializerReference(Type* in_element) : m_element(in_element)
      {}

    public:
      SerializerReference() = delete;
      
      Type read() const
      {
        return *m_element;
      }
      
      void write(Type in_value)
      {
        *m_element = in_value;
      }

      template<size_t tc_bufferSize>
      friend class Serializer;
  };

  template<size_t tc_bufferSize>
  class Serializer
  {
    private:
      size_t m_cursor = 0;
      uint8_t* m_begin; // ToDo: Maybe change to "uint8_t* const"?!?

    public:
      Serializer() = delete;
      Serializer(uint8_t* out_begin) : m_begin(out_begin)
      {}
      
      void reset()
      {
        m_cursor = 0;
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

      uint8_t* getBuffer()
      {
        return m_begin;
      }

      const uint8_t* getBuffer() const
      {
        return m_begin;
      }

      uint8_t* getBufferWithOffset()
      {
        return m_begin + m_cursor;
      }

      const uint8_t* getBufferWithOffset() const
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
      tl::expected<SerializerReference<Type>, SerializerStatus> skip()
      {
        static_assert(std::is_arithmetic<Type>::value, "Type must be arithmetic!");
        if (m_cursor + sizeof(Type) > tc_bufferSize) { return tl::make_unexpected(SerializerStatus::writeOutOfRange); }
        
        SerializerReference<Type> element(reinterpret_cast<Type*>(m_begin + m_cursor));
        m_cursor = m_cursor + sizeof(Type);
        return element;
      }

      template<typename Type>
      SerializerStatus write(Type in_value)
      {
        static_assert(std::is_arithmetic<Type>::value, "Type must be arithmetic!");
        if (m_cursor + sizeof(Type) > tc_bufferSize) { return SerializerStatus::writeOutOfRange; }

        *reinterpret_cast<Type*>(m_begin + m_cursor) = in_value;
        m_cursor = m_cursor + sizeof(Type);
        return SerializerStatus::success;
      }

      template<typename Type>
      SerializerStatus writeEnum(Type in_value)
      {
        using UnderlyingType = typename std::underlying_type<Type>::type;
        static_assert(std::is_enum<Type>::value && std::is_arithmetic<UnderlyingType>::value, "Type must be an enum and underlying type must be arithmetic!");
        if (m_cursor + sizeof(UnderlyingType) > tc_bufferSize) { return SerializerStatus::writeOutOfRange; }

        *reinterpret_cast<UnderlyingType*>(m_begin + m_cursor) = static_cast<UnderlyingType>(in_value);
        m_cursor = m_cursor + sizeof(UnderlyingType);
        return SerializerStatus::success;
      }
      
      template<typename SizeType>
      SerializerStatus writeStr(const char* in_string, SizeType in_size)
      {
        static_assert(isSizeType<SizeType>(), "SizeType must be an unsigned int!");
        if (m_cursor + sizeof(SizeType) + in_size > tc_bufferSize) { return SerializerStatus::writeOutOfRange; }
        
        if (write<SizeType>(in_size) != SerializerStatus::success) { return SerializerStatus::writeStringSizeOutOfRange; }
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
      const uint8_t* m_begin; // ToDo: Maybe change to "const uint8_t* const"?!?

    public:
      Deserializer() = delete;
      Deserializer(const uint8_t* in_begin) : m_begin(in_begin)
      {}
      
      void reset()
      {
        m_cursor = 0;
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

      const uint8_t* getBuffer() const
      {
        return m_begin;
      }

      const uint8_t* getBufferWithOffset() const
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
      DeserializerStatus skip()
      {
        static_assert(std::is_arithmetic<Type>::value, "Type must be arithmetic!");
        if (m_cursor + sizeof(Type) > tc_bufferSize) { return DeserializerStatus::readOutOfRange; }

        m_cursor = m_cursor + sizeof(Type);
        return DeserializerStatus::success;
      }

      template<typename Type>
      tl::expected<Type, DeserializerStatus> read()
      {
        static_assert(std::is_arithmetic<Type>::value, "Type must be arithmetic!");
        if (m_cursor + sizeof(Type) > tc_bufferSize) { return tl::make_unexpected(DeserializerStatus::readOutOfRange); }
        
        Type value = *reinterpret_cast<const Type*>(m_begin + m_cursor);
        m_cursor = m_cursor + sizeof(Type);
        return value;
      }

      template<typename Type, typename UnderlyingType = typename std::underlying_type<Type>::type>
      tl::expected<Type, DeserializerStatus> readEnum(const std::function<bool(UnderlyingType)>& fun_isEnumValue)
      {
        static_assert(std::is_same<UnderlyingType, typename std::underlying_type_t<Type>>::value, "UnderlyingType is not underlying type of Type!");
        static_assert(std::is_enum<Type>::value && std::is_arithmetic<UnderlyingType>::value, "Type must be an enum and underlying type must be arithmetic!");
        if (fun_isEnumValue == nullptr) { return tl::make_unexpected(DeserializerStatus::readIsEnumFunIsNullptr); }
        if (m_cursor + sizeof(UnderlyingType) > tc_bufferSize) { tl::make_unexpected(DeserializerStatus::readOutOfRange); }
        
        UnderlyingType value = *reinterpret_cast<const UnderlyingType*>(m_begin + m_cursor);
        if (not fun_isEnumValue(value)) { return tl::make_unexpected(DeserializerStatus::readIsEnumFunFalse); }
        m_cursor = m_cursor + sizeof(UnderlyingType);
        return static_cast<Type>(value);
      }
      
      template<typename SizeType>
      tl::expected<SizeType, DeserializerStatus> readStr(SizeType in_maxStringSize, char* out_string)
      {
        static_assert(isSizeType<SizeType>(), "Type must be an unsigned int!");
        if (out_string == nullptr) { return tl::make_unexpected(DeserializerStatus::readStringOutIsNullptr); }
        if (m_cursor + sizeof(SizeType) + in_maxStringSize > tc_bufferSize) { return tl::make_unexpected(DeserializerStatus::readStringOutOfRange); }
        
        auto sizeElement = read<SizeType>();
        if (not sizeElement.has_value()) { return tl::make_unexpected(DeserializerStatus::readStringSizeOutOfRange); }
        const SizeType size = sizeElement.value() < in_maxStringSize ? sizeElement.value() : in_maxStringSize - 1;
        
        std::memcpy(out_string, m_begin + m_cursor, size);
        out_string[size] = '\0';
        m_cursor = m_cursor + size;
        return size;
      }
      
      template<typename SizeType>
      tl::expected<String, DeserializerStatus> readStr(SizeType in_maxStringSize)
      {
        static_assert(isSizeType<SizeType>(), "Type must be an unsigned int!");
        if (m_cursor + sizeof(SizeType) + in_maxStringSize > tc_bufferSize) { return tl::make_unexpected(DeserializerStatus::readStringOutOfRange); }
        
        auto sizeElement = read<SizeType>();
        if (not sizeElement.has_value()) { return tl::make_unexpected(DeserializerStatus::readStringSizeOutOfRange); }
        const SizeType size = sizeElement.value() <= in_maxStringSize ? sizeElement.value() : in_maxStringSize - 1;
        
#if defined(ARDUINO_TEENSY41)
        String string;
        string.copy(reinterpret_cast<const char*>(m_begin + m_cursor), size);
#else
        String string(reinterpret_cast<const char*>(m_begin + m_cursor), size);
#endif
        m_cursor = m_cursor + size;
        return string;
      }
  };
}
