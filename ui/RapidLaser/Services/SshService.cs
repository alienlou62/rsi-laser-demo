
namespace RapidLaser.Services;

public interface ISshService
{
    Task<string> RunSshCommandAsync(string command, string sshUser, string sshPass, string ipAddress);
}

public partial class SshService : ObservableObject, ISshService
{
    public async Task<string> RunSshCommandAsync(string command, string sshUser, string sshPass, string ipAddress)
    {

        if (string.IsNullOrWhiteSpace(command))
        {
            throw new ArgumentException("Command cannot be null or empty.", nameof(command));
        }

        try
        {
            using var client = new SshClient(ipAddress, sshUser, sshPass);

            // Set connection timeout
            client.ConnectionInfo.Timeout = TimeSpan.FromSeconds(30);

            await Task.Run(client.Connect);

            if (!client.IsConnected)
            {
                throw new InvalidOperationException("Failed to establish SSH connection.");
            }

            // Use shell stream approach - more reliable for commands with output
            using var shell = client.CreateShellStream("xterm", 80, 24, 800, 600, 1024);

            var result = await Task.Run(() =>
            {
                var output = new StringBuilder();

                // Wait a moment for shell to initialize
                Thread.Sleep(500);

                // Change to correct directory
                shell.WriteLine("cd ~/Documents/rsi-laser-demo");
                Thread.Sleep(200);

                // Clear any existing output
                shell.Flush();

                // Execute the command
                shell.WriteLine(command);

                // Read output for up to 5 seconds
                var startTime = DateTime.Now;
                var lastActivity = DateTime.Now;

                while ((DateTime.Now - startTime).TotalSeconds < 5)
                {
                    if (shell.DataAvailable)
                    {
                        var data = shell.Read();
                        output.Append(data);
                        lastActivity = DateTime.Now;
                    }
                    else
                    {
                        // If no activity for 1 second after we've gotten some output, assume done
                        if (output.Length > 0 && (DateTime.Now - lastActivity).TotalSeconds > 1)
                        {
                            break;
                        }
                        Thread.Sleep(100);
                    }
                }

                return output.ToString();
            });

            client.Disconnect();

            // Clean up the output - remove shell prompts and command echo
            var lines = result.Split('\n');
            var cleanOutput = new StringBuilder();
            var foundCommand = false;

            foreach (var line in lines)
            {
                // Skip until we find our command
                if (!foundCommand && line.Contains(command))
                {
                    foundCommand = true;
                    continue;
                }

                // Stop when we see a new shell prompt
                if (foundCommand && (line.Contains("$") || line.Contains("#")) && line.Contains("@"))
                {
                    break;
                }

                // Collect output lines
                if (foundCommand && !string.IsNullOrWhiteSpace(line))
                {
                    cleanOutput.AppendLine(line.Trim());
                }
            }

            var finalOutput = cleanOutput.ToString().Trim();
            return string.IsNullOrEmpty(finalOutput) ? "Command executed but no output captured" : finalOutput;
        }
        catch (Exception ex)
        {
            throw new InvalidOperationException($"SSH command execution failed: {ex.Message}", ex);
        }
    }
}