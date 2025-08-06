namespace RapidLaser.Services;

public interface ISshService
{
    Task<string> RunSshCommandAsync(string command, string sshUser, string sshPass, string? ipAddress = null);
}

public partial class SshService : ObservableObject, ISshService
{
    public async Task<string> RunSshCommandAsync(string command, string sshUser, string sshPass, string? ipAddress = null)
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

            // Wrap the command in a login shell to ensure environment is loaded
            var wrappedCommand = $"bash -l -c \"{command.Replace("\"", "\\\"")}\"";
            var sshCommand = client.CreateCommand(wrappedCommand);
            sshCommand.CommandTimeout = TimeSpan.FromSeconds(30);

            sshCommand.Execute();
            var result = string.IsNullOrEmpty(sshCommand.Error) ? sshCommand.Result : sshCommand.Error;

            client.Disconnect();

            return result;
        }
        catch (Exception ex)
        {
            throw new InvalidOperationException($"SSH command execution failed: {ex.Message}", ex);
        }
    }
}