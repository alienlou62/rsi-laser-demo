#:package Newtonsoft.Json@13.0.3

using System;
using System.Collections.Generic;
using System.IO;
using System.Net;
using System.Net.NetworkInformation;
using System.Net.Sockets;
using System.Threading;
using System.Threading.Tasks;
using System.Linq;
using Newtonsoft.Json;

// ============================================================================================
// MAIN APPLICATION SCRIPT (.NET 10 File-Based App Style)
// Camera Server - Bridges RTTask data to UI clients
// Run with (.NET 10): dotnet run HttpCameraServer.cs
// ============================================================================================
var shutdown = false;
var exitCode = 0;

// RT Task communication via shared data files
const string DATA_FILE_PATH = "/tmp/rsi_camera_data.json";
const string RT_TASK_RUNNING_FLAG = "/tmp/rsi_rt_task_running";

// Declare httpListener so it's in scope for all handlers
HttpListener httpListener = null;

// Unified cleanup logic
void Cleanup()
{
    Console.WriteLine("Disposing HttpListener and cleaning up...");
    try
    {
        httpListener?.Stop();
        httpListener?.Close();
    }
    catch (Exception ex)
    {
        Console.WriteLine($"Error disposing HttpListener: {ex.Message}");
    }
}

// Handle Ctrl+C gracefully
Console.CancelKeyPress += (sender, e) =>
{
    e.Cancel = true;
    Console.WriteLine("SIGINT received, setting shutdown flag...");
    shutdown = true;
    Cleanup();
};

// Handle process exit (SIGTERM, normal exit)
AppDomain.CurrentDomain.ProcessExit += (sender, e) =>
{
    Console.WriteLine("ProcessExit event received.");
    Cleanup();
};

