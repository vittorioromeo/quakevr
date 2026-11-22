using System.IO.Compression;
using System.Text;
using QuakeVR.Installer.Core;
using QuakeVR.Installer.Core.Packaging;
using QuakeVR.Installer.Core.Quest;

namespace QuakeVR.Installer.SelfTest;

/// <summary>The Quest target (Core/Quest): the device states, adb's discovery and download, the file plan, and the whole
/// procedure against <see cref="FakeAdb"/>: a PC install, then the Quest from it; reinstall skipping; no device,
/// unauthorized, several devices, a full headset, a failed APK install. Nothing here talks to a real device.</summary>
static class QuestTests
{
    static void Eq<T>(T expected, T actual, string what)
    {
        if (!EqualityComparer<T>.Default.Equals(expected, actual))
        {
            throw new Exception($"{what}: expected <{expected}>, got <{actual}>");
        }
    }

    static void True(bool condition, string what)
    {
        if (!condition)
        {
            throw new Exception(what);
        }
    }

    static T Throws<T>(Action a, string what) where T : Exception
    {
        try
        {
            a();
        }
        catch (T e)
        {
            return e;
        }
        throw new Exception($"{what}: expected {typeof(T).Name}");
    }

    /// <summary>A PC install (package, id1 with upper-case paks as some stores ship them, Armagon, HD textures) and an APK.</summary>
    sealed record Setup(string Quake, string Install, string Apk, List<QuestFile> Files);

    static Setup MakeSetup(Func<string, string> dir, string name)
    {
        var quake = dir(name + "-quake");
        Fixtures.MakeOriginal(quake);
        File.Move(Path.Combine(quake, "id1", "pak1.pak"), Path.Combine(quake, "id1", "PAK1.PAK"));
        Fixtures.MakePack(quake, "hipnotic");
        var music = Directory.CreateDirectory(Path.Combine(quake, "id1", "music")).FullName;
        File.WriteAllText(Path.Combine(music, "Track02.ogg"), "ogg");
        File.WriteAllText(Path.Combine(music, "Track03.WAV"), "wav");
        var target = Path.Combine(dir(name + "-pc"), "QuakeVR");
        new InstallEngine().Install(new InstallPlan
        {
            PackagePath = Fixtures.MakePackage(dir(name + "-pkg"), "1.2.0 (2026-11-22 aaaaaaa1)"),
            TargetDir = target,
            QuakeDir = quake,
            HdTexturesZip = Fixtures.TextureZip(Path.Combine(dir(name + "-tex"), "uhd.zip")),
            OwnedPacks = ["hipnotic"],
        }, null, CancellationToken.None);
        // Maps the PC game relit (quakevr\relit_custom\<game>\maps): e1m1 complete (in "ID1", as a PC folder may be named),
        // e1m3 without its note (half made), Dissolution's (not on this PC's Quest), a moved-aside stale copy.
        void Relit(string game, string map, params string[] exts)
        {
            var maps = Directory.CreateDirectory(Path.Combine(target, "quakevr", "relit_custom", game, "maps")).FullName;
            foreach (var ext in exts)
            {
                File.WriteAllText(Path.Combine(maps, map + ext), $"{game} {map}{ext}");
            }
        }
        Relit("ID1", "e1m1", ".bsp", ".lit", ".relight");
        Relit("ID1", "e1m3", ".bsp", ".lit");
        Relit("rogue", "r1m1", ".bsp", ".lit", ".relight");
        Relit("_stale", "e1m2", ".bsp", ".relight");
        var apk = Path.Combine(dir(name + "-apk"), "QuakeVR.apk");
        File.WriteAllText(apk, "fake apk 1");
        return new Setup(quake, target, apk, QuestFiles.Plan(target, quake, []));
    }

    static void NothingRemoved(FakeAdb adb)
    {
        foreach (var c in adb.Commands)
        {
            var words = c.Split(' ');
            True(!words.Any(w => w is "rm" or "rmdir" or "uninstall" or "clear" or "-rf"), $"a command that removes something: {c}");
        }
    }

