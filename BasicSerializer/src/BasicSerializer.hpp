#pragma once

#include <type_traits>
#include <limits>
#include <array>
#include <memory>
#include <cstring>

// **** **** **** ****
// NOTE: Serializer, Deserializer, SerializerReference and DeserializerReference are only
//       valid until your underlying buffer (of uint8_t*) expires.
//       When the underlying buffer is gone, use of these types will cause undefined behaviour!
// **** **** **** ****

namespace halvoe
{
  template<typename SizeType>
  static constexpr bool isSizeType()
  {
    return sizeof(SizeType) <= sizeof(size_t)       &&
           (std::is_same<SizeType, size_t>::value   ||
            std::is_same<SizeType, uint8_t>::value  ||
            std::is_same<SizeType, uint16_t>::value ||
            std::is_same<SizeType, uint32_t>::value ||
            std::is_same<SizeType, uint64_t>::value);
  }
  
  template<typename Type>
  class SerializerReference
  {
    static_assert(std::is_arithmetic<Type>::value, "Type must be arithmetic!");
    
    private:
      Type* m_element;
    
    public:
      SerializerReference() : m_element(nullptr)
      {}
      
      SerializerReference(Type* in_element) : m_element(in_element)
      {}
      
      bool isNull() const
      {
        return m_element == nullptr;
      }
      
      Type read() const
      {
        if (m_element == nullptr) { return std::numeric_limits<Type>::max(); }
        
        return *m_element;
      }
      
      bool write(Type in_value)
      {
        if (m_element == nullptr) { return false; }
        
        *m_element = in_value;
        return true;
      }
  };

  template<size_t tc_bufferSize>
  class Serializer
  {
    private:
      uint8_t* m_begin;
      size_t m_cursor = 0;

    public:
      Serializer() = delete;
      Serializer(uint8_t* out_begin) : m_begin(out_begin)
      {}
      Serializer(std::array<uint8_t, tc_bufferSize>& out_array) : m_begin(out_array.data())
      {}

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
      SerializerReference<Type> skip()
      {
        static_assert(std::is_arithmetic<Type>::value, "Type must be arithmetic!");
        if (m_cursor + sizeof(Type) > tc_bufferSize) { return SerializerReference<Type>(); }
        
        SerializerReference<Type> element(reinterpret_cast<Type*>(m_begin + m_cursor));
        m_cursor = m_cursor + sizeof(Type);
        return element;
      }

      template<typename Type>
      bool write(Type in_value)
      {
        static_assert(std::is_arithmetic<Type>::value, "Type must be arithmetic!");
        if (m_cursor + sizeof(Type) > tc_bufferSize) { return false; }

        *reinterpret_cast<Type*>(m_begin + m_cursor) = in_value;
        m_cursor = m_cursor + sizeof(Type);
        return true;
      }

      template<typename Type>
      bool writeEnum(Type in_value)
      {
        using UnderlyingType = std::underlying_type<Type>::type;
        static_assert(std::is_enum<Type>::value && std::is_arithmetic<UnderlyingType>::value, "Type must be an enum and underlying type must be arithmetic!");
        if (m_cursor + sizeof(UnderlyingType) > tc_bufferSize) { return false; }

        *reinterpret_cast<UnderlyingType*>(m_begin + m_cursor) = static_cast<UnderlyingType>(in_value);
        m_cursor = m_cursor + sizeof(UnderlyingType);
        return true;
      }
      
      template<typename SizeType>
      bool write(const char* in_string, SizeType in_size)
      {
        static_assert(isSizeType<SizeType>(), "SizeType must be an unsigned int!");
        if (m_cursor + sizeof(SizeType) + in_size > tc_bufferSize) { return false; }
        
        write<SizeType>(in_size);
        std::memcpy(m_begin, in_string, in_size);
        m_cursor = m_cursor + in_size;
        return true;
      }
  };
  
  template<typename Type>
  class DeserializerReference
  {
    static_assert(std::is_arithmetic<Type>::value, "Type must be arithmetic!");
    
    private:
      const Type* m_element;
    
    public:
      DeserializerReference() : m_element(nullptr)
      {}
      
      DeserializerReference(const Type* in_element) : m_element(in_element)
      {}
      
