#pragma once

#ifndef RT_TASKS_GLOBALS_H
#define RT_TASKS_GLOBALS_H

#include <atomic>                   // For std::atomic
#include <cstddef>                  // For std::size_t, offsetof
#include <cstring>                  // For std::memset, std::strcmp
#include <type_traits>              // For std::is_same

#include "rttask.h"


#if defined(WIN32)
  #define LIBRARY_EXPORT __declspec(dllexport)
  #define LIBRARY_IMPORT __declspec(dllimport)
#elif defined(__linux__)
  #define LIBRARY_EXPORT __attribute__((visibility("default")))
  #define LIBRARY_IMPORT __attribute__((visibility("default")))
#else
  #define LIBRARY_EXPORT
  #define LIBRARY_IMPORT
#endif // defined(WIN32) || defined(__linux__)

#define NAME(name) name
#define CONCAT(left, right) left ## right
#define RSI_TASK(name) \
  void CONCAT(name, Core)(RSI::RapidCode::RealTimeTasks::GlobalData*); \
  extern "C" LIBRARY_EXPORT int32_t NAME(name)(RSI::RapidCode::RealTimeTasks::GlobalData* data, char* buffer, const uint32_t size) { return CallFunction(CONCAT(name, Core), data, buffer, size); } \
  void CONCAT(name, Core)(RSI::RapidCode::RealTimeTasks::GlobalData* data)

template<typename FunctionType>
int32_t CallFunction(FunctionType&& func, RSI::RapidCode::RealTimeTasks::GlobalData* data, char* buffer, const uint32_t size)
{
  int32_t result = 0;
  try
  {
    func(data);
  }
  catch(const std::exception& error)
  {
    result = -1;
    std::snprintf(buffer, size, "%s", error.what());
  }
  catch(...)
  {
    result = -1;
    std::snprintf(buffer, size, "Unknown error occurred in task.");
  }
  return result;
}


namespace RSI
{
namespace RapidCode
{
namespace RealTimeTasks
{

struct GlobalData
{
  GlobalData() { std::memset(this, 0, sizeof(*this)); }
  GlobalData(GlobalData&& other) { std::memcpy(this, &other, sizeof(*this)); }

  RSI_GLOBAL(bool, cameraInitialized);
  RSI_GLOBAL(bool, cameraPrimed);
  RSI_GLOBAL(double, targetX);
  RSI_GLOBAL(double, targetY);
};

inline constexpr GlobalMetadataMap<RSI::RapidCode::RealTimeTasks::GlobalMaxSize> GlobalMetadata(
{
  REGISTER_GLOBAL(cameraInitialized),
  REGISTER_GLOBAL(cameraPrimed),
  REGISTER_GLOBAL(targetX),
  REGISTER_GLOBAL(targetY),
});

extern "C"
{
  LIBRARY_EXPORT int32_t GlobalMemberOffsetGet(const char* const name)
  {
    return GlobalMetadata[name].offset;
  }
  static_assert(std::is_same<decltype(&GlobalMemberOffsetGet), GlobalMemberOffsetGetter>::value, "GlobalMemberOffsetGet function signature does not match GlobalMemberOffsetGetter type.");
  
  LIBRARY_EXPORT int32_t GlobalNamesFill(const char* names[], int32_t capacity)
  {
    int32_t index = 0;
    for (; index < GlobalMetadata.Size() && index < capacity; ++index)
    {
      names[index] = GlobalMetadata[index].key;
    }
    return index;
  }
  static_assert(std::is_same<decltype(&GlobalNamesFill), GlobalNamesGetter>::value, "GlobalNamesGet function signature does not match GlobalNamesGetter type.");

  LIBRARY_EXPORT std::int32_t GlobalMemberTypeGet(const char* const name)
  {
    return static_cast<std::int32_t>(GlobalMetadata[name].type);
  }
  static_assert(std::is_same<decltype(&GlobalMemberTypeGet), GlobalMemberTypeGetter>::value, "GlobalMemberTypeGet function signature does not match GlobalMemberTypeGetter type.");
}

static_assert(sizeof(GlobalData) <= RSI::RapidCode::RealTimeTasks::GlobalMaxSize, "GlobalData struct is too large.");

} // end namespace RtTask
} // end namespace RapidCode
} // end namespace RSI

extern "C"
{
  LIBRARY_IMPORT RSI::RapidCode::MotionController* MotionControllerGet(char* errorBuffer, const uint32_t errorBufferSize);
  LIBRARY_IMPORT RSI::RapidCode::Axis* AxisGet(const int32_t axisIndex, char* errorBuffer, const uint32_t errorBufferSize);
  LIBRARY_IMPORT RSI::RapidCode::RapidCodeNetworkNode* NetworkNodeGet(const int32_t index, char* errorBuffer, const uint32_t errorBufferSize);
  LIBRARY_IMPORT RSI::RapidCode::MultiAxis* MultiAxisGet(const int32_t index, char* errorBuffer, const uint32_t errorBufferSize);
}

template<typename FunctionType, typename ... Args>
auto RTObjectGet(FunctionType&& func, Args&& ... args)
{
  char errorBuffer[256] = {};
  auto object = std::forward<FunctionType>(func)(std::forward<Args>(args)..., errorBuffer, sizeof(errorBuffer));
  if (object == nullptr) { throw std::runtime_error(errorBuffer); };
  return object;
}
inline auto RTMotionControllerGet() { return RTObjectGet(MotionControllerGet); }
inline auto RTAxisGet(const int32_t index) { return RTObjectGet(AxisGet, index); }
inline auto RTMultiAxisGet(const int32_t index) { return RTObjectGet(MultiAxisGet, index); }
inline auto RTNetworkNodeGet(const int32_t index) { return RTObjectGet(NetworkNodeGet, index); }

#endif // !defined(RT_TASKS_GLOBALS_H)
