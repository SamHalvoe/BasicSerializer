#pragma once

#include <type_traits>
#include <functional>
#include <cstring>

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
    return sizeof(SizeType) <= sizeof(size_t)       &&
           (std::is_same<SizeType, size_t>::value   ||
            std::is_same<SizeType, uint8_t>::value  ||
            std::is_same<SizeType, uint16_t>::value ||
            std::is_same<SizeType, uint32_t>::value ||
            std::is_same<SizeType, uint64_t>::value);
  }

  enum class SerializerStatus : uint8_t
  {
    ok = 0,
    writeOutOfRange
  };
  
  template<typename Type>
  class SerializerReference
  {
    static_assert(std::is_arithmetic<Type>::value, "Type must be arithmetic!");
    static_assert(not std::is_enum<Type>::value, "Type must not be an enum!");
    
    private:
      Type* m_element; // ToDo: Maybe change to "Type* const"?!?
    
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
        if (m_element == nullptr) { return 0; }
        
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
      size_t m_cursor = 0;
      uint8_t* m_begin; // ToDo: Maybe change to "uint8_t* const"?!?
      SerializerStatus m_status = SerializerStatus::ok;

    public:
      Serializer() = delete;
      Serializer(uint8_t* out_begin) : m_begin(out_begin)
      {}
      
      void reset()
      {
        m_cursor = 0;
        m_status = SerializerStatus::ok;
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
      SerializerReference<Type> skip()
      {
        static_assert(std::is_arithmetic<Type>::value, "Type must be arithmetic!");
        if (m_cursor + sizeof(Type) > tc_bufferSize) { m_status = SerializerStatus::writeOutOfRange; return SerializerReference<Type>(); }
        
        SerializerReference<Type> element(reinterpret_cast<Type*>(m_begin + m_cursor));
        m_cursor = m_cursor + sizeof(Type);
        return element;
      }

      template<typename Type>
      bool write(Type in_value)
      {
        static_assert(std::is_arithmetic<Type>::value, "Type must be arithmetic!");
        if (m_cursor + sizeof(Type) > tc_bufferSize) { m_status = SerializerStatus::writeOutOfRange; return false; }

        *reinterpret_cast<Type*>(m_begin + m_cursor) = in_value;
        m_cursor = m_cursor + sizeof(Type);
        return true;
      }

      template<typename Type>
      bool writeEnum(Type in_value)
      {
        using UnderlyingType = typename std::underlying_type<Type>::type;
        static_assert(std::is_enum<Type>::value && std::is_arithmetic<UnderlyingType>::value, "Type must be an enum and underlying type must be arithmetic!");
        if (m_cursor + sizeof(UnderlyingType) > tc_bufferSize) { m_status = SerializerStatus::writeOutOfRange; return false; }

        *reinterpret_cast<UnderlyingType*>(m_begin + m_cursor) = static_cast<UnderlyingType>(in_value);
        m_cursor = m_cursor + sizeof(UnderlyingType);
        return true;
      }
      
      template<typename SizeType>
      bool write(const char* in_string, SizeType in_size)
      {
        static_assert(isSizeType<SizeType>(), "SizeType must be an unsigned int!");
        if (m_cursor + sizeof(SizeType) + in_size > tc_bufferSize) { m_status = SerializerStatus::writeOutOfRange; return false; }
        
        write<SizeType>(in_size);
        std::memcpy(m_begin + m_cursor, in_string, in_size);
        m_cursor = m_cursor + in_size;
        return true;
      }
  };

  enum class DeserializerStatus : uint8_t
  {
    ok = 0,
    readOutOfRange,
    viewOutOfRange,
    readStringOutOfRange,
    viewStringSizeOutOfRange,
    readStringOutIsNullptr,
    readIsEnumFunIsNullptr
  };
  
  template<typename Type>
  class DeserializerReference
  {
    static_assert(std::is_arithmetic<Type>::value, "Type must be arithmetic!");
    static_assert(not std::is_enum<Type>::value, "Type must not be an enum!");
    
    private:
      const Type* m_element; // ToDo: Maybe change to "const Type* const"?!?
    
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
        if (m_element == nullptr) { return 0; }
        
        return *m_element;
      }
  };
  
  template<size_t tc_bufferSize>
  class Deserializer
  {
    private:
      size_t m_cursor = 0;
      const uint8_t* m_begin; // ToDo: Maybe change to "const uint8_t* const"?!?
      DeserializerStatus m_status = DeserializerStatus::ok;

    public:
      Deserializer() = delete;
      Deserializer(const uint8_t* in_begin) : m_begin(in_begin)
      {}
      
      void reset()
      {
        m_cursor = 0;
        m_status = DeserializerStatus::ok;
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
      bool skip()
      {
        static_assert(std::is_arithmetic<Type>::value, "Type must be arithmetic!");
        if (m_cursor + sizeof(Type) > tc_bufferSize) { m_status = DeserializerStatus::readOutOfRange; return false; }

        m_cursor = m_cursor + sizeof(Type);
        return true;
      }

      template<typename Type>
      Type read()
      {
        static_assert(std::is_arithmetic<Type>::value, "Type must be arithmetic!");
        if (m_cursor + sizeof(Type) > tc_bufferSize) { m_status = DeserializerStatus::readOutOfRange; return 0; }
        
        Type value = *reinterpret_cast<const Type*>(m_begin + m_cursor);
        m_cursor = m_cursor + sizeof(Type);
        return value;
      }

      template<typename Type, typename UnderlyingType = typename std::underlying_type<Type>::type>
      bool readEnum(Type& out_value, const std::function<bool(UnderlyingType)>& fun_isEnumValue)
      {
        static_assert(std::is_same<UnderlyingType, typename std::underlying_type_t<Type>>::value, "UnderlyingType is not underlying type of Type!");
        static_assert(std::is_enum<Type>::value && std::is_arithmetic<UnderlyingType>::value, "Type must be an enum and underlying type must be arithmetic!");
        if (fun_isEnumValue == nullptr) { m_status = DeserializerStatus::readIsEnumFunIsNullptr; return false; }
        if (m_cursor + sizeof(UnderlyingType) > tc_bufferSize) { m_status = DeserializerStatus::readOutOfRange; return false; }
        
        UnderlyingType value = *reinterpret_cast<const UnderlyingType*>(m_begin + m_cursor);
        if (not fun_isEnumValue(value)) { return false; }
        out_value = static_cast<Type>(value);
        m_cursor = m_cursor + sizeof(UnderlyingType);
        return true;
      }
      
      template<typename Type>
      const DeserializerReference<Type> view()
      {
        static_assert(std::is_arithmetic<Type>::value, "Type must be arithmetic!");
        if (m_cursor + sizeof(Type) > tc_bufferSize) { m_status = DeserializerStatus::viewOutOfRange; return DeserializerReference<Type>(); }
        
        DeserializerReference<Type> element(reinterpret_cast<const Type*>(m_begin + m_cursor));
        m_cursor = m_cursor + sizeof(Type);
        return element;
      }
      
      template<typename SizeType>
      SizeType read(SizeType in_maxStringSize, char* out_string)
      {
        static_assert(isSizeType<SizeType>(), "Type must be an unsigned int!");
        if (out_string == nullptr) { m_status = DeserializerStatus::readStringOutIsNullptr; return 0; }
        if (m_cursor + sizeof(SizeType) + in_maxStringSize > tc_bufferSize) { m_status = DeserializerStatus::readStringOutOfRange; return 0; }
        
        auto sizeElement = view<SizeType>();
        if (sizeElement.isNull()) { m_status = DeserializerStatus::viewStringSizeOutOfRange; return 0; }
        const SizeType size = sizeElement.read() < in_maxStringSize ? sizeElement.read() : in_maxStringSize - 1; 
        
        std::memcpy(out_string, m_begin + m_cursor, size);
        out_string[size] = '\0';
        m_cursor = m_cursor + size;
        return size;
      }
  };
}
