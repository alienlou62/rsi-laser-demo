#ifndef SAMPLE_APPS_HELPER
#define SAMPLE_APPS_HELPER

// These macros are defined as part of the build process in CMake. To avoid
// build errors, we default them to empty strings if they are not defined.
#ifndef RMP_DEFAULT_PATH
#define RMP_DEFAULT_PATH ""
#endif

#include "rsi.h" // Import our RapidCode Library.
#include <iostream>
#include <string>
#include <sstream>
#include <source_location>

using namespace RSI::RapidCode; // Import our RapidCode namespace

// Forward Declarations
namespace SampleAppsHelper
{
  static void CheckErrors(RapidCodeObject *rsiObject, const std::source_location &location = std::source_location::current());
}

/*!
  @brief This struct provides static methods and constants for user configuration in RMP applications. Everything in this struct is expected to be
  modifed by the user.
*/
/// @internal @[SampleAppsConfig-cpp]
struct SampleAppsConfig
{
  // Motion Controller Creation Parameters
  static constexpr char RMP_PATH[] = RMP_DEFAULT_PATH; ///< Path to the RMP.rta file (usually the RapidSetup folder).
  static constexpr char NIC_PRIMARY[] = "";            ///< Name of the NIC to use for the EtherCAT network (not needed for running on phantom axes)
  static constexpr char NODE_NAME[] = "";              ///< (Windows only) INtime node name. Use "" for default node.
  static constexpr int CPU_AFFINITY = 0;               ///< (Linux only) CPU core to use. This should be an isolated core.

  // If you want to use hardware, then follow the instructions in the README before setting this to true.
  static constexpr bool USE_HARDWARE = false; ///< Flag for whether to use hardware or phantom axes

  /*!
    @brief Configures the hardware axes according to user specified implementation.
    @param controller Pointer to the MotionController to configure the axes on.
    @exception std::runtime_error Thrown if the function is called without being implemented.
  */
  static void ConfigureHardwareAxes(MotionController *controller)
  {
    // If you have configured your axes using rsiconfig, RapidSetup, or RapidSetupX then you can comment out this error and return.
    // IMPLEMENTATION REQUIRED: Configure the axes for hardware
    std::ostringstream message;
    message << "You must implement the " << __func__ << " function (found in " << __FILE__ << " line " << __LINE__ << ") to use hardware.";
    throw std::runtime_error(message.str().c_str());

    // Simple Axis Configuration Example:
    // (Look at the Axis: Configuration section in the RapidCode API Reference for more advanced configurations)
    /*
    const int USER_UNITS = 1048576; // example for 20-bit encoder, 2^20 = 1048576
    Axis *axis = controller->AxisGet(0);
    SampleAppsHelper::CheckErrors(axis);
    axis->UserUnitsSet(USER_UNITS);
    axis->PositionSet(0);
    axis->ErrorLimitTriggerValueSet(0.1 * USER_UNITS);
    axis->ErrorLimitActionSet(RSIAction::RSIActionABORT);
    axis->Abort();
    axis->ClearFaults();
    */
  }
};
/// @internal @[SampleAppsConfig-cpp]

/*!
  @brief SampleAppsHelper namespace provides static methods for common tasks in RMP applications.
  @details This namespace encapsulates operations such as error checking, network management, and controller configuration to streamline application
  development.
*/
namespace SampleAppsHelper
{
  /*!
    @brief Returns a MotionController::CreationParameters object with user-defined parameters.
    @snippet SampleAppsHelper.h Get-Creation-Parameters-cpp
  */
  /// @internal @[Get-Creation-Parameters-cpp]
  #if _WIN32
  static MotionController::CreationParameters GetCreationParameters()
  {
    // Create a controller with using defined parameters
    MotionController::CreationParameters params;
    strncpy(params.RmpPath, SampleAppsConfig::RMP_PATH, params.PathLengthMaximum);
    strncpy(params.NicPrimary, SampleAppsConfig::NIC_PRIMARY, params.PathLengthMaximum);
    strncpy(params.NodeName, SampleAppsConfig::NODE_NAME, params.PathLengthMaximum);
    return params;
  }
  #elif __linux__
  static MotionController::CreationParameters GetCreationParameters()
  {
    MotionController::CreationParameters params;
    strncpy(params.RmpPath, SampleAppsConfig::RMP_PATH, params.PathLengthMaximum);
    strncpy(params.NicPrimary, SampleAppsConfig::NIC_PRIMARY, params.PathLengthMaximum);
    params.CpuAffinity = SampleAppsConfig::CPU_AFFINITY;
    return params;
  }
  #endif
  /// @internal @[Get-Creation-Parameters-cpp]

