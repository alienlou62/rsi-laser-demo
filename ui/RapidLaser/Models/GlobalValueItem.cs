namespace RapidLaser.Models;

public partial class GlobalValueItem : ObservableObject
{
    [ObservableProperty]
    private string _name = string.Empty;

    [ObservableProperty]
    private string _value = string.Empty;

    [ObservableProperty]
    private bool _isNumeric = true;

    public override string ToString() => Name;
}