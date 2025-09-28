#pragma once

#include <Arduino.h>

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
    writeStringOutOfRange,
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
          case SerializerStatus::writeStringOutOfRange:     return "write string operation out of range";
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
      SerializerStatus m_status = SerializerStatus::success; // contains last error status

    private:
      tl::unexpected<SerializerStatus> error(SerializerStatus in_error)
      {
        m_status = in_error;
        return tl::make_unexpected(in_error);
      }

    public:
      Serializer() = delete;
      Serializer(uint8_t* out_begin) : m_begin(out_begin)
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
        if (m_cursor + sizeof(Type) > tc_bufferSize) { return error(SerializerStatus::writeOutOfRange); }
        
        SerializerReference<Type> element(reinterpret_cast<Type*>(m_begin + m_cursor));
        m_cursor = m_cursor + sizeof(Type);
        return element;
      }

      template<typename Type>
      tl::expected<size_t, SerializerStatus> write(Type in_value)
      {
        static_assert(std::is_arithmetic<Type>::value, "Type must be arithmetic!");
        if (m_cursor + sizeof(Type) > tc_bufferSize) { return error(SerializerStatus::writeOutOfRange); }

        *reinterpret_cast<Type*>(m_begin + m_cursor) = in_value;
        m_cursor = m_cursor + sizeof(Type);
        return m_cursor;
      }

      template<typename Type>
      tl::expected<size_t, SerializerStatus> writeEnum(Type in_value)
      {
        using UnderlyingType = typename std::underlying_type<Type>::type;
        static_assert(std::is_enum<Type>::value && std::is_arithmetic<UnderlyingType>::value, "Type must be an enum and underlying type must be arithmetic!");
        if (m_cursor + sizeof(UnderlyingType) > tc_bufferSize) { return error(SerializerStatus::writeOutOfRange); }

        *reinterpret_cast<UnderlyingType*>(m_begin + m_cursor) = static_cast<UnderlyingType>(in_value);
        m_cursor = m_cursor + sizeof(UnderlyingType);
        return m_cursor;
      }
      
      template<typename SizeType>
      tl::expected<size_t, SerializerStatus> writeStr(const char* in_string, SizeType in_size)
      {
        static_assert(isSizeType<SizeType>(), "SizeType must be an unsigned int!");
        if (m_cursor + sizeof(SizeType) + in_size > tc_bufferSize) { return error(SerializerStatus::writeStringOutOfRange); }
        
        if (not write<SizeType>(in_size).has_value()) { return error(SerializerStatus::writeStringSizeOutOfRange); }
        std::memcpy(m_begin + m_cursor, in_string, in_size);
        m_cursor = m_cursor + in_size;
        return m_cursor;
      }
      
      template<typename SizeType>
      tl::expected<size_t, SerializerStatus> writeStr(const String& in_string)
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
      DeserializerStatus m_status = DeserializerStatus::success; // contains last error status

    private:
      tl::unexpected<DeserializerStatus> error(DeserializerStatus in_error)
      {
        m_status = in_error;
        return tl::make_unexpected(in_error);
      }

    public:
      Deserializer() = delete;
      Deserializer(const uint8_t* in_begin) : m_begin(in_begin)
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
      tl::expected<size_t, DeserializerStatus> skip()
      {
        static_assert(std::is_arithmetic<Type>::value, "Type must be arithmetic!");
        if (m_cursor + sizeof(Type) > tc_bufferSize) { return error(DeserializerStatus::readOutOfRange); }

        m_cursor = m_cursor + sizeof(Type);
        return m_cursor;
      }

      template<typename Type>
      tl::expected<Type, DeserializerStatus> read()
      {
        static_assert(std::is_arithmetic<Type>::value, "Type must be arithmetic!");
        if (m_cursor + sizeof(Type) > tc_bufferSize) { return error(DeserializerStatus::readOutOfRange); }
        
        Type value = *reinterpret_cast<const Type*>(m_begin + m_cursor);
        m_cursor = m_cursor + sizeof(Type);
        return value;
      }

      template<typename Type, typename UnderlyingType>
      tl::expected<Type, DeserializerStatus> readEnum(const std::function<bool(UnderlyingType)>& fun_isEnumValue)
      {
        static_assert(std::is_same<UnderlyingType, typename std::underlying_type_t<Type>>::value, "UnderlyingType is not underlying type of Type!");
        static_assert(std::is_enum<Type>::value && std::is_arithmetic<UnderlyingType>::value, "Type must be an enum and underlying type must be arithmetic!");
        if (fun_isEnumValue == nullptr) { return error(DeserializerStatus::readIsEnumFunIsNullptr); }
        if (m_cursor + sizeof(UnderlyingType) > tc_bufferSize) { error(DeserializerStatus::readOutOfRange); }
        
        UnderlyingType value = *reinterpret_cast<const UnderlyingType*>(m_begin + m_cursor);
        if (not fun_isEnumValue(value)) { return error(DeserializerStatus::readIsEnumFunFalse); }
        m_cursor = m_cursor + sizeof(UnderlyingType);
        return static_cast<Type>(value);
      }
      
      template<typename SizeType>
      tl::expected<SizeType, DeserializerStatus> readStr(SizeType in_maxStringSize, char* out_string)
      {
        static_assert(isSizeType<SizeType>(), "Type must be an unsigned int!");
        if (out_string == nullptr) { return error(DeserializerStatus::readStringOutIsNullptr); }
        if (m_cursor + sizeof(SizeType) + in_maxStringSize > tc_bufferSize) { return error(DeserializerStatus::readStringOutOfRange); }
        
        auto sizeElement = read<SizeType>();
        if (not sizeElement.has_value()) { return error(DeserializerStatus::readStringSizeOutOfRange); }
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
        if (m_cursor + sizeof(SizeType) + in_maxStringSize > tc_bufferSize) { return error(DeserializerStatus::readStringOutOfRange); }
        
        auto sizeElement = read<SizeType>();
        if (not sizeElement.has_value()) { return error(DeserializerStatus::readStringSizeOutOfRange); }
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
