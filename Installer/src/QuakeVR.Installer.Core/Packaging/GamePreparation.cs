using System.Diagnostics;
using System.Globalization;
using QuakeVR.Installer.Core.Shortcuts;

namespace QuakeVR.Installer.Core.Packaging;

/// <summary>A step of the preparation run: the bar's fraction and the status line when they change (null: as they
/// were), and a line for the log.</summary>
public sealed record PreparationProgress(double? Fraction, string? Status, string? Log = null, LogLevel Level = LogLevel.Info);

/// <summary>What the preparation run did (<see cref="GamePreparation"/>).</summary>
public sealed class PreparationResult
{
    /// <summary>The game started and said so.</summary>
    public bool Started { get; set; }
    /// <summary>It got to its end ("done").</summary>
    public bool Finished { get; set; }
    /// <summary>Cancelled (Skip): the game stopped, with its light processes.</summary>
    public bool Cancelled { get; set; }
    public int MapsOk { get; set; }
    public int MapsFailed { get; set; }
    public bool RelightAsked { get; set; }
    /// <summary>The relight's batch ended with no map failed or cancelled: the game's first start relights nothing.</summary>
    public bool RelightDone { get; set; }
    public int Relit { get; set; }
    public int RelightSkipped { get; set; }
    public int RelightFailed { get; set; }
    /// <summary>The game's own last word on the relight (Graphics > Relighting's status line).</summary>
    public string? RelightStatus { get; set; }
    /// <summary>Why it stopped early (null: it did not).</summary>
    public string? Error { get; set; }
    public TimeSpan Elapsed { get; set; }
}

/// <summary>
/// The work a first start would otherwise do in the headset, done by the installer with the game hidden (the engine's
/// <c>-prepare</c> run, Quake/vr/vr_prepare.cpp): Quake VR's first maps loaded once (their compiled hulls, ambient
/// occlusion and normal maps written to the game's disk caches: vrstart's first load 12 s to under 1 s), then, when the
/// relight is pending, every map relit with the bundled ericw-tools, exactly as the game's first start would (the same
/// command, <c>vr_relight_batch everything</c>, so the game finds them current and relights nothing). A relight that
/// finished removes the first-start marker (<see cref="FirstStartRelight"/>); one that did not (skipped, failed, no
/// light.exe) leaves it, and the game relights at its first start as before. Never fatal: the install is done already.
/// </summary>
public static class GamePreparation
{
    /// <summary>The progress file, kept as the last run's log: <c>&lt;QVR&gt;\quakevr\cache\setup_prepare.txt</c>.</summary>
    public static string ProgressPath(string installDir) => Path.Combine(installDir, "quakevr", "cache", "setup_prepare.txt");

    /// <summary>A run that writes nothing for this long is stopped (a cold vrstart on a slow PC: about a minute; the
    /// relight writes a line every half second).</summary>
    public static readonly TimeSpan Stall = TimeSpan.FromMinutes(15);

    /// <summary>The share of the bar the maps take (the relight the rest).</summary>
    const double MapsPart = 0.15;

    /// <summary>The game's command line: the shortcut's, then the preparation run's switches (as the motion review starts
    /// its copies: the mock headset, so no VR runtime is opened; no config written; no player's autoexec; no sound).</summary>
    public static ProcessStartInfo StartInfo(string installDir, string quakeDir, bool relight)
    {
        var extra = $"-prepare {LaunchCommand.Quote(ProgressPath(installDir))}{(relight ? " -preparerelight" : "")} " +
                    "-vrmock -noconfigwrite -noautoexec -nosound -nomapindex -noaddons -window -width 640 -height 360";
        var info = new ProcessStartInfo(Path.Combine(installDir, LaunchCommand.Exe), LaunchCommand.Arguments(quakeDir, installDir, LaunchVariant.Vr, extra))
        {
            WorkingDirectory = installDir,
            UseShellExecute = false,
            CreateNoWindow = true,
        };
        info.Environment["QVR_TEST_HIDDEN"] = "1";     // no window
        info.Environment["QVR_TEST_BACKGROUND"] = "1"; // no network (update check, map index), quits without asking
        info.Environment["QVR_NO_ERROR_DIALOG"] = "1"; // an error goes to qvr_error.txt, never a dialog no one sees
        return info;
    }