  /*!
    @brief Checks for errors in the given RapidCodeObject and throws an exception if any non-warning errors are found.
    @param rsiObject Pointer to the RapidCodeObject to check for errors.
    @param location Source location of the function call for error reporting.
    @exception std::runtime_error Thrown if any non-warning errors are encountered in the error log of the RapidCodeObject.
    @snippet SampleAppsHelper.h Check-Errors-cpp
  */
  /// @internal @[Check-Errors-cpp]
  static void CheckErrors(RapidCodeObject *rsiObject, const std::source_location &location)
  {
    bool hasErrors = false;
    std::string errorStrings("");
    while (rsiObject->ErrorLogCountGet() > 0)
    {
      const RsiError *err = rsiObject->ErrorLogGet();

      errorStrings += err->what();
      errorStrings += "\n";

      if (!err->isWarning)
      {
        hasErrors = true;
      }
    }

    if (hasErrors)
    {
      std::ostringstream message;
      message << "Error! In "
              << location.file_name() << '('
              << location.line() << ':'
              << location.column() << ") `"
              << location.function_name() << "`:\n"
              << errorStrings;

      throw std::runtime_error(message.str().c_str());
    }
  }
  /// @internal @[Check-Errors-cpp]

  /*!
    @brief Starts the network communication for the given MotionController.
    @param controller Pointer to the MotionController to start the network on.
    @exception std::runtime_error Thrown if the network does not reach the OPERATIONAL state.
    @snippet SampleAppsHelper.h Start-The-Network-cpp
  */
  /// @internal @[Start-The-Network-cpp]
  static void StartTheNetwork(MotionController *controller)
  {
    // Initialize the Network
    if (controller->NetworkStateGet() != RSINetworkState::RSINetworkStateOPERATIONAL) // Check if network is started already.
    {
      std::cout << "Starting Network.." << std::endl;
      controller->NetworkStart(); // If not. Initialize The Network. (This can also be done from RapidSetup Tool)
    }

    if (controller->NetworkStateGet() != RSINetworkState::RSINetworkStateOPERATIONAL) // Check if network is started again.
    {
      int messagesToRead = controller->NetworkLogMessageCountGet(); // Some kind of error starting the network, read the network log messages

      for (int i = 0; i < messagesToRead; i++)
      {
        std::cout << controller->NetworkLogMessageGet(i) << std::endl; // Print all the messages to help figure out the problem
      }
      throw std::runtime_error("Expected OPERATIONAL state but the network did not get there.");
    }
    else // Else, of network is operational.
    {
      std::cout << "Network Started" << std::endl << std::endl;
    }
  }
  /// @internal @[Start-The-Network-cpp]