      bool isNull() const
      {
        return m_element == nullptr;
      }
      
      Type read() const
      {
        if (m_element == nullptr) { return std::numeric_limits<Type>::max(); }
        
        return *m_element;
      }
  };
  
  template<size_t tc_bufferSize>
  class Deserializer
  {
    private:
      const uint8_t* m_begin;
      size_t m_cursor = 0;
      
    private:
      std::unique_ptr<const char[]> getNullString()
      {
        auto nullString = std::make_unique<char[]>(1);
        nullString[0] = '\0';
        return nullString;
      }

    public:
      Deserializer() = delete;
      Deserializer(const uint8_t* in_begin) : m_begin(in_begin)
      {}
      Deserializer(const std::array<uint8_t, tc_bufferSize>& in_array) : m_begin(in_array.data())
      {}

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
      bool skip()
      {
        static_assert(std::is_arithmetic<Type>::value, "Type must be arithmetic!");
        if (m_cursor + sizeof(Type) > tc_bufferSize) { return false; }

        m_cursor = m_cursor + sizeof(Type);
        return true;
      }

      template<typename Type>
      Type read()
      {
        static_assert(std::is_arithmetic<Type>::value, "Type must be arithmetic!");
        if (m_cursor + sizeof(Type) > tc_bufferSize) { return std::numeric_limits<Type>::max(); }
        
        Type value = *reinterpret_cast<const Type*>(m_begin + m_cursor);
        m_cursor = m_cursor + sizeof(Type);
        return value;
      }

      template<typename Type>
      Type readEnum()
      {
        using UnderlyingType = std::underlying_type<Type>::type;
        static_assert(std::is_enum<Type>::value && std::is_arithmetic<UnderlyingType>::value, "Type must be an enum and underlying type must be arithmetic!");
        if (m_cursor + sizeof(UnderlyingType) > tc_bufferSize) { return Type{ std::numeric_limits<UnderlyingType>::max() }; }
        
        UnderlyingType value = *reinterpret_cast<const UnderlyingType*>(m_begin + m_cursor);
        m_cursor = m_cursor + sizeof(UnderlyingType);
        return Type{ value };
      }
      
      template<typename Type>
      const DeserializerReference<Type> view()
      {
        static_assert(std::is_arithmetic<Type>::value, "Type must be arithmetic!");
        if (m_cursor + sizeof(Type) > tc_bufferSize) { return DeserializerReference<Type>(); }
        
        DeserializerReference<Type> element(reinterpret_cast<const Type*>(m_begin + m_cursor));
        m_cursor = m_cursor + sizeof(Type);
        return element;
      }
      
      template<typename SizeType>
      std::unique_ptr<const char[]> read(SizeType in_maxStringSize, SizeType& out_stringSize)
      {
        static_assert(isSizeType<SizeType>(), "Type must be an unsigned int!");
        if (m_cursor + sizeof(SizeType) + in_maxStringSize > tc_bufferSize) { return getNullString(); }
        
        auto sizeElement = view<SizeType>();
        if (sizeElement.isNull()) { return getNullString(); }
        const SizeType size = sizeElement.read() < in_maxStringSize ? sizeElement.read() : in_maxStringSize - 1; 
        
        auto string = std::make_unique<char[]>(size + 1);
        std::memcpy(string.get(), m_begin + m_cursor, size);
        string[size] = '\0';
        out_stringSize = size;
        m_cursor = m_cursor + size;
        return string;
      }
      
      template<typename SizeType>
      std::unique_ptr<const char[]> read(SizeType in_maxStringSize)
      {
        static_assert(isSizeType<SizeType>(), "Type must be an unsigned int!");
        if (m_cursor + sizeof(SizeType) + in_maxStringSize > tc_bufferSize) { return getNullString(); }
        
        auto sizeElement = view<SizeType>();
        if (sizeElement.isNull()) { return getNullString(); }
        const SizeType size = sizeElement.read() < in_maxStringSize ? sizeElement.read() : in_maxStringSize - 1; 
        
        auto string = std::make_unique<char[]>(size + 1);
        std::memcpy(string.get(), m_begin + m_cursor, size);
        string[size] = '\0';
        m_cursor = m_cursor + size;
        return string;
      }
  };
}