    /// <summary>Applies one progress line to the result; returns what to show (fraction, status, a log line), or null.</summary>
    public static PreparationProgress? Apply(PreparationResult r, string line)
    {
        var words = line.Split(' ', StringSplitOptions.RemoveEmptyEntries);
        if (words.Length == 0)
        {
            return null;
        }
        static int Int(string s) => int.TryParse(s, NumberStyles.Integer, CultureInfo.InvariantCulture, out var v) ? v : 0;
        static string Map(string m) => m switch
        {
            "vrcalibration" => "the calibration room",
            "vrtutorial" => "the tutorial",
            "vrstart" => "the hub",
            _ => m,
        };
        switch (words[0])
        {
            case "start":
                r.Started = true;
                return new PreparationProgress(0, "Preparing the game", $"Preparing the game ({string.Join(' ', words.Skip(1))}): its first maps loaded once" +
                                                                   (r.RelightAsked ? ", then every map relit" : "") + ", so the first start doesn't wait for them.");
            case "step" when words.Length >= 5 && words[1] == "maps":
            {
                var i = Int(words[2]);
                var n = Math.Max(1, Int(words[3]));
                return new PreparationProgress(MapsShare(r) * (i - 1) / n, $"Preparing the game: loading {Map(words[4])} ({i} of {n})");
            }
            case "map" when words.Length >= 4:
            {
                var ok = words[2] == "ok";
                if (ok)
                {
                    ++r.MapsOk;
                }
                else
                {
                    ++r.MapsFailed;
                }
                return new PreparationProgress(null, null, ok ? $"Prepared {Map(words[1])} ({words[3]} s)." : $"{Map(words[1])} did not load in time: the game prepares it at its first visit.",
                    ok ? LogLevel.Info : LogLevel.Warning);
            }
            case "step" when words.Length >= 2 && words[1] == "relight":
                return new PreparationProgress(MapsShare(r), "Relighting the maps", "Relighting every map with ericw-tools (the game's own relight, as at its first start).");
            case "relight" when words.Length >= 2:
            {
                var percent = Math.Clamp(Int(words[1]), 0, 100);
                var text = string.Join(' ', words.Skip(2));
                return new PreparationProgress(MapsShare(r) + (1 - MapsShare(r)) * percent / 100.0, text.Length > 0 ? text : "Relighting the maps");
            }
            case "result" when words.Length >= 2 && words[1] == "relight":
            {
                var bar = line.IndexOf(" | ", StringComparison.Ordinal);
                r.RelightStatus = bar >= 0 ? line[(bar + 3)..].Trim() : null;
                var fields = (bar >= 0 ? line[..bar] : line).Split(' ', StringSplitOptions.RemoveEmptyEntries)
                    .Select(w => w.Split('=', 2)).Where(p => p.Length == 2).ToDictionary(p => p[0], p => Int(p[1]));
                int F(string k) => fields.TryGetValue(k, out var v) ? v : 0;
                r.Relit = F("relit");
                r.RelightSkipped = F("skipped");
                r.RelightFailed = F("failed");
                r.RelightDone = F("ended") == 1 && F("failed") == 0 && F("cancelled") == 0;
                return new PreparationProgress(1, "Relighting the maps", r.RelightDone
                    ? $"Relit {r.Relit} map(s){(r.RelightSkipped > 0 ? $", {r.RelightSkipped} relit before and kept" : "")}: the game's first start relights nothing."
                    : $"The relight did not finish ({r.RelightStatus ?? "no result"}): the game relights at its first start instead.",
                    r.RelightDone ? LogLevel.Success : LogLevel.Warning);
            }
            case "done":
                r.Finished = true;
                return new PreparationProgress(1, null);
            default:
                return null;
        }

        static double MapsShare(PreparationResult r) => r.RelightAsked ? MapsPart : 1.0;
    }