  /*!
    @brief Shuts down the network communication for the given MotionController.
    @param controller Pointer to the MotionController to shut down the network on.
    @exception std::runtime_error Thrown if the network does not reach the SHUTDOWN or UNINITIALIZED state.
    @snippet SampleAppsHelper.h Shutdown-The-Network-cpp
  */
  /// @internal @[Shutdown-The-Network-cpp]
  static void ShutdownTheNetwork(MotionController *controller)
  {
    // Check if the network is already shutdown
    if (controller->NetworkStateGet() == RSINetworkState::RSINetworkStateUNINITIALIZED ||
        controller->NetworkStateGet() == RSINetworkState::RSINetworkStateSHUTDOWN)
    {
      return;
    }

    // Shutdown the network
    std::cout << "Shutting down the network.." << std::endl;
    controller->NetworkShutdown();

    if (controller->NetworkStateGet() != RSINetworkState::RSINetworkStateUNINITIALIZED &&
        controller->NetworkStateGet() != RSINetworkState::RSINetworkStateSHUTDOWN) // Check if the network is shutdown.
    {
      int messagesToRead = controller->NetworkLogMessageCountGet(); // Some kind of error shutting down the network, read the network log messages

      for (int i = 0; i < messagesToRead; i++)
      {
        std::cout << controller->NetworkLogMessageGet(i) << std::endl; // Print all the messages to help figure out the problem
      }
      throw std::runtime_error("Expected SHUTDOWN state but the network did not get there.");
    }
    else // Else, of network is shutdown.
    {
      std::cout << "Network Shutdown" << std::endl << std::endl;
    }
  }
  /// @internal @[Shutdown-The-Network-cpp]

  /*!
    @brief Configures a specified axis as a phantom axis with custom settings.
    @param controller Pointer to the MotionController containing the axis.
    @param axisIndex The index of the axis to configure as a phantom axis.
    @exception std::invalid_argument Thrown if an invalid axis index is provided.
    @snippet SampleAppsHelper.h Configure-Phantom-Axis-cpp
  */
  /// @internal @[Configure-Phantom-Axis-cpp]
  static void ConfigurePhantomAxis(MotionController *controller, int axisIndex)
  {
    // Check that the axis has been initialized correctly
    Axis* axis = controller->AxisGet(axisIndex);
    CheckErrors(axis);

    // These limits are not meaningful for a Phantom Axis (e.g., a phantom axis has no actual position so a position error trigger is not necessary)
    // Therefore, you must set all of their actions to "NONE".
    axis->PositionSet(0);                                      // Set the position to 0
    axis->ErrorLimitActionSet(RSIAction::RSIActionNONE);       // Set Error Limit Action.
    axis->AmpFaultActionSet(RSIAction::RSIActionNONE);         // Set Amp Fault Action.
    axis->AmpFaultTriggerStateSet(1);                          // Set Amp Fault Trigger State.
    axis->HardwareNegLimitActionSet(RSIAction::RSIActionNONE); // Set Hardware Negative Limit Action.
    axis->HardwarePosLimitActionSet(RSIAction::RSIActionNONE); // Set Hardware Positive Limit Action.
    axis->SoftwareNegLimitActionSet(RSIAction::RSIActionNONE); // Set Software Negative Limit Action.
    axis->SoftwarePosLimitActionSet(RSIAction::RSIActionNONE); // Set Software Positive Limit Action.
    axis->HomeActionSet(RSIAction::RSIActionNONE);             // Set Home Action.

    const double positionToleranceMax =
        std::numeric_limits<double>::max() /
        10.0; // Reduce from max slightly, so XML to string serialization and deserialization works without throwing System.OverflowException
    axis->PositionToleranceCoarseSet(positionToleranceMax); // Set Settling Coarse Position Tolerance to max value
    axis->PositionToleranceFineSet(positionToleranceMax
    ); // Set Settling Fine Position Tolerance to max value (so Phantom axis will get immediate MotionDone when target is reached)

    axis->MotorTypeSet(RSIMotorType::RSIMotorTypePHANTOM); // Set the MotorType to phantom
  }
  /// @internal @[Configure-Phantom-Axis-cpp]

  static int initialAxisCount = -1; ///< Initial axis count to restore later (-1 indicates it has not been set)
  static int initialMotionCount = -1; ///< Initial motion count to restore later (-1 indicates it has not been set)

