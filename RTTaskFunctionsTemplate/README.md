# RapidCode Real-Time Tasks Template

This is a **CMake template** for building a task functions library for use
with Real-Time Tasks (RTTasks).

This guide will help you integrate RTTasks into your application. If you're new
to RTTasks or want to better understand how they work, refer to the following resources:

- Read the **Real-Time Tasks** concept page in our [docs] for an overview
- Explore the RTTasks examples in the C++ folder, beginning with `HelloRTTasks`
- Review the `RTTaskFunctions` folder included with the C++ examples

---

## 🔹 Table of Contents

- [Quickstart](#-quickstart)
- [Set up your project](#-set-up-your-project)
- [Create an RTTask functions library](#-create-an-rttask-functions-library)
- [Launch your tasks](#-launch-your-tasks)

---

## 🔹 Quickstart

To get up and running quickly, follow these steps:

- Copy the `RTTaskFunctionsTemplate` (found in the examples folder) into your project
- Add any required global variables to `src/rttaskglobals.h`
- Define your task functions in `src/rttaskfunctions.cpp`, using `Increment` as
  a template
- Build the library using CMake
- In your application, create a `RTTaskManager` and use `TaskSubmit` to launch
  your tasks

For a more detailed walkthrough, continue below.

## 🔹 Set up your project

The `examples` folder in your RMP installation includes a CMake project called
`RTTaskFunctionsTemplate`. This serves as a starting point for building a shared
library containing RTTask functions. Copy this folder into your project directory
and rename it appropriately.

Here is an overview of the key files:

### Directory Structure

```text
RTTaskFunctionsTemplate/
├── src/
│   ├── rttaskglobals.h      # Defines Global variables
│   └── rttaskfunctions.cpp  # Task function implementations
│
├── CMakeLists.txt           # CMake setup for building the shared library
└── cmake/                   # CMake helper modules
    ├── RapidSoftware.cmake  # Paths for RapidCode/RMP firmware
    └── INtime/              # INtime helper scripts
```

### How it works

The template project takes all the source files (.h, .cpp) in the `src/` directory
and compiles them into a shared library for platform (Windows/INtime or Linux).
It automatically includes the RMP headers, links the RMP libraries, and applies
necessary configuration for use with RTTasks.

By default, the resulting library is named `RTTaskFunctions` and is output to
the default RMP install directory (e.g., `/rsi` or `C:/RSI/X.X.X`). If these
paths or names are modified, you must specify them explicitly when submitting a
task. Otherwise, they will be discovered automatically.

If you want to change any of the default behavior or are interested in learning
more about how it works, then look at the `CMakeLists.txt` located in the root
of the project folder.

## 🔹 Create an RTTask functions library

In this guide, you’ll build a simple application that moves an axis based on the
value of an analog input. It will use one global variable and two RTTasks. The
first task, `CalculateTarget`, reads the analog input and calculates a target
position, storing it in the global variable `targetPosition`. The second task,
`FollowTarget`, moves the axis to the specified target.

### Create a global variable

Open `src/rttaskglobals.h`. This file is where the global variables are defined
and registered. The global data looks similar to the following:

```cpp
struct GlobalData
{
  GlobalData() { std::memset(this, 0, sizeof(*this)); }
  GlobalData(GlobalData&& other) { std::memcpy(this, &other, sizeof(*this)); }

  RSI_GLOBAL(int64_t, counter)
  //... other globals
  RSI_GLOBAL(double, average);
};

inline constexpr GlobalMetadataMap<RSI::RapidCode::RealTimeTasks::GlobalMaxSize> GlobalMetadata(
{
  REGISTER_GLOBAL(counter),
  //... other globals
  REGISTER_GLOBAL(average),
});
```

To add a new global of type `double` called `targetPosition`:

- Add `RSI_GLOBAL(double, targetPosition)` to the `GlobalData` struct
- Add `REGISTER_GLOBAL(targetPosition)` to the `GlobalMetaDataMap`

The result should be:

```cpp
struct GlobalData
{
  GlobalData() { std::memset(this, 0, sizeof(*this)); }
  GlobalData(GlobalData&& other) { std::memcpy(this, &other, sizeof(*this)); }

  RSI_GLOBAL(int64_t, counter);
  //... other globals
  RSI_GLOBAL(double, average);
  RSI_GLOBAL(double, targetPosition);
};

inline constexpr GlobalMetadataMap<RSI::RapidCode::RealTimeTasks::GlobalMaxSize> GlobalMetadata(
{
  REGISTER_GLOBAL(counter),
  //... other globals
  REGISTER_GLOBAL(average),
  REGISTER_GLOBAL(targetPosition),
});
```

### Create a task function

Open `src/rttaskfunctions.cpp`. This is the file where the task functions are
defined. A task function must follow this template:

```cpp
LIBRARY_EXPORT void FunctionName(GlobalData* data)
{
  ...
}
```

We will add a new task function called `CalculateTarget` that will read the
analog input, scale it to a value between 0 and 1, and store it in the global
created in the previous step.

```cpp
  LIBRARY_EXPORT void CalculateTarget(GlobalData* data)
  {
    constexpr int NODE_INDEX = 0;         // The network node with the analog input
    constexpr int ANALOG_INDEX = 0;       // The index of the analog input to use
    constexpr int ANALOG_MAX = 65536;     // Max value of the analog input
    constexpr int ANALOG_ORIGIN = 42800;  // The value to treat as the "origin" of the analog input

    if (networkNodes[NODE_INDEX] != nullptr)
    {
      // Read the raw analog input value
      int32_t analogInVal = networkNodes[NODE_INDEX]->AnalogInGet(ANALOG_INDEX);

      // Shift the value by the origin
      int32_t shiftedVal = analogInVal - ANALOG_ORIGIN;

      // Make sure the value is between 0 and ANALOG_MAX using modulo
      int32_t modVal = (shiftedVal + ANALOG_MAX) % ANALOG_MAX;

      // Scale the value to be between 0 and 1 and store it in targetPosition
      data->targetPosition = double(modVal) / ANALOG_MAX;
    }
  }
```

Then we will add another task function called `FollowTarget` that will move the
axis towards the target position.

```cpp
  LIBRARY_EXPORT void FollowTarget(GlobalData* data)
  {
    constexpr int AXIS_INDEX = 1;      // The index of the axis to move
    constexpr double TOLERANCE = 0.02; // The tolerance for the position difference

    if (axes[AXIS_INDEX] != nullptr)
    {
      // Check if the axis is within the tolerance of the target position
      if (abs(axes[AXIS_INDEX]->ActualPositionGet() - data->targetPosition) > TOLERANCE)
      {
        // Move the axis to the target position
        axes[AXIS_INDEX]->MoveSCurve(data->targetPosition);
      }
    }
  }
```

Once the task functions have been added, build the project to produce a new library.

## 🔹 Launch your tasks

In your application, after configuring your RapidCode objects, create an
`RTTaskManager` instance and submit your tasks.

```cpp
RTTaskManagerCreationParameters parameters;

// The path to the RMP install folder
std::snprintf(parameters.RTTaskDirectory,
              RTTaskManagerCreationParameters::DirectoryLengthMaximum,
              RMP_DEFAULT_PATH);

// On Windows/INtime, use PlatformType::INtime for real-time or PlatformType::Windows for debugging
// On Linux, Platform does not need to be set
parameters.Platform = PlatformType::INtime

// For INtime managers, set the node you want the manager to run on
std::snprintf(parameters.NodeName,
              RTTaskManagerCreationParameters::NameLengthMaximum,
              "NodeA");

// For Linux, set the CPU core you want the manager to run on
parameters.CpuCore = 3;

// Create the RTTaskManager
std::shared_ptr<RTTaskManager> manager(RTTaskManager::Create(parameters));
```

Next, run the `Initialize` task one time, to get access to RapidCode objects and
initialize your global variables.

```cpp
// If you use the default library name and directory, you can omit the last two arguments
RTTaskCreationParameters initParams("Initialize");
params.Repeats = RTTaskCreationParameters::RepeatNone;  // Don't repeat this task
std::shared_ptr<RTTask> initTask(manager->TaskSubmit(initParams));
constexpr int timeoutMs = 5000;
initTask->ExecutionCountAbsoluteWait(1, timeoutMs); // Wait for the task to execute once.
```

Then submit the tasks created in the previous section.

```cpp
RTTaskCreationParameters calcParams("CalculateTarget");
calcParams.Repeats = RTTaskCreationParameters::RepeatForever;  // Repeat this task infinitely
calcParams.Period = 10;  // Run the task every 10 samples
std::shared_ptr<RTTask> calcTask(manager->TaskSubmit(calcParams));

calcTask->ExecutionCountAbsoluteWait(1);

RTTaskCreationParameters followParams("FollowTarget");
followParams.Repeats = RTTaskCreationParameters::RepeatForever;
followParams.Period = 10;
std::shared_ptr<RTTask> followTask(manager->TaskSubmit(followParams));
```

While your tasks are running, you can use `RTTask::StatusGet` and
`RTTaskManager::GlobalValueGet` to monitor your tasks.

```cpp
FirmwareValue value = manager->GlobalValueGet("targetPosition");
std::cout << "Target position: " << value.Double << std::endl;
```

At the end of your program, stop the tasks and manager.

```cpp
followTask->Stop();
calcTask->Stop();
manager->Shutdown();
```

<!-- Link Definitions -->

[docs]: https://support.roboticsys.com/rmp/
