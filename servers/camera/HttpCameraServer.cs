#:package Newtonsoft.Json@13.0.3

using System;
using System.IO;
using System.Net;
using System.Threading;
using System.Threading.Tasks;
using Newtonsoft.Json;

// ============================================================================================
// MAIN APPLICATION SCRIPT (.NET 10 File-Based App Style)
// Camera Server - Bridges RT Task data to UI clients  
// Run with: dotnet run SimpleCameraServer.cs
// ============================================================================================

const string EXECUTABLE_NAME = "Camera Data Bridge (C# Server)";
var shutdown = false;
var exitCode = 0;

Console.WriteLine("============================================");
Console.WriteLine($" {EXECUTABLE_NAME}");
Console.WriteLine("============================================");
Console.WriteLine($"Started at: {DateTime.Now:yyyy-MM-dd HH:mm:ss}");
Console.WriteLine();

// RT Task communication via shared data files
const string DATA_FILE_PATH = "/tmp/rsi_camera_data.json";
const string RT_TASK_RUNNING_FLAG = "/tmp/rsi_rt_task_running";

// Declare httpListener so it's in scope for both the handler and the try block
HttpListener httpListener = null;
// Handle Ctrl+C gracefully
Console.CancelKeyPress += (sender, e) =>
{
    e.Cancel = true;
    Console.WriteLine("SIGINT received, setting shutdown flag...");
    shutdown = true;
    // Attempt to stop and dispose the listener if possible
    try
    {
        httpListener?.Stop();
        httpListener?.Close();
    }
    catch (Exception ex)
    {
        Console.WriteLine($"Error disposing HttpListener: {ex.Message}");
    }
};
try
{
    // Simple HTTP server for camera data
    httpListener?.Stop();
    httpListener?.Close();
    httpListener = new HttpListener();
    httpListener.Prefixes.Add("http://localhost:50080/");
    httpListener.Start();

    Console.WriteLine("HTTP camera server started successfully on http://localhost:50080/");
    Console.WriteLine("Endpoints:");
    Console.WriteLine("  GET /camera/frame - Get latest camera frame");
    Console.WriteLine("  GET /status - Server status");
    Console.WriteLine();

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
                    var status = new { status = "running", server = EXECUTABLE_NAME };
                    var json = JsonConvert.SerializeObject(status);
                    var buffer = System.Text.Encoding.UTF8.GetBytes(json);

                    response.ContentType = "application/json";
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
    string serverAddress = "http://localhost:50080/";
    while (!shutdown)
    {
        Console.WriteLine($"[{DateTime.Now:HH:mm:ss}] Server running at {serverAddress}");
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
Console.WriteLine("============================================");
Console.WriteLine($" {EXECUTABLE_NAME} - Exit Code: {exitCode}");
Console.WriteLine($"Ended at: {DateTime.Now:yyyy-MM-dd HH:mm:ss}");
Console.WriteLine("============================================");

Environment.Exit(exitCode);
