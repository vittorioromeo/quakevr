namespace QuakeVR.Installer.Core.Quest;

public enum QuestDeviceState
{
    /// <summary>No headset seen by adb.</summary>
    None,
    /// <summary>Plugged in, but "Allow USB debugging" was not accepted in the headset yet.</summary>
    Unauthorized,
    /// <summary>adb sees it but cannot talk to it (asleep, a bad cable, adb restarting).</summary>
    Offline,
    /// <summary>More than one device is connected and none was chosen.</summary>
    Several,
    /// <summary>One headset, ready.</summary>
    Ready,
}

public sealed record AdbDevice(string Serial, string State, string? Model);

/// <summary>What <c>adb devices -l</c> says, as one state with a message for the player.</summary>
public sealed record QuestDeviceStatus(QuestDeviceState State, AdbDevice? Device, IReadOnlyList<AdbDevice> All)
{
    public bool Ready => State == QuestDeviceState.Ready;

    public string Message => State switch
    {
        QuestDeviceState.None => "No headset found. Plug the Quest into this PC with a USB cable (a data cable, not a charge-only one), " +
                                 "make sure developer mode is on, then press Check again.",
        QuestDeviceState.Unauthorized => "The Quest is connected but has not allowed this PC yet. Put the headset on and accept " +
                                         "\"Allow USB debugging\" (tick \"Always allow from this computer\"), then press Check again.",
        QuestDeviceState.Offline => "The Quest is connected but not answering (asleep?). Put the headset on to wake it, unplug and " +
                                    "plug the cable again, then press Check again.",
        QuestDeviceState.Several => $"{All.Count} devices are connected ({string.Join(", ", All.Select(d => d.Model ?? d.Serial))}). " +
                                    "Unplug all but the Quest, then press Check again.",
        _ => $"Quest found: {Device!.Model ?? "headset"} ({Device.Serial}).",
    };

    /// <summary>Parses <c>adb devices -l</c>. <paramref name="serial"/> picks one of several (the CLI's --serial).</summary>
    public static QuestDeviceStatus Parse(string output, string? serial = null)
    {
        var all = new List<AdbDevice>();
        foreach (var raw in output.Split('\n'))
        {
            var line = raw.Trim();
            if (line.Length == 0 || line.StartsWith("List of devices", StringComparison.OrdinalIgnoreCase) || line.StartsWith('*'))
            {
                continue;
            }
            var parts = line.Split((char[])[' ', '\t'], StringSplitOptions.RemoveEmptyEntries);
            if (parts.Length < 2)
            {
                continue;
            }
            var model = parts.Skip(2).FirstOrDefault(p => p.StartsWith("model:", StringComparison.Ordinal))?["model:".Length..].Replace('_', ' ');
            all.Add(new AdbDevice(parts[0], parts[1], model));
        }
        var pool = serial is null ? all : all.Where(d => d.Serial == serial).ToList();
        if (pool.Count == 0)
        {
            return new(QuestDeviceState.None, null, all);
        }
        if (pool.Count > 1)
        {
            return new(QuestDeviceState.Several, null, all);
        }
        var d = pool[0];
        return new(d.State switch
        {
            "device" => QuestDeviceState.Ready,
            "unauthorized" => QuestDeviceState.Unauthorized,
            _ => QuestDeviceState.Offline,
        }, d, all);
    }
}

/// <summary>The headset is not ready: the message says what to do.</summary>
public sealed class QuestDeviceException(QuestDeviceStatus status) : Exception(status.Message)
{
    public QuestDeviceStatus Status { get; } = status;
}
