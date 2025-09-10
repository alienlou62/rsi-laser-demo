
using Avalonia;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Data.Core.Plugins;
using Avalonia.Markup.Xaml;
using Microsoft.Extensions.DependencyInjection;
using RapidLaser.Services;
using RapidLaser.ViewModels;
using RapidLaser.Views;

namespace RapidLaser;

public partial class App : Application
{
    private ServiceProvider? _serviceProvider;

    public override void Initialize()
    {
        AvaloniaXamlLoader.Load(this);

        // Configure dependency injection
        var services = new ServiceCollection();
        ConfigureServices(services);
        _serviceProvider = services.BuildServiceProvider();
    }

    public override void OnFrameworkInitializationCompleted()
    {
        // Avoid duplicate validations from both Avalonia and the CommunityToolkit. 
        // More info: https://docs.avaloniaui.net/docs/guides/development-guides/data-validation#manage-validationplugins
        DisableAvaloniaDataAnnotationValidation();

        // get the mainviewmodel
        var mainViewModel = _serviceProvider?.GetRequiredService<MainViewModel>();

        if (ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop)
        {
            desktop.MainWindow = new MainWindow { DataContext = mainViewModel };

            // attach viewmodel events to the window
            mainViewModel?.SetMainWindow(desktop.MainWindow);

            // dispose service provider on appl shutdown
            desktop.ShutdownRequested += (s, e) => _serviceProvider?.Dispose();
        }
        else if (ApplicationLifetime is ISingleViewApplicationLifetime singleViewPlatform)
        {
            singleViewPlatform.MainView = new MainView { DataContext = mainViewModel };
        }

        base.OnFrameworkInitializationCompleted();
    }

    private void ConfigureServices(IServiceCollection services)
    {
        services.AddSingleton<ISshService, SshService>();
        services.AddSingleton<ICameraService, HttpCameraService>();
        services.AddSingleton<IRmpGrpcService, RmpGrpcService>();
        services.AddSingleton<MainViewModel>();
    }

    private void DisableAvaloniaDataAnnotationValidation()
    {
        // Get an array of plugins to remove
        var dataValidationPluginsToRemove =
            BindingPlugins.DataValidators.OfType<DataAnnotationsValidationPlugin>().ToArray();

        // remove each entry found
        foreach (var plugin in dataValidationPluginsToRemove)
        {
            BindingPlugins.DataValidators.Remove(plugin);
        }
    }
}