    public static IEnumerable<(string Name, Action Body)> All(Func<string, string> dir) =>
    [
        ("quest: adb devices -l read as none, unauthorized, offline, several, one, --serial", () =>
        {
            const string head = "List of devices attached\n";
            Eq(QuestDeviceState.None, QuestDeviceStatus.Parse(head + "\n").State, "none");
            Eq(QuestDeviceState.None, QuestDeviceStatus.Parse("* daemon not running; starting now at tcp:5037\n* daemon started successfully\n" + head).State, "daemon start lines");
            var un = QuestDeviceStatus.Parse(head + "1WMHH0001\tunauthorized usb:1-1 transport_id:3\n");
            Eq(QuestDeviceState.Unauthorized, un.State, "unauthorized");
            True(un.Message.Contains("Allow USB debugging"), "unauthorized: says to accept the prompt in the headset");
            Eq(QuestDeviceState.Offline, QuestDeviceStatus.Parse(head + "1WMHH0001 offline\n").State, "offline");
            var two = head + "1WMHH0001 device usb:1 model:Quest_3 transport_id:1\nemulator-5554 device product:sdk model:sdk_phone transport_id:2\n";
            var several = QuestDeviceStatus.Parse(two);
            Eq(QuestDeviceState.Several, several.State, "several");
            True(several.Message.Contains("Quest 3") && several.Message.Contains("Unplug"), "several: names them, says to unplug the others");
            var one = QuestDeviceStatus.Parse(two, "1WMHH0001");
            Eq(QuestDeviceState.Ready, one.State, "--serial picks one");
            Eq("Quest 3", one.Device!.Model, "the model");
            True(QuestDeviceStatus.Parse(head).Message.Contains("USB cable"), "none: says to plug it in");
        }),

        ("quest: adb found (path, cache, PATH, SDK) and Google's platform-tools fetched into the cache", () =>
        {
            var cache = dir("quest-adbcache");
            var sdk = dir("quest-sdk");
            Eq<string?>(null, AdbLocator.Find(null, cache, _ => null), "nothing anywhere");
            Directory.CreateDirectory(Path.Combine(sdk, "platform-tools"));
            File.WriteAllText(Path.Combine(sdk, "platform-tools", "adb.exe"), "sdk adb");
            Eq<string?>(Path.Combine(sdk, "platform-tools", "adb.exe"), AdbLocator.Find(null, cache, v => v == "ANDROID_HOME" ? sdk : null), "the SDK's");
            Eq<string?>(Path.Combine(sdk, "platform-tools", "adb.exe"), AdbLocator.Find(null, cache, v => v == "PATH" ? @"C:\nowhere;" + Path.Combine(sdk, "platform-tools") : null), "PATH's");

            var zip = new MemoryStream();
            using (var z = new ZipArchive(zip, ZipArchiveMode.Create, leaveOpen: true))
            {
                foreach (var f in new[] { "adb.exe", "AdbWinApi.dll", "AdbWinUsbApi.dll", "fastboot.exe" })
                {
                    using var s = z.CreateEntry("platform-tools/" + f).Open();
                    s.Write(Encoding.UTF8.GetBytes("google " + f));
                }
            }
            using var server = new LocalHttpServer();
            server.Serve("/platform-tools-latest-windows.zip", zip.ToArray());
            using var http = Downloader.CreateClient();
            var adb = AdbLocator.FetchAsync(http, cache, null, CancellationToken.None, server.Url("/platform-tools-latest-windows.zip").ToString()).GetAwaiter().GetResult();
            Eq(AdbLocator.CachedAdb(cache), adb, "unpacked into the cache");
            Eq("google adb.exe", File.ReadAllText(adb), "adb.exe");
            True(File.Exists(Path.Combine(cache, "platform-tools", "AdbWinUsbApi.dll")), "its DLLs");
            True(!File.Exists(Path.Combine(cache, "platform-tools", "fastboot.exe")), "only what adb needs");
            Eq<string?>(adb, AdbLocator.Find(null, cache, v => v == "ANDROID_HOME" ? sdk : null), "the cache's first next time");
            True(AdbLocator.PlatformToolsUrl.StartsWith("https://dl.google.com/android/repository/", StringComparison.Ordinal), "Google's official address");

            var bad = new MemoryStream();
            using (var z = new ZipArchive(bad, ZipArchiveMode.Create, leaveOpen: true))
            {
                z.CreateEntry("readme.txt");
            }
            server.Serve("/junk.zip", bad.ToArray());
            True(Throws<AggregateException>(() => AdbLocator.FetchAsync(http, dir("quest-adbcache2"), null, CancellationToken.None, server.Url("/junk.zip").ToString()).Wait(), "a zip without adb")
                .InnerException is InstallException, "a zip without adb.exe refused");
        }),

        ("quest: the files for the headset (game files from the install, id1 lower case, music it plays, relit maps, Armagon, HD textures; no PC programs)", () =>
        {
            var s = MakeSetup(dir, "quest-plan");
            var paths = s.Files.Select(f => f.DevicePath).ToHashSet(StringComparer.Ordinal);
            // (music: bgmusic.c asks for music/trackNN.<ext> through the search path, lower case, and the Quest build decodes WAV only;
            // relit maps: VR_ModelFile's relit_custom/<game>/maps/<map>.bsp in a game folder, <game> as the headset names it)
            foreach (var p in new[] { "quakevr/progs.dat", "quakevr/default.cfg", "quakevr/maps/vrhub.bsp", "id1/pak0.pak", "id1/pak1.pak",
                         "id1/music/track03.wav", "hipnotic/pak0.pak", "id1/textures/wall1.png", "hipnotic/textures/h_wall.png",
                         "quakevr/relit_custom/id1/maps/e1m1.bsp", "quakevr/relit_custom/id1/maps/e1m1.lit", "quakevr/relit_custom/id1/maps/e1m1.relight" })
            {
                True(paths.Contains(p), $"{p} goes to the Quest");
            }
            foreach (var p in new[] { "id1/music/track02.ogg", "quakevr/relit_custom/ID1/maps/e1m1.bsp", "quakevr/relit_custom/id1/maps/e1m3.bsp",
                         "quakevr/relit_custom/rogue/maps/r1m1.bsp", "quakevr/relit_custom/_stale/maps/e1m2.bsp" })
            {
                True(!paths.Contains(p), $"{p}: not for the Quest");
            }
            Eq(4, paths.Count(p => p.StartsWith("quakevr/relit_custom/", StringComparison.Ordinal) || p.StartsWith("id1/music/", StringComparison.Ordinal)),
                "only e1m1's relit copy and the WAV track");
            Eq(Path.Combine(s.Install, "quakevr", "relit_custom", "ID1", "maps", "e1m1.lit"),
                s.Files.Single(f => f.DevicePath == "quakevr/relit_custom/id1/maps/e1m1.lit").Source, "ID1's relit copy as id1's");
            foreach (var p in paths)
            {
                True(!p.EndsWith(".exe") && !p.EndsWith(".dll") && !p.EndsWith(".pdb") && !p.StartsWith("quakevr/tools/") && p.Contains('/'),
                    $"{p}: a PC program or tool, not for the Quest");
                True(!p.StartsWith("rogue/"), $"{p}: Dissolution is not owned here");
            }
            Eq(Path.Combine(s.Quake, "id1", "PAK1.PAK"), s.Files.Single(f => f.DevicePath == "id1/pak1.pak").Source, "PAK1.PAK as id1/pak1.pak");
            Throws<InstallException>(() => QuestFiles.Plan(dir("quest-noinstall"), s.Quake, []), "no install record");
        }),

        ("quest: the whole procedure on a fake headset (PC install, then the Quest from it; the PC install unchanged, the player's files kept)", () =>
        {
            var s = MakeSetup(dir, "quest-full");
            var adb = new FakeAdb(dir("quest-full-device"));
            var save = Path.Combine(adb.DeviceFiles, "quakevr", "s0.sav");
            Directory.CreateDirectory(Path.GetDirectoryName(save)!);
            File.WriteAllText(save, "my save");
            File.WriteAllText(Path.Combine(adb.DeviceFiles, "quakevr", "config.cfg"), "my config");
            var before = Snapshot(s.Install) + Snapshot(s.Quake);
            var log = new List<string>();
            var result = new QuestInstaller(adb).Install(new QuestPlan { ApkPath = s.Apk, Files = s.Files },
                new SyncProgress<InstallProgress>(p => { lock (log) { if (p.Log is not null) log.Add(p.Log); } }), CancellationToken.None);
            Eq(1, adb.Installs, "the APK installed once");
            True(adb.Commands.Contains($"-s {FakeAdb.Serial} install -r {s.Apk}"), "install -r: keeps the app's files");
            Eq(s.Files.Count, result.Pushed, "every file copied");
            Eq(0, result.Skipped, "nothing there before");
            Eq(s.Files.Count, adb.TarFiles, "one tar stream with every file");
            foreach (var f in s.Files)
            {
                var onDevice = Path.Combine(adb.DeviceFiles, f.DevicePath.Replace('/', '\\'));
                True(File.Exists(onDevice) && File.ReadAllBytes(onDevice).SequenceEqual(File.ReadAllBytes(f.Source)), $"{f.DevicePath} on the headset as on the PC");
            }
            True(adb.Chmodded.Contains("quakevr/progs.dat") && adb.Chmodded.Contains("quakevr/maps"), "made readable by the game (files and folders)");
            True(!adb.Chmodded.Contains("quakevr/s0.sav"), "the player's save not touched");
            Eq("my save", File.ReadAllText(save), "the player's save kept");
            Eq("my config", File.ReadAllText(Path.Combine(adb.DeviceFiles, "quakevr", "config.cfg")), "the player's config kept");
            Eq(before, Snapshot(s.Install) + Snapshot(s.Quake), "the PC install and Quake unchanged");
            True(log.Any(l => l.Contains("Unknown Sources")), "says where to start it");
            // The Done page's Quest card (and the Install page's): installed, the files copied, where to start it.
            var (ok, title, text) = QuestTarget.Outcome(result, null);
            True(ok && title == $"Installed on your {result.Device}", $"the Done page's title: {title}");
            True(text.StartsWith("The app was installed") && text.Contains($"{s.Files.Count} files copied (") && text.Contains("0 already there") &&
                 text.EndsWith(QuestTarget.StartNote), $"the Done page's text: {text}");
            // The relit maps on the headset where the Quest build looks (VR_ModelFile: relit_custom/<game>/maps/<map>.bsp in a
            // game folder, here quakevr; the game folder as the headset names it).
            foreach (var p in new[] { "quakevr/relit_custom/id1/maps/e1m1.bsp", "quakevr/relit_custom/id1/maps/e1m1.lit", "quakevr/relit_custom/id1/maps/e1m1.relight" })
            {
                True(File.Exists(Path.Combine(adb.DeviceFiles, p.Replace('/', '\\'))), $"{p} on the headset");
            }
            NothingRemoved(adb);
        }),

        ("quest: installing again skips unchanged files and the same APK; a changed file is copied again", () =>
        {
            var s = MakeSetup(dir, "quest-again");
            var adb = new FakeAdb(dir("quest-again-device"));
            var installer = new QuestInstaller(adb);
            installer.Install(new QuestPlan { ApkPath = s.Apk, Files = s.Files }, null, CancellationToken.None);
            var again = installer.Install(new QuestPlan { ApkPath = s.Apk, Files = s.Files }, null, CancellationToken.None);
            Eq(0, again.Pushed, "nothing copied again");
            Eq(s.Files.Count, again.Skipped, "everything skipped");
            True(!again.ApkInstalled, "the same APK: not installed again");
            var againText = QuestTarget.Outcome(again, null).Text;
            True(againText.StartsWith("The app was already up to date; 0 files copied, ") && againText.Contains($"{s.Files.Count} already there"),
                $"the Done page after nothing changed: {againText}");
            Eq(1, adb.Installs, "one install in all");
            Eq(s.Files.Count, adb.TarFiles, "no tar stream the second time");

            // A new release: progs.dat changes at the same size (the MD5 tells), the APK changes.
            var progs = Path.Combine(s.Install, "quakevr", "progs.dat");
            var text = File.ReadAllText(progs);
            File.WriteAllText(progs, text[..^1] + (text[^1] == 'x' ? 'y' : 'x'));
            File.WriteAllText(s.Apk, "fake apk 2");
            var third = installer.Install(new QuestPlan { ApkPath = s.Apk, Files = QuestFiles.Plan(s.Install, s.Quake, []) }, null, CancellationToken.None);
            Eq(1, third.Pushed, "only the changed file");
            True(third.ApkInstalled, "the new APK installed");
            Eq(File.ReadAllText(progs), File.ReadAllText(Path.Combine(adb.DeviceFiles, "quakevr", "progs.dat")), "the new progs.dat on the headset");
            NothingRemoved(adb);
        }),

        ("quest: no device, unauthorized, several: a clear message and nothing sent to the headset", () =>
        {
            var s = MakeSetup(dir, "quest-nodev");
            foreach (var (devices, state) in new[]
                     {
                         ("List of devices attached\n\n", QuestDeviceState.None),
                         ($"List of devices attached\n{FakeAdb.Serial}\tunauthorized usb:1-1 transport_id:4\n", QuestDeviceState.Unauthorized),
                         ($"List of devices attached\n{FakeAdb.Serial} device model:Quest_3\nR58N12345 device model:SM_G991B\n", QuestDeviceState.Several),
                     })
            {
                var adb = new FakeAdb(dir("quest-nodev-device-" + state)) { Devices = devices };
                var e = Throws<QuestDeviceException>(() => new QuestInstaller(adb).Install(new QuestPlan { ApkPath = s.Apk, Files = s.Files }, null, CancellationToken.None), state.ToString());
                Eq(state, e.Status.State, "the state");
                Eq(1, adb.Commands.Count, $"{state}: only adb devices");
                Eq(0, Directory.EnumerateFileSystemEntries(adb.DeviceFiles).Count(), $"{state}: nothing on the headset");
                // The Done page's Quest card: not installed, why, and what to do.
                var (ok, title, text) = QuestTarget.Outcome(null, e.Message);
                True(!ok && title.StartsWith("Not installed") && text.StartsWith(e.Message.TrimEnd()) && text.Contains("Run Setup again"), $"{state}: the Done page says why: {text}");
            }
            True(QuestTarget.Outcome(null, "").Text.StartsWith("The Quest was skipped."), "skipped without a try: says so");
            // Several, one chosen (--serial): installs on that one.
            var chosen = new FakeAdb(dir("quest-nodev-chosen")) { Devices = $"List of devices attached\n{FakeAdb.Serial} device model:Quest_3\nR58N12345 device model:SM_G991B\n" };
            Eq(s.Files.Count, new QuestInstaller(chosen).Install(new QuestPlan { ApkPath = s.Apk, Files = s.Files, Serial = FakeAdb.Serial }, null, CancellationToken.None).Pushed, "--serial");
        }),

        ("quest: a full headset refused before copying; one that fills up mid-copy reported", () =>
        {
            var s = MakeSetup(dir, "quest-full-disk");
            var adb = new FakeAdb(dir("quest-disk-device")) { FreeBytes = 10L << 20 };
            var e = Throws<InstallException>(() => new QuestInstaller(adb).Install(new QuestPlan { ApkPath = s.Apk, Files = s.Files }, null, CancellationToken.None), "full");
            True(e.Message.Contains("Not enough space on the Quest"), $"says so: {e.Message}");
            Eq(0, adb.TarFiles, "nothing copied");

            var filling = new FakeAdb(dir("quest-disk-filling")) { WriteQuota = 40 };
            var e2 = Throws<InstallException>(() => new QuestInstaller(filling).Install(new QuestPlan { ApkPath = s.Apk, Files = s.Files }, null, CancellationToken.None), "fills up");
            True(e2.Message.Contains("storage is full"), $"says the storage is full: {e2.Message}");
            NothingRemoved(filling);
        }),

        ("quest: a failed APK install (another signature) says what to do; no files copied, never uninstalled", () =>
        {
            var s = MakeSetup(dir, "quest-apkfail");
            var adb = new FakeAdb(dir("quest-apkfail-device")) { InstallFailure = "INSTALL_FAILED_UPDATE_INCOMPATIBLE: Package com.vittorioromeo.quakevr signatures do not match newer version" };
            var e = Throws<InstallException>(() => new QuestInstaller(adb).Install(new QuestPlan { ApkPath = s.Apk, Files = s.Files }, null, CancellationToken.None), "incompatible");
            True(e.Message.Contains("signed differently") && e.Message.Contains("saves"), $"explains, warns about saves: {e.Message}");
            Eq(0, adb.TarFiles, "no files copied");
            NothingRemoved(adb);
            var other = new FakeAdb(dir("quest-apkfail-other")) { InstallFailure = "INSTALL_FAILED_NO_MATCHING_ABIS" };
            True(Throws<InstallException>(() => new QuestInstaller(other).Install(new QuestPlan { ApkPath = s.Apk, Files = s.Files }, null, CancellationToken.None), "abis")
                .Message.Contains("INSTALL_FAILED_NO_MATCHING_ABIS"), "another failure: adb's words");
        }),

        ("quest: a Quest-only install fills a staging folder with the same install steps", () =>
        {
            // Quest only: the window installs into its staging folder (no shortcuts, no Apps & Features entry), then
            // copies from there; the result on the headset is the same as from a PC install.
            var s = MakeSetup(dir, "quest-only");
            var staging = QuestTarget.StagingDir(dir("quest-only-local"), null);
            True(staging.EndsWith("quest-files", StringComparison.Ordinal), "under the installer's own folder");
            new InstallEngine().Install(new InstallPlan
            {
                PackagePath = Fixtures.MakePackage(dir("quest-only-pkg"), "1.2.0 (2026-11-22 aaaaaaa1)"), TargetDir = staging, QuakeDir = s.Quake,
            }, null, CancellationToken.None);
            var files = QuestFiles.Plan(staging, s.Quake, []);
            var adb = new FakeAdb(dir("quest-only-device"));
            Eq(files.Count, new QuestInstaller(adb).Install(new QuestPlan { ApkPath = s.Apk, Files = files }, null, CancellationToken.None).Pushed, "copied from staging");
            True(File.Exists(Path.Combine(adb.DeviceFiles, "quakevr", "progs.dat")) && File.Exists(Path.Combine(adb.DeviceFiles, "id1", "pak1.pak")), "the game and id1 on the headset");
            Eq(0, Directory.EnumerateFiles(staging, "*.lnk", SearchOption.AllDirectories).Count(), "no shortcuts");
        }),
    ];

    static string Snapshot(string dir) => string.Join("|", Directory.EnumerateFileSystemEntries(dir, "*", SearchOption.AllDirectories).Order()
        .Select(p => File.Exists(p) ? $"{p}:{new FileInfo(p).Length}:{File.GetLastWriteTimeUtc(p).Ticks}" : p));
}