  /*!
    @brief Setup the controller and check if the network is in the correct state for the configuration.
    @snippet SampleAppsHelper.h Setup-Controller-cpp
  */
  /// @internal @[Setup-Controller-cpp]
  static void SetupController(MotionController *controller, int numAxes = 0)
  {
    // Start or shutdown the network depending on the configuration
    if (SampleAppsConfig::USE_HARDWARE)
    {
      StartTheNetwork(controller);
    }
    else if (controller->NetworkStateGet() != RSINetworkState::RSINetworkStateUNINITIALIZED &&
      controller->NetworkStateGet() != RSINetworkState::RSINetworkStateSHUTDOWN)
    {
      std::ostringstream message;    
      message << "The Sample Apps are configured to use Phantom Axes, but the network is not in the UNINITIALIZED or SHUTDOWN state.\n"
              << "If you intended to run with hardware, then follow the steps in README.md and /src/SampleAppsHelper.h\n"
              << "Otherwise, shutdown the network before running the sample apps with phantom axes.";

      throw std::runtime_error(message.str().c_str());
    }

    // Check if we are using hardware or phantom axes
    if (SampleAppsConfig::USE_HARDWARE)
    {
      // Let the user defined function configure the axes
      SampleAppsConfig::ConfigureHardwareAxes(controller);
      
      // Check that there are enough axes configured
      int axisCount = controller->AxisCountGet();
      if (axisCount < numAxes)
      {
        std::ostringstream message;
        message << "Error! Not enough axes configured. Expected " << numAxes << " axes but only found " << axisCount << " axes."
                << "Please configure the axes in the SampleAppsConfig::ConfigureHardwareAxes function.";
        throw std::runtime_error(message.str().c_str());
      }
    }
    else
    {
      // Record the initial object counts to restore later
      initialAxisCount = controller->AxisCountGet();
      initialMotionCount = controller->MotionCountGet();

      // Add phantom axes for the sample app if needed
      if (numAxes > initialAxisCount)
      {
        controller->AxisCountSet(numAxes);
      }
      
      for (int i = 0; i < numAxes; i++)
      {
        ConfigurePhantomAxis(controller, i);
      }
    }
  }
  /// @internal @[Setup-Controller-cpp]

  /*!
    @brief Cleanup the controller and restore the object counts to the original values.
    @snippet SampleAppsHelper.h Cleanup-cpp
  */
  /// @internal @[Cleanup-cpp]
  static void Cleanup(MotionController* controller)
  {
    // Restore the object counts to the original values
    if (initialAxisCount != -1) { controller->AxisCountSet(initialAxisCount); }
    if (initialMotionCount != -1) { controller->MotionCountSet(initialMotionCount); }

    // Reset the object counts
    initialAxisCount = -1;
    initialMotionCount = -1;
  }
  /// @internal @[Cleanup-cpp]

  /*!
    @brief Print a start message to indicate that the sample app has started
    @param sampleAppName The name of the sample application to print in the header.
  */
  /// @internal @[Print-Header-cpp]
  static void PrintHeader(std::string sampleAppName)
  {
    std::cout << "----------------------------------------------------------------------------------------------------\n";
    std::cout << "Running " << sampleAppName << " Sample App\n";
    std::cout << "----------------------------------------------------------------------------------------------------\n" << std::endl;
  }
  /// @internal @[Print-Header-cpp]

  /*!
    @brief Print a message to indicate the sample app has finished and if it was successful or not
    @param sampleAppName The name of the sample application to print in the footer.
    @param exitCode The exit code of the sample application.
  */
  /// @internal @[Print-Footer-cpp]
  static void PrintFooter(std::string sampleAppName, int exitCode)
  {
    std::cout << "\n----------------------------------------------------------------------------------------------------\n";
    if (exitCode == 0)
    {
      std::cout << sampleAppName << " Sample App Completed Successfully\n";
    }
    else
    {
      std::cout << sampleAppName << " Sample App Failed with Exit Code: " << exitCode << "\n";
    }
    std::cout << "----------------------------------------------------------------------------------------------------\n" << std::endl;
  }
  /// @internal @[Print-Footer-cpp]
} // namespace SampleAppsHelper

#endif // SAMPLE_APPS_HELPER