    /// <summary>Runs the game's preparation and waits for it (<paramref name="ct"/>: stops it, with its light
    /// processes). Then, with the relight finished, the first-start marker removed and install.json says so.</summary>
    public static Task<PreparationResult> RunAsync(string installDir, string quakeDir, bool relight, IProgress<PreparationProgress>? progress,
        CancellationToken ct) => Task.Run(() => Run(installDir, quakeDir, relight, progress, ct), CancellationToken.None);

    public static PreparationResult Run(string installDir, string quakeDir, bool relight, IProgress<PreparationProgress>? progress, CancellationToken ct)
    {
        var result = new PreparationResult { RelightAsked = relight };
        var watch = Stopwatch.StartNew();
        var path = ProgressPath(installDir);
        var exe = Path.Combine(installDir, LaunchCommand.Exe);
        if (!File.Exists(exe))
        {
            result.Error = $"{LaunchCommand.Exe} not found";
            return result;
        }
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        File.WriteAllText(path, ""); // (a line from an earlier run is never read as this one's)
        var errorFile = Path.Combine(installDir, "qvr_error.txt");
        var errorBefore = File.Exists(errorFile) ? File.GetLastWriteTimeUtc(errorFile) : DateTime.MinValue;

        Process process;
        try
        {
            process = Process.Start(StartInfo(installDir, quakeDir, relight)) ?? throw new InvalidOperationException("no process");
        }
        catch (Exception e) when (e is System.ComponentModel.Win32Exception or InvalidOperationException)
        {
            result.Error = $"the game could not start ({e.Message})";
            return result;
        }
        using (process)
        {
            long read = 0;
            var pending = "";
            var lastLine = DateTime.UtcNow;
            void ReadNew()
            {
                try
                {
                    using var fs = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete);
                    if (fs.Length <= read)
                    {
                        return;
                    }
                    fs.Seek(read, SeekOrigin.Begin);
                    using var sr = new StreamReader(fs);
                    var text = pending + sr.ReadToEnd();
                    read = fs.Length;
                    var lines = text.Split('\n');
                    pending = lines[^1]; // (a line not finished yet)
                    foreach (var raw in lines[..^1])
                    {
                        lastLine = DateTime.UtcNow;
                        if (Apply(result, raw.TrimEnd('\r')) is { } p)
                        {
                            progress?.Report(p);
                        }
                    }
                }
                catch (IOException)
                {
                    // (being written: the next poll reads it)
                }
            }
            while (!process.WaitForExit(250))
            {
                ReadNew();
                if (ct.IsCancellationRequested || DateTime.UtcNow - lastLine > Stall)
                {
                    var stalled = !ct.IsCancellationRequested;
                    try
                    {
                        process.Kill(entireProcessTree: true); // (the relight's light processes too)
                        process.WaitForExit(10000);
                    }
                    catch (Exception e) when (e is InvalidOperationException or System.ComponentModel.Win32Exception)
                    {
                        // (it ended meanwhile)
                    }
                    result.Cancelled = !stalled;
                    result.Error = stalled ? $"the game wrote nothing for {Stall.TotalMinutes:0} minutes and was stopped" : "skipped";
                    break;
                }
            }
            ReadNew();
            if (result.Error is null && !result.Finished)
            {
                result.Error = $"the game stopped early (exit code {process.ExitCode})";
                if (File.Exists(errorFile) && File.GetLastWriteTimeUtc(errorFile) > errorBefore)
                {
                    result.Error += ": " + string.Join(' ', File.ReadAllLines(errorFile).Take(3)).Trim();
                }
            }
        }
        result.Elapsed = watch.Elapsed;
        Conclude(installDir, result);
        return result;
    }

    /// <summary>A relight that finished: the first-start marker removed and install.json's RelightPending cleared (the
    /// game's first start relights nothing). Otherwise both stay: the game relights at its first start, as before.</summary>
    public static void Conclude(string installDir, PreparationResult result)
    {
        if (!result.RelightAsked || !result.RelightDone || !result.Finished)
        {
            return;
        }
        FirstStartRelight.Set(installDir, false);
        if (InstallRecord.Load(installDir) is { RelightPending: true } record)
        {
            record.RelightPending = false;
            record.Save(installDir);
        }
    }
}