// Handle unhandled exceptions
AppDomain.CurrentDomain.UnhandledException += (sender, e) =>
{
    Console.WriteLine("UnhandledException event received.");
    Cleanup();
};
try
{
    // Simple HTTP server for camera data
    httpListener?.Stop();
    httpListener?.Close();
    httpListener = new HttpListener();

    // Add prefixes for all available IP addresses
    httpListener.Prefixes.Add("http://+:50080/"); // Bind to all interfaces
    try
    {
        httpListener.Start();
    }
    catch (HttpListenerException ex) when (ex.ErrorCode == 10013 || ex.Message.Contains("Address already in use"))
    {
        Console.WriteLine("ERROR: Port 50080 is already in use.\n");
        Console.WriteLine("To fix this, run:");
        Console.WriteLine("sudo lsof -i :50080      // List processes using port 50080");
        Console.WriteLine("sudo kill -9 <PID>       // Kill the process using 50080\n");
        exitCode = 1;
        throw;
    }

    var localIPs = GetLocalIPAddresses();
    Console.WriteLine("HTTP camera server started successfully on all network interfaces (port 50080)");
    Console.WriteLine("Available endpoints:");
    foreach (var ip in localIPs)
    {
        Console.WriteLine($"  GET http://{ip}:50080/camera/frame    - Get latest camera frame");
        Console.WriteLine($"  GET http://{ip}:50080/status          - Server status");
    }
    Console.WriteLine();

    // Function to get all available IP addresses
    string[] GetLocalIPAddresses()
    {
        var addresses = new List<string>();

        try
        {
            // Get all network interfaces
            var networkInterfaces = NetworkInterface.GetAllNetworkInterfaces()
                .Where(ni => ni.OperationalStatus == OperationalStatus.Up &&
                           ni.NetworkInterfaceType != NetworkInterfaceType.Loopback);

            foreach (var networkInterface in networkInterfaces)
            {
                var properties = networkInterface.GetIPProperties();
                foreach (var address in properties.UnicastAddresses)
                {
                    if (address.Address.AddressFamily == AddressFamily.InterNetwork) // IPv4 only
                    {
                        addresses.Add(address.Address.ToString());
                    }
                }
            }

            // Always include localhost addresses
            if (!addresses.Contains("127.0.0.1"))
                addresses.Insert(0, "127.0.0.1");
            if (!addresses.Contains("localhost"))
                addresses.Insert(0, "localhost");
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Error getting IP addresses: {ex.Message}");
            addresses.AddRange(new[] { "localhost", "127.0.0.1" });
        }

        return addresses.ToArray();
    }

    // Function to read camera data from RT tasks
    object GetCameraDataFromRTTasks()
    {
        try
        {
            if (File.Exists(DATA_FILE_PATH))
            {
                var jsonData = File.ReadAllText(DATA_FILE_PATH);
                return JsonConvert.DeserializeObject(jsonData) ?? CreateMockFrame();
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Error reading RT task data: {ex.Message}");
        }

        return CreateMockFrame();
    }

    object CreateMockFrame()
    {
        // Create a simple 1x1 red pixel as mock image data
        var mockImageBytes = new byte[] { 0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 0x4A, 0x46, 0x49, 0x46, 0x00, 0x01 }; // JPEG header
        var base64Image = Convert.ToBase64String(mockImageBytes);

        return new
        {
            imageData = $"data:image/jpeg;base64,{base64Image}"
        };
    }

    // Start HTTP request handling task
    var httpTask = Task.Run(async () =>
    {
        while (!shutdown && httpListener.IsListening)
        {
            try
            {
                var context = await httpListener.GetContextAsync();
                var response = context.Response;
                var url = context.Request.Url?.AbsolutePath ?? "";

                Console.WriteLine($"Request: {url}");

                if (url == "/camera/frame")
                {
                    // Get data from RT tasks via shared data file
                    var frameData = GetCameraDataFromRTTasks();

                    var json = JsonConvert.SerializeObject(frameData, Formatting.Indented);
                    var buffer = System.Text.Encoding.UTF8.GetBytes(json);

                    response.ContentType = "application/json";
                    response.ContentLength64 = buffer.Length;
                    await response.OutputStream.WriteAsync(buffer, 0, buffer.Length);
                }
                else if (url == "/status")
                {
                    var buffer = System.Text.Encoding.UTF8.GetBytes("OK");
                    response.ContentType = "text/plain";
                    response.ContentLength64 = buffer.Length;
                    await response.OutputStream.WriteAsync(buffer, 0, buffer.Length);
                }
                else
                {
                    response.StatusCode = 404;
                    var error = System.Text.Encoding.UTF8.GetBytes("Not Found");
                    response.ContentLength64 = error.Length;
                    await response.OutputStream.WriteAsync(error, 0, error.Length);
                }

                response.Close();
            }
            catch (Exception ex) when (!shutdown)
            {
                Console.WriteLine($"HTTP request error: {ex.Message}");
            }
        }
    });

    // Main monitoring loop
    while (!shutdown)
    {
        var addresses = GetLocalIPAddresses();
        var addressList = string.Join(", ", addresses.Select(ip => $"http://{ip}:50080"));
        Console.WriteLine($"[{DateTime.Now:HH:mm:ss}] Server running on: {addressList}");
        await Task.Delay(5000); // Check every 5 seconds
    }

    Console.WriteLine("\nShutdown initiated...");

    httpListener.Stop();
    httpListener.Close();
    httpListener = new HttpListener();
    httpListener.Prefixes.Add("http://localhost:50080/");
    httpListener.Start();
    await httpTask;
    Console.WriteLine("HTTP server shutdown complete");
}
catch (Exception ex)
{
    Console.WriteLine($"Application exception: {ex.Message}");
    Console.WriteLine($"Stack trace: {ex.StackTrace}");
    exitCode = 1;
}

Console.WriteLine();
Console.WriteLine($"Exit Code: {exitCode}");
Console.WriteLine($"Ended at: {DateTime.Now:yyyy-MM-dd HH:mm:ss}");

Environment.Exit(exitCode);
