#pragma once

#include <system_error>
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


enum class SerializerStatus : uint8_t
{
  success = 0,
  writeOutOfRange
};

enum class DeserializerStatus : uint8_t
{
  success = 0,
  readOutOfRange,
  viewOutOfRange,
  readStringOutOfRange,
  viewStringSizeOutOfRange,
  readStringOutIsNullptr,
  readIsEnumFunIsNullptr
};

namespace std
{
  // Tell the C++ STL metaprogramming that enum SerializerStatus
  // is registered with the standard error code system
  template<> struct is_error_code_enum<SerializerStatus> : true_type
  {};

  // Tell the C++ STL metaprogramming that enum DeserializerStatus
  // is registered with the standard error code system
  template<> struct is_error_code_enum<DeserializerStatus> : true_type
  {};
}

namespace detail
{
  // Define a custom error code category derived from std::error_category
  class SerializerStatus_category : public std::error_category
  {
    private:
      const char* messageImpl(int in_code) const
      {
        switch (static_cast<SerializerStatus>(in_code))
        {
          case SerializerStatus::success:         return "operation successful";
          case SerializerStatus::writeOutOfRange: return "write operation out of range";
          default:                                return "unknown";
        }
      }

    public:
      // Return a short descriptive name for the category
      const char* name() const noexcept override final
      {
        return "SerializerStatus";
      }

      // Return what each enum means in text as std::string
      std::string message(int in_code) const override final
      {
        return messageImpl(in_code);
      }

      // Return what each enum means in text as Arduino::String
      String messageString(int in_code) const
      {
        return messageImpl(in_code);
      }
  };
}

// Define the linkage for this function to be used by external code.
// This would be the usual __declspec(dllexport) or __declspec(dllimport)
// if we were in a Windows DLL etc. But for this example use a global
// instance but with inline linkage so multiple definitions do not collide.

// Declare a global function returning a static instance of the custom category
extern inline const detail::SerializerStatus_category& SerializerStatus_category()
{
  static detail::SerializerStatus_category category;
  return category;
}


// Overload the global make_error_code() free function with our
// custom enum. It will be found via ADL by the compiler if needed.
inline std::error_code make_error_code(SerializerStatus in_errorCode)
{
  return { static_cast<int>(in_errorCode), SerializerStatus_category() };
}

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
      tl::expected<SerializerReference<Type>, std::error_code> skip()
      {
        static_assert(std::is_arithmetic<Type>::value, "Type must be arithmetic!");
        if (m_cursor + sizeof(Type) > tc_bufferSize) { return tl::make_unexpected(SerializerStatus::writeOutOfRange); }
        
        SerializerReference<Type> element(reinterpret_cast<Type*>(m_begin + m_cursor));
        m_cursor = m_cursor + sizeof(Type);
        return element;
      }

      template<typename Type>
      std::error_code write(Type in_value)
      {
        static_assert(std::is_arithmetic<Type>::value, "Type must be arithmetic!");
        if (m_cursor + sizeof(Type) > tc_bufferSize) { return SerializerStatus::writeOutOfRange; }

        *reinterpret_cast<Type*>(m_begin + m_cursor) = in_value;
        m_cursor = m_cursor + sizeof(Type);
        return SerializerStatus::success;
      }

      template<typename Type>
      std::error_code writeEnum(Type in_value)
      {
        using UnderlyingType = typename std::underlying_type<Type>::type;
        static_assert(std::is_enum<Type>::value && std::is_arithmetic<UnderlyingType>::value, "Type must be an enum and underlying type must be arithmetic!");
        if (m_cursor + sizeof(UnderlyingType) > tc_bufferSize) { return SerializerStatus::writeOutOfRange; }

        *reinterpret_cast<UnderlyingType*>(m_begin + m_cursor) = static_cast<UnderlyingType>(in_value);
        m_cursor = m_cursor + sizeof(UnderlyingType);
        return SerializerStatus::success;
      }
      
      template<typename SizeType>
      std::error_code write(const char* in_string, SizeType in_size)
      {
        static_assert(isSizeType<SizeType>(), "SizeType must be an unsigned int!");
        if (m_cursor + sizeof(SizeType) + in_size > tc_bufferSize) { return SerializerStatus::writeOutOfRange; }
        
        write<SizeType>(in_size);
        std::memcpy(m_begin + m_cursor, in_string, in_size);
        m_cursor = m_cursor + in_size;
        return SerializerStatus::success;
      }
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
      bool skip()
      {
        static_assert(std::is_arithmetic<Type>::value, "Type must be arithmetic!");
        if (m_cursor + sizeof(Type) > tc_bufferSize) {return false; }

        m_cursor = m_cursor + sizeof(Type);
        return true;
      }

      template<typename Type>
      Type read()
      {
        static_assert(std::is_arithmetic<Type>::value, "Type must be arithmetic!");
        if (m_cursor + sizeof(Type) > tc_bufferSize) { return 0; }
        
        Type value = *reinterpret_cast<const Type*>(m_begin + m_cursor);
        m_cursor = m_cursor + sizeof(Type);
        return value;
      }

      template<typename Type, typename UnderlyingType = typename std::underlying_type<Type>::type>
      bool readEnum(Type& out_value, const std::function<bool(UnderlyingType)>& fun_isEnumValue)
      {
        static_assert(std::is_same<UnderlyingType, typename std::underlying_type_t<Type>>::value, "UnderlyingType is not underlying type of Type!");
        static_assert(std::is_enum<Type>::value && std::is_arithmetic<UnderlyingType>::value, "Type must be an enum and underlying type must be arithmetic!");
        if (fun_isEnumValue == nullptr) { return false; }
        if (m_cursor + sizeof(UnderlyingType) > tc_bufferSize) { return false; }
        
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
        if (m_cursor + sizeof(Type) > tc_bufferSize) { return DeserializerReference<Type>(); }
        
        DeserializerReference<Type> element(reinterpret_cast<const Type*>(m_begin + m_cursor));
        m_cursor = m_cursor + sizeof(Type);
        return element;
      }
      
      template<typename SizeType>
      SizeType read(SizeType in_maxStringSize, char* out_string)
      {
        static_assert(isSizeType<SizeType>(), "Type must be an unsigned int!");
        if (out_string == nullptr) { return 0; }
        if (m_cursor + sizeof(SizeType) + in_maxStringSize > tc_bufferSize) { return 0; }
        
        auto sizeElement = view<SizeType>();
        if (sizeElement.isNull()) { return 0; }
        const SizeType size = sizeElement.read() < in_maxStringSize ? sizeElement.read() : in_maxStringSize - 1; 
        
        std::memcpy(out_string, m_begin + m_cursor, size);
        out_string[size] = '\0';
        m_cursor = m_cursor + size;
        return size;
      }
  };
}
