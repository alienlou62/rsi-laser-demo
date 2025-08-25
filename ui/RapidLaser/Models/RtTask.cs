namespace RapidLaser.Models;

public partial class RtTask : ObservableObject
{
    /** FIELDS **/
    public int Id { get; private set; }

    //creation parameters
    [ObservableProperty]
    private string _function = string.Empty;

    [ObservableProperty]
    private int _period = 0;

    //status
    [ObservableProperty]
    private RTTaskState _state = RTTaskState.Unknown;

    [ObservableProperty]
    private long _executionCount = 0;

    [ObservableProperty]
    private double _executionTimeMax = 0;

    [ObservableProperty]
    private double _executionTimeMin = 0;

    [ObservableProperty]
    private double _executionTimeMean = 0;

    [ObservableProperty]
    private double _executionTimeLast = 0;

    [ObservableProperty]
    private double _startTimeDeltaLast = 0;

    [ObservableProperty]
    private double _startTimeDeltaMax = 0;

    [ObservableProperty]
    private double _startTimeDeltaMean = 0;

    [ObservableProperty]
    private string _errorMessage = string.Empty;


    /** CONSTRUCTOR **/
    public RtTask(int id)
    {
        Id = id;
    }


    /** METHODS **/
    public void UpdateTaskInfo(RTTaskStatus status)
    {
        State = status.State;
        ExecutionCount = status.ExecutionCount;
        ExecutionTimeMax = status.ExecutionTimeMax / 1_000_000.0;  // Convert nanoseconds to milliseconds
        ExecutionTimeMin = status.ExecutionTimeMin / 1_000_000.0;  // Convert nanoseconds to milliseconds
        ExecutionTimeMean = status.ExecutionTimeMean / 1_000_000.0;  // Convert nanoseconds to milliseconds
        ExecutionTimeLast = status.ExecutionTimeLast / 1_000_000.0;  // Convert nanoseconds to milliseconds
        StartTimeDeltaLast = status.StartTimeDeltaLast / 1_000_000.0;  // Convert nanoseconds to milliseconds
        StartTimeDeltaMax = status.StartTimeDeltaMax / 1_000_000.0;  // Convert nanoseconds to milliseconds
        StartTimeDeltaMean = status.StartTimeDeltaMean / 1_000_000.0;  // Convert nanoseconds to milliseconds
        ErrorMessage = status.ErrorMessage;
    }
}