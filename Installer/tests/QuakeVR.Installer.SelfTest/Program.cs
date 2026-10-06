using System.Security.Cryptography;
using QuakeVR.Installer.Core;
using QuakeVR.Installer.Core.Assets;
using QuakeVR.Installer.Core.Detection;
using QuakeVR.Installer.Core.Packaging;
using QuakeVR.Installer.Core.Platform;
using QuakeVR.Installer.Core.Shortcuts;
using QuakeVR.Installer.SelfTest;

// The installer core's tests. Usage: SelfTest [scratch root] [name filter]. Every test works in its own new folder
// under the scratch root (nothing is cleaned up: the folders are small) and never touches the real registry,
// desktop or Start menu: the machine is a MemorySystemProbe, shortcuts go into test folders.
var scratchRoot = args.Length > 0 ? args[0] : Path.Combine(Path.GetTempPath(), "qvr-installer-selftest");
var filter = args.Length > 1 ? args[1] : null;
var run = Path.GetFullPath(Path.Combine(scratchRoot, DateTime.Now.ToString("yyyyMMdd-HHmmss-fff")));
Directory.CreateDirectory(run);

string Dir(string name) => Directory.CreateDirectory(Path.Combine(run, name)).FullName;

void Eq<T>(T expected, T actual, string what)
{
    if (!EqualityComparer<T>.Default.Equals(expected, actual))
    {
        throw new Exception($"{what}: expected <{expected}>, got <{actual}>");
    }
}

void True(bool condition, string what)
{
    if (!condition)
    {
        throw new Exception(what);
    }
}

T Throws<T>(Action a, string what) where T : Exception
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

var tests = new List<(string Name, Action Body)>
{
    ("vdf: libraryfolders, both formats, escapes, comments", () =>
    {
        var v = Vdf.Parse("""
            // comment
            "libraryfolders"
            {
                "0" { "path" "C:\\Program Files (x86)\\Steam" "apps" { "2310" "123" } }
                "1" "D:\\Old \"Lib\""
                [$WIN32] "x" "y"
            }
            """);
        var lf = v.GetObject("LIBRARYFOLDERS")!;
        Eq(@"C:\Program Files (x86)\Steam", lf.GetObject("0")!.GetString("path"), "path");
        Eq("123", lf.GetObject("0")!.GetObject("apps")!.GetString("2310"), "apps");
        Eq("D:\\Old \"Lib\"", lf.GetString("1"), "escaped");
        Eq("y", lf.GetString("x"), "conditional skipped");
        Eq(0, Vdf.Parse("\"a\" {").GetObject("a")!.Entries.Count, "truncated input");
    }),
    ("steam: libraries and app folders from a fake Steam", () =>
    {
        var steam = Dir("steam");
        var lib2 = Dir("steamlib2");
        Directory.CreateDirectory(Path.Combine(steam, "steamapps"));
        File.WriteAllText(Path.Combine(steam, "steamapps", "libraryfolders.vdf"),
            $"\"libraryfolders\" {{ \"0\" {{ \"path\" \"{steam.Replace("\\", "\\\\")}\" }} \"1\" {{ \"path\" \"{lib2.Replace("\\", "\\\\")}\" }} \"2\" {{ \"path\" \"Z:\\\\gone\" }} }}");
        Directory.CreateDirectory(Path.Combine(lib2, "steamapps", "common", "Quake"));
        File.WriteAllText(Path.Combine(lib2, "steamapps", "appmanifest_2310.acf"), "\"AppState\" { \"appid\" \"2310\" \"installdir\" \"Quake\" }");
        var probe = new MemorySystemProbe().SetValue(@"HKCU\Software\Valve\Steam", "SteamPath", steam.Replace('\\', '/').ToLowerInvariant());
        var locator = new SteamLocator(probe);
        Eq(steam, locator.FindSteamRoot(), "root (real case)");
        Eq(2, locator.FindLibraries().Count, "libraries (missing one skipped)");
        Eq(Path.Combine(lib2, "steamapps", "common", "Quake"), locator.FindApp("2310"), "Quake");
        Eq(null, locator.FindApp("250820"), "SteamVR absent");
    }),
    ("gog and epic", () =>
    {
        var gog = Dir("gog");
        Fixtures.MakeOriginal(gog);
        var epicGame = Dir("epicgame");
        Fixtures.MakeRerelease(Path.Combine(epicGame, "rerelease"));
        var manifests = Dir("epicmanifests");
        File.WriteAllText(Path.Combine(manifests, "a.item"), $$"""{ "DisplayName": "Quake", "InstallLocation": "{{epicGame.Replace("\\", "\\\\")}}" }""");
        File.WriteAllText(Path.Combine(manifests, "b.item"), """{ "DisplayName": "Fortnite", "InstallLocation": "C:\\Nope" }""");
        File.WriteAllText(Path.Combine(manifests, "c.item"), "{ broken");
        var probe = new MemorySystemProbe().SetValue(@"HKLM\SOFTWARE\WOW6432Node\GOG.com\Games\1435828198", "path", gog);
        var found = new QuakeDetector(probe, manifests).FindAll();
        Eq(2, found.Count, "installs");
        Eq(QuakeStore.Gog, found[0].Store, "gog first");
        Eq(gog, found[0].BaseDir, "gog base");
        Eq(QuakeStore.Epic, found[1].Store, "epic");
        Eq(Path.Combine(epicGame, "rerelease"), found[1].BaseDir, "epic base is its rerelease folder");
        Eq(Id1Kind.Rerelease, found[1].Rerelease.Kind, "epic rerelease");
    }),
    ("id1: original, rerelease, shareware, missing, corrupt; manual pick of id1 or rerelease", () =>
    {
        var both = Dir("both");
        Fixtures.MakeOriginal(both);
        Fixtures.MakeRerelease(Path.Combine(both, "rerelease"));
        var q = QuakeDetector.Describe(QuakeStore.Steam, "Steam", both);
        Eq(Id1Kind.Original, q.Original.Kind, "original");
        Eq(false, q.Original.KnownVersion, "tiny paks are not 1.06");
        Eq(Id1Kind.Rerelease, q.Rerelease.Kind, "rerelease");
        Eq(both, q.BaseDir, "original preferred as base");
        Eq(both, QuakeDetector.DescribeManual(Path.Combine(both, "id1")).InstallDir, "picked id1");
        Eq(both, QuakeDetector.DescribeManual(Path.Combine(both, "rerelease")).InstallDir, "picked rerelease");
        var sw = Dir("shareware");
        Directory.CreateDirectory(Path.Combine(sw, "id1"));
        PakFile.Write(Path.Combine(sw, "id1", "pak0.pak"), [("maps/start.bsp", Fixtures.Bsp())]);
        Eq(Id1Kind.Shareware, QuakeData.Inspect(sw).Kind, "shareware");
        Eq(false, QuakeDetector.Describe(QuakeStore.Manual, "x", sw).Playable, "shareware not playable");
        Eq(Id1Kind.Missing, QuakeData.Inspect(Dir("empty")).Kind, "missing");
        var bad = Dir("corrupt");
        Directory.CreateDirectory(Path.Combine(bad, "id1"));
        File.WriteAllText(Path.Combine(bad, "id1", "pak0.pak"), "not a pak at all");
        Eq(Id1Kind.Corrupt, QuakeData.Inspect(bad).Kind, "corrupt");
    }),
    ("resources: the engine's structural checks", () =>
    {
        True(ResourceValidator.IsValid(Fixtures.Mdl(), "progs/x.mdl"), "mdl");
        True(ResourceValidator.IsValid(Fixtures.Wav(), "sound/x.wav"), "wav");
        True(ResourceValidator.IsValid(Fixtures.Spr(), "progs/x.spr"), "spr");
        True(ResourceValidator.IsValid(Fixtures.Bsp(), "maps/x.bsp"), "bsp");
        True(!ResourceValidator.IsValid(Fixtures.Mdl()[..120], "progs/x.mdl"), "truncated mdl");
        True(!ResourceValidator.IsValid(Fixtures.Wav()[..44], "sound/x.wav"), "truncated wav");
        True(!ResourceValidator.IsValid(Fixtures.Bsp()[..150], "maps/x.bsp"), "truncated bsp");
        True(!ResourceValidator.IsValid(Fixtures.Mdl(), "maps/x.bsp"), "mdl named bsp");
    }),
    ("packs: ready, incomplete, corrupt, textures-only, loose files", () =>
    {
        Eq(100, PackLists.Resources("hipnotic").Count, "hipnotic list from the engine's .inc");
        var root = Dir("packs");
        Fixtures.MakePack(root, "hipnotic");
        Eq(PackStatus.Ready, PackInspector.Inspect("hipnotic", PackLists.Resources("hipnotic"), [root]).Status, "ready");
        var partial = Dir("packs-partial");
        var missing = PackLists.Resources("rogue")[5];
        Fixtures.MakePack(partial, "rogue", leaveOut: missing);
        var r = PackInspector.Inspect("rogue", PackLists.Resources("rogue"), [partial]);
        Eq(PackStatus.Incomplete, r.Status, "incomplete");
        True(r.Detail.Contains(missing), "names the missing file");
        // ... completed by the loose file in a second base dir
        var loose = Dir("packs-loose");
        var path = Path.Combine(loose, "rogue", missing.Replace('/', '\\'));
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        File.WriteAllBytes(path, Fixtures.For(missing));
        Eq(PackStatus.Ready, PackInspector.Inspect("rogue", PackLists.Resources("rogue"), [partial, loose]).Status, "loose file completes it");
        var textures = Dir("packs-textures");
        Directory.CreateDirectory(Path.Combine(textures, "hipnotic", "textures"));
        File.WriteAllText(Path.Combine(textures, "hipnotic", "textures", "a.png"), "x");
        Eq(PackStatus.Missing, PackInspector.Inspect("hipnotic", PackLists.Resources("hipnotic"), [textures]).Status, "textures only");
        var corrupt = Dir("packs-corrupt");
        Directory.CreateDirectory(Path.Combine(corrupt, "hipnotic"));
        File.WriteAllText(Path.Combine(corrupt, "hipnotic", "pak0.pak"), "PACK garbage");
        Eq(PackStatus.Incomplete, PackInspector.Inspect("hipnotic", PackLists.Resources("hipnotic"), [corrupt]).Status, "corrupt pak");
    }),
    ("expansions: base dirs for hipnotic/rogue, owned roots for dopa/mg1/mg3, mg1 not yet supported", () =>
    {
        var quake = Dir("exp-quake");
        Fixtures.MakeOriginal(quake);
        Fixtures.MakePack(quake, "hipnotic");
        var rerelease = Path.Combine(quake, "rerelease");
        Fixtures.MakeRerelease(rerelease);
        Fixtures.MakePack(rerelease, "dopa");
        Fixtures.MakePack(rerelease, "mg1");
        // A textures-only dopa folder in the Quake VR folder (higher priority) must not hide the real one.
        var qvr = Dir("exp-qvr");
        Directory.CreateDirectory(Path.Combine(qvr, "dopa", "textures"));
        var list = ExpansionDetector.Detect([quake, qvr], []);
        var byName = list.ToDictionary(e => e.Folder);
        Eq(ExpansionState.Ready, byName["hipnotic"].State, "hipnotic");
        Eq(ExpansionState.NotFound, byName["rogue"].State, "rogue");
        Eq(ExpansionState.Ready, byName["dopa"].State, "dopa");
        Eq(rerelease, byName["dopa"].Root, "dopa from <base>\\rerelease");
        Eq(ExpansionState.DetectedNotSupported, byName["mg1"].State, "mg1");
        Eq(ExpansionState.NotFound, byName["mg3"].State, "mg3");
    }),
    ("vr: active runtime, VD suggestion, VC++ runtime", () =>
    {
        var pf = Dir("pf");
        Directory.CreateDirectory(Path.Combine(pf, "Virtual Desktop Streamer"));
        File.WriteAllText(Path.Combine(pf, "Virtual Desktop Streamer", "VirtualDesktop.Streamer.exe"), "");
        var probe = new MemorySystemProbe()
            .SetFolder(KnownFolder.ProgramFiles, pf)
            .SetValue(VrDetector.OpenXrKey, "ActiveRuntime", @"C:\Steam\steamapps\common\SteamVR\steamxr_win64.json")
            .SetValue(VrDetector.AvailableRuntimesKey, @"C:\Steam\steamapps\common\SteamVR\steamxr_win64.json", 0)
            .SetValue(VrDetector.AvailableRuntimesKey, @"C:\x\virtualdesktop-openxr.json", 1);
        var vr = VrDetector.Detect(probe, new SteamLocator(probe));
        Eq(RuntimeKind.SteamVR, vr.Active!.Kind, "active");
        True(vr.VirtualDesktopInstalled && vr.SuggestVdxr, "suggest VDXR");
        Eq(false, vr.Available.Single(r => r.Kind == RuntimeKind.VirtualDesktop).Enabled, "disabled runtime");
        probe.SetValue(VrDetector.OpenXrKey, "ActiveRuntime", @"C:\x\virtualdesktop-openxr.json");
        Eq(false, VrDetector.Detect(probe, new SteamLocator(probe)).SuggestVdxr, "VDXR already active");
        Eq(false, VrDetector.Detect(new MemorySystemProbe(), new SteamLocator(new MemorySystemProbe())).VirtualDesktopInstalled, "nothing");
        const string vc = @"HKLM\SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64";
        Eq(false, VcRuntimeDetector.Detect(new MemorySystemProbe().SetValue(vc, "Major", 14).SetValue(vc, "Minor", 40)).Ok, "14.40 too old");
        Eq(true, VcRuntimeDetector.Detect(new MemorySystemProbe().SetValue(vc, "Major", 14).SetValue(vc, "Minor", 44)).Ok, "14.44 ok");
        Eq(false, VcRuntimeDetector.Detect(new MemorySystemProbe()).Ok, "absent");
    }),
    ("launch arguments and shortcut plan", () =>
    {
        Eq("-basedir \"C:\\Games\\Quake\" -basedir \"C:\\Users\\p\\QuakeVR\" -game quakevr",
            LaunchCommand.Arguments(@"C:\Games\Quake\", @"C:\Users\p\QuakeVR", LaunchVariant.Vr), "vr (trailing backslash dropped)");
        True(LaunchCommand.Arguments("a", "b", LaunchVariant.Flat).EndsWith(" +vr_enabled 0"), "flat");
        True(LaunchCommand.Arguments("a", "b", LaunchVariant.Log).EndsWith(" -condebug"), "log");
        var all = ShortcutPlanner.Plan(new ShortcutOptions { DesktopDir = @"T:\Desk", StartMenuDir = @"T:\Prog" }, @"Q:\Quake", @"T:\QVR");
        Eq(5, all.Count, "desktop + 4 in the Start menu");
        Eq(@"T:\Prog\Quake VR Unleashed\Quake VR Unleashed (flat screen).lnk", all[2].LinkPath, "flat in the Start menu");
        Eq(@"T:\QVR", all[0].WorkingDirectory, "start in the Quake VR folder");
        var desktopOnly = ShortcutPlanner.Plan(new ShortcutOptions { DesktopDir = @"T:\Desk", StartMenu = false, Log = false }, @"Q:\Quake", @"T:\QVR");
        Eq(2, desktopOnly.Count, "desktop: VR + flat");
        Eq(0, ShortcutPlanner.Plan(new ShortcutOptions { Desktop = false, StartMenu = false }, "q", "v").Count, "none");
    }),
    (".lnk round trip through IShellLink", () =>
    {
        var dir = Dir("lnk");
        var spec = new ShortcutSpec(Path.Combine(dir, "Quake VR.lnk"), @"C:\QVR\ironwail.exe",
            LaunchCommand.Arguments(@"C:\Quake", @"C:\QVR", LaunchVariant.Vr), @"C:\QVR", "Play Quake VR", @"C:\QVR\ironwail.exe");
        ShellLink.Save(spec);
        var back = ShellLink.Load(spec.LinkPath);
        Eq(spec.TargetPath, back.TargetPath, "target");
        Eq(spec.Arguments, back.Arguments, "arguments");
        Eq(spec.WorkingDirectory, back.WorkingDirectory, "start in");
        Eq(spec.Description, back.Description, "description");
        Eq(spec.IconPath, back.IconPath, "icon");
    }),
    ("manifest: round trip and unsafe paths refused", () =>
    {
        var pkg = Fixtures.MakePackage(Dir("manifest-pkg"), "v1");
        var m = PackageManifest.Parse(File.ReadAllText(Path.Combine(pkg, PackageManifest.FileName)));
        Eq(11, m.Files.Count, "files");
        Eq("v1", m.Version, "version");
        True(m.Files.Any(f => f.Path == "quakevr/tools/ericw-tools/light.exe"), "forward slashes");
        foreach (var bad in new[] { "../evil.dll", "C:/Windows/evil.dll", "/abs", "a//b", "a/./b", "a\\..\\..\\b" })
        {
            var json = m.ToJson().Replace("\"ironwail.exe\"", JsonString(bad));
            Throws<InvalidDataException>(() => PackageManifest.Parse(json), $"unsafe path {bad}");
        }
        Throws<InvalidDataException>(() => PathUtil.SafeCombine(@"C:\QVR", "..\\x"), "SafeCombine");
        Eq(@"C:\QVR\quakevr\x.cfg", PathUtil.SafeCombine(@"C:\QVR", "quakevr/x.cfg"), "SafeCombine ok");
    }),
    ("install target rules", () =>
    {
        var probe = new MemorySystemProbe().SetFolder(KnownFolder.ProgramFiles, @"C:\Program Files");
        True(InstallEngine.ValidateTarget(@"C:\Games\Quake\QuakeVR", @"C:\Games\Quake", probe)!.Contains("outside"), "inside Quake");
        True(InstallEngine.ValidateTarget(@"C:\Games", @"C:\Games\Quake", probe)!.Contains("outside"), "parent of Quake");
        True(InstallEngine.ValidateTarget(@"C:\Program Files\QuakeVR", @"D:\Quake", probe)!.Contains("Program Files"), "Program Files");
        True(InstallEngine.ValidateTarget(@"C:\", @"D:\Quake", probe) is not null, "drive root");
        var busy = Dir("busy");
        File.WriteAllText(Path.Combine(busy, "x.txt"), "x");
        True(InstallEngine.ValidateTarget(busy, @"D:\Quake", probe)!.Contains("already has files"), "non-empty");
        Eq(null, InstallEngine.ValidateTarget(Path.Combine(run, "fresh"), @"D:\Quake", probe), "new folder ok");
    }),
    ("install, verify, update, uninstall: end to end from a zip", () =>
    {
        var quake = Dir("e2e-quake");
        Fixtures.MakeOriginal(quake);
        var quakeBefore = Snapshot(quake);
        var zip = Fixtures.Zip(Fixtures.MakePackage(Dir("e2e-pkg1"), "v1"), Path.Combine(run, "QuakeVR-v1.zip"), "QuakeVR");
        var target = Path.Combine(run, "e2e-QuakeVR");
        var shortcuts = Dir("e2e-shortcuts");
        var options = new ShortcutOptions { DesktopDir = Path.Combine(shortcuts, "Desktop"), StartMenuDir = Path.Combine(shortcuts, "Programs") };
        var logs = new List<string>();
        var progress = new SyncProgress<InstallProgress>(p => { if (p.Log is not null) { logs.Add(p.Log); } });
        var record = new InstallEngine().Install(new InstallPlan
        {
            PackagePath = zip, TargetDir = target, QuakeDir = quake, Shortcuts = options, RelightOnFirstRun = true,
        }, progress, CancellationToken.None);
        Eq(11, record.Files.Count, "files installed");
        Eq(5, record.Shortcuts.Count, "shortcuts");
        True(record.RelightPending, "relight pending");
        True(File.Exists(Path.Combine(target, "quakevr", "progs.dat")), "progs.dat");
        True(!Directory.EnumerateFiles(target, "*.qvrnew", SearchOption.AllDirectories).Any(), "no staging leftovers");
        var lnk = ShellLink.Load(Path.Combine(shortcuts, "Desktop", "Quake VR Unleashed.lnk"));
        Eq(Path.Combine(target, "ironwail.exe"), lnk.TargetPath, "shortcut target");
        Eq(LaunchCommand.Arguments(quake, target, LaunchVariant.Vr), lnk.Arguments, "shortcut args");
        Eq(0, Uninstaller.Verify(target).Count, "verify clean");

        // The player plays: config, a save, a changed shipped file.
        File.WriteAllText(Path.Combine(target, "quakevr", "ironwail.cfg"), "player config");
        File.WriteAllText(Path.Combine(target, "quakevr", "s0.sav"), "save");
        File.AppendAllText(Path.Combine(target, "quakevr", "vr_defaults.cfg"), "\nvr_default fov 110");
        Eq(1, Uninstaller.Verify(target).Count, "verify sees the changed file");

        // Update: v2 drops maps/vrhub.bsp and SDL2.dll, changes progs.dat, no flat shortcut any more.
        var pkg2 = Fixtures.MakePackage(Dir("e2e-pkg2"), "v2", new() { ["quakevr/maps/vrhub.bsp"] = "", ["SDL2.dll"] = "", ["quakevr/newfile.txt"] = "new" });
        File.SetLastWriteTimeUtc(Path.Combine(target, "SDL2.dll"), DateTime.UtcNow); // unchanged content: still removed
        var r2 = new InstallEngine().Install(new InstallPlan
        {
            PackagePath = pkg2, TargetDir = target, QuakeDir = quake,
            Shortcuts = new ShortcutOptions { DesktopDir = options.DesktopDir, StartMenuDir = options.StartMenuDir, Flat = false },
        }, progress, CancellationToken.None);
        Eq("v2", r2.Version, "updated version");
        True(!File.Exists(Path.Combine(target, "SDL2.dll")), "dropped file removed");
        True(!File.Exists(Path.Combine(target, "quakevr", "maps", "vrhub.bsp")), "dropped map removed");
        True(File.Exists(Path.Combine(target, "quakevr", "newfile.txt")), "new file");
        Eq("player config", File.ReadAllText(Path.Combine(target, "quakevr", "ironwail.cfg")), "config kept");
        True(File.Exists(Path.Combine(target, "quakevr", "s0.sav")), "save kept");
        True(!File.Exists(Path.Combine(options.StartMenuDir!, "Quake VR Unleashed", "Quake VR Unleashed (flat screen).lnk")), "stale shortcut removed");
        Eq(4, r2.Shortcuts.Count, "shortcuts after update");
        Eq(record.InstalledAt, r2.InstalledAt, "install date kept");

        // A shortcut of the same name the player made elsewhere is never deleted.
        var foreign = Path.Combine(shortcuts, "Desktop", "Other.lnk");
        ShellLink.Save(new ShortcutSpec(foreign, @"C:\Windows\notepad.exe", "", @"C:\", "not ours"));
        True(!Uninstaller.RemoveShortcut(foreign, target), "foreign shortcut kept");

        File.AppendAllText(Path.Combine(target, "quakevr", "default.cfg"), "\n// player edit");
        var u = Uninstaller.Uninstall(target, new UninstallOptions());
        Eq(4, u.ShortcutsRemoved, "shortcuts removed");
        True(!Directory.Exists(Path.Combine(options.StartMenuDir!, "Quake VR Unleashed")), "Start menu folder removed");
        True(File.Exists(foreign), "foreign shortcut still there");
        Eq("quakevr/default.cfg", string.Join("|", u.ChangedKept), "changed shipped file kept");
        True(u.PlayerFilesLeft.Contains("quakevr/ironwail.cfg") && u.PlayerFilesLeft.Contains("quakevr/s0.sav"), "player files left");
        True(!File.Exists(Path.Combine(target, "ironwail.exe")), "engine removed");
        True(!Directory.Exists(Path.Combine(target, "quakevr", "tools")), "empty folders removed");
        Eq(quakeBefore, Snapshot(quake), "Quake folder untouched");
    }),
    ("uninstall of an untouched install removes the folder", () =>
    {
        var quake = Dir("clean-quake");
        Fixtures.MakeOriginal(quake);
        var target = Path.Combine(run, "clean-QuakeVR");
        new InstallEngine().Install(new InstallPlan { PackagePath = Fixtures.MakePackage(Dir("clean-pkg"), "v1"), TargetDir = target, QuakeDir = quake },
            null, CancellationToken.None);
        var u = Uninstaller.Uninstall(target, new UninstallOptions());
        Eq(11, u.FilesRemoved, "files");
        True(u.FolderRemoved && !Directory.Exists(target), "folder gone");
    }),
    ("a damaged package fails cleanly; a cancelled install leaves nothing", () =>
    {
        var quake = Dir("bad-quake");
        var pkg = Fixtures.MakePackage(Dir("bad-pkg"), "v1");
        File.AppendAllText(Path.Combine(pkg, "quakevr", "progs.dat"), "tampered");
        var target = Path.Combine(run, "bad-QuakeVR");
        var e = Throws<InstallException>(() => new InstallEngine().Install(new InstallPlan { PackagePath = pkg, TargetDir = target, QuakeDir = quake },
            null, CancellationToken.None), "hash mismatch");
        True(e.Message.Contains("progs.dat"), "names the file");
        True(!Directory.Exists(target), "nothing left behind");
        var noManifest = Fixtures.MakePackage(Dir("nomanifest-pkg"), "v1", manifest: false);
        Throws<InstallException>(() => new InstallEngine().Install(new InstallPlan { PackagePath = noManifest, TargetDir = target, QuakeDir = quake },
            null, CancellationToken.None), "no manifest");
        var unverified = new InstallEngine().Install(new InstallPlan { PackagePath = noManifest, TargetDir = target, QuakeDir = quake, AllowUnverified = true },
            null, CancellationToken.None);
        Eq(11, unverified.Files.Count, "unverified developer install");
        var cancelTarget = Path.Combine(run, "cancel-QuakeVR");
        using var cts = new CancellationTokenSource();
        var copied = 0;
        Throws<OperationCanceledException>(() => new InstallEngine().Install(new InstallPlan
        {
            PackagePath = Fixtures.MakePackage(Dir("cancel-pkg"), "v1"), TargetDir = cancelTarget, QuakeDir = quake,
        }, new SyncProgress<InstallProgress>(p => { if (p.Status.StartsWith("Copying") && ++copied == 3) { cts.Cancel(); } }), cts.Token), "cancel");
        True(!Directory.Exists(cancelTarget), "cancelled install left nothing");
    }),
    ("hd textures: owned packs only, the player's own files kept, kept on uninstall unless asked", () =>
    {
        var quake = Dir("tex-quake");
        var target = Path.Combine(run, "tex-QuakeVR");
        var zip = Fixtures.TextureZip(Path.Combine(run, "textures.zip"));
        Directory.CreateDirectory(Path.Combine(target, "id1", "textures"));
        File.WriteAllText(Path.Combine(target, InstallRecord.FileName), "{\"schema\":1}"); // an existing (empty) install
        File.WriteAllText(Path.Combine(target, "id1", "textures", "wall1.png"), "the player's own");
        var r = new InstallEngine().Install(new InstallPlan
        {
            PackagePath = Fixtures.MakePackage(Dir("tex-pkg"), "v1"), TargetDir = target, QuakeDir = quake, HdTexturesZip = zip, OwnedPacks = ["hipnotic"],
        }, null, CancellationToken.None);
        var tex = r.Files.Where(f => f.Component == Components.HdTextures).Select(f => f.Path).Order().ToList();
        Eq("hipnotic/textures/h_wall.png|id1/textures/e1m1/floor.png", string.Join("|", tex), "textures installed");
        Eq("the player's own", File.ReadAllText(Path.Combine(target, "id1", "textures", "wall1.png")), "own texture kept");
        var u = Uninstaller.Uninstall(target, new UninstallOptions());
        True(File.Exists(Path.Combine(target, "hipnotic", "textures", "h_wall.png")), "textures kept");
        Eq(2, InstallRecord.Load(target)!.Files.Count, "record keeps the textures");
        Uninstaller.Uninstall(target, new UninstallOptions { RemoveHdTextures = true });
        True(!File.Exists(Path.Combine(target, "hipnotic", "textures", "h_wall.png")), "textures removed when asked");
        True(File.Exists(Path.Combine(target, "id1", "textures", "wall1.png")), "player's texture still kept");
    }),
    ("downloads: mirrors, wrong file skipped, resume, pinned hash", () =>
    {
        using var server = new LocalHttpServer();
        var body = RandomNumberGenerator.GetBytes(300_000);
        var sha = Convert.ToHexStringLower(SHA256.HashData(body));
        server.Serve("good/QuakeVR.zip", body);
        server.Serve("evil/QuakeVR.zip", RandomNumberGenerator.GetBytes(300_000));
        using var http = Downloader.CreateClient();
        var dl = new Downloader(http);
        var dest = Path.Combine(Dir("dl"), "QuakeVR.zip");
        dl.DownloadAsync([server.Url("missing/QuakeVR.zip"), server.Url("evil/QuakeVR.zip"), server.Url("good/QuakeVR.zip")],
            dest, body.Length, sha, null, CancellationToken.None, attemptsPerMirror: 1).GetAwaiter().GetResult();
        True(File.ReadAllBytes(dest).AsSpan().SequenceEqual(body), "content");
        True(!File.Exists(dest + ".part"), "no part file");
        // Resume from a partial download.
        var dest2 = Path.Combine(Dir("dl2"), "QuakeVR.zip");
        File.WriteAllBytes(dest2 + ".part", body[..123_456]);
        var before = server.RangeRequests;
        dl.DownloadAsync([server.Url("good/QuakeVR.zip")], dest2, body.Length, sha, null, CancellationToken.None).GetAwaiter().GetResult();
        Eq(before + 1, server.RangeRequests, "resumed with a Range request");
        True(File.ReadAllBytes(dest2).AsSpan().SequenceEqual(body), "resumed content");
        // Nothing serves the pinned file.
        var e = Throws<InstallException>(() => dl.DownloadAsync([server.Url("evil/QuakeVR.zip")], Path.Combine(Dir("dl3"), "x.zip"),
            body.Length, sha, null, CancellationToken.None).GetAwaiter().GetResult(), "wrong hash everywhere");
        True(e.Message.Contains("SHA-256"), e.Message);
    }),
    ("release feed and GitHub releases API from a local server", () =>
    {
        using var server = new LocalHttpServer();
        var sha = new string('a', 64);
        server.Serve("latest.json", $$"""
            { "schema": 1, "version": "2026-10-06 c131f4bf",
              "package": { "file": "QuakeVR.zip", "size": 1000, "sha256": "{{sha}}", "urls": ["{{server.Url("gh/QuakeVR.zip")}}", "{{server.Url("vr/QuakeVR.zip")}}"] },
              "components": { "hdtextures": { "file": "tex.zip", "size": 5, "sha256": "{{sha}}", "urls": ["{{server.Url("gh/tex.zip")}}"] } } }
            """);
        server.Serve("bad.json", "{ not json");
        using var http = Downloader.CreateClient();
        var feed = ReleaseFeed.FetchAsync(http, [server.Url("nothing.json"), server.Url("bad.json"), server.Url("latest.json")], CancellationToken.None).GetAwaiter().GetResult();
        Eq("2026-10-06 c131f4bf", feed.Version, "version");
        Eq(2, feed.Package!.Mirrors.Count, "mirrors");
        Eq(5L, feed.Components["hdtextures"].Size, "component");
        server.Serve("api/releases/latest", $$"""
            { "tag_name": "v1", "assets": [ { "name": "QuakeVR.zip", "size": 1000, "browser_download_url": "https://example.invalid/QuakeVR.zip", "digest": "sha256:{{sha}}" },
                                            { "name": "notes.txt", "size": 3, "browser_download_url": "https://example.invalid/notes.txt" } ] }
            """);
        var (tag, assets) = GitHubReleases.LatestAsync(http, server.Url("api/releases/latest"), CancellationToken.None).GetAwaiter().GetResult();
        Eq("v1", tag, "tag");
        Eq(sha, assets[0].Sha256, "digest");
        Eq(null, assets[1].Sha256, "no digest");
    }),
    ("skin assets: pak search order, palette, WAD pictures and CONCHARS, a map's textures, WAV decoding", () =>
    {
        // A made-up Quake: two paks (pak1 overrides pak0), a palette, gfx.wad with a qpic and CONCHARS, a BSP29 with
        // one 4x2 texture, an 8-bit and a 16-bit stereo WAV. Nothing of id Software's data is used.
        var game = Dir("assets/id1");
        var palette = new byte[768];
        for (var i = 0; i < 256; ++i)
        {
            palette[i * 3] = (byte)i;
            palette[i * 3 + 1] = (byte)(255 - i);
            palette[i * 3 + 2] = 7;
        }
        var pic = new byte[8 + 6];
        BitConverter.GetBytes(3).CopyTo(pic, 0);
        BitConverter.GetBytes(2).CopyTo(pic, 4);
        for (var i = 0; i < 6; ++i)
        {
            pic[8 + i] = (byte)(i == 5 ? 255 : 10 + i);
        }
        var conchars = new byte[128 * 128];
        conchars[5] = 42;
        var picAt = 12;
        var charsAt = picAt + pic.Length;
        var table = charsAt + conchars.Length;
        var wad = new List<byte>();
        wad.AddRange("WAD2"u8.ToArray());
        wad.AddRange(BitConverter.GetBytes(2));
        wad.AddRange(BitConverter.GetBytes(table));
        wad.AddRange(pic);
        wad.AddRange(conchars);
        void Lump(int pos, int size, byte type, string name)
        {
            wad.AddRange(BitConverter.GetBytes(pos));
            wad.AddRange(BitConverter.GetBytes(size));
            wad.AddRange(BitConverter.GetBytes(size));
            wad.AddRange([type, 0, 0, 0]);
            var n = new byte[16];
            System.Text.Encoding.ASCII.GetBytes(name).CopyTo(n, 0);
            wad.AddRange(n);
        }
        Lump(picAt, pic.Length, (byte)'B', "NUM_1");
        Lump(charsAt, conchars.Length, (byte)'D', "CONCHARS");
        // BSP29: version and 15 lumps, then the texture lump: count, offset, miptex (name, 4x2, mip offsets), pixels.
        var bsp = new byte[4 + 15 * 8 + 8 + 40 + 8];
        BitConverter.GetBytes(29).CopyTo(bsp, 0);
        var texLump = 4 + 15 * 8;
        BitConverter.GetBytes(texLump).CopyTo(bsp, 4 + 2 * 8);
        BitConverter.GetBytes(bsp.Length - texLump).CopyTo(bsp, 4 + 2 * 8 + 4);
        BitConverter.GetBytes(1).CopyTo(bsp, texLump);
        BitConverter.GetBytes(8).CopyTo(bsp, texLump + 4);
        var mt = texLump + 8;
        System.Text.Encoding.ASCII.GetBytes("wbrick1_5").CopyTo(bsp, mt);
        BitConverter.GetBytes(4).CopyTo(bsp, mt + 16);
        BitConverter.GetBytes(2).CopyTo(bsp, mt + 20);
        BitConverter.GetBytes(40).CopyTo(bsp, mt + 24);
        for (var i = 0; i < 8; ++i)
        {
            bsp[mt + 40 + i] = (byte)(100 + i);
        }
        byte[] Wav(short channels, short bits, byte[] data)
        {
            var w = new List<byte>();
            w.AddRange("RIFF"u8.ToArray());
            w.AddRange(BitConverter.GetBytes(36 + data.Length));
            w.AddRange("WAVEfmt "u8.ToArray());
            w.AddRange(BitConverter.GetBytes(16));
            w.AddRange(BitConverter.GetBytes((short)1));
            w.AddRange(BitConverter.GetBytes(channels));
            w.AddRange(BitConverter.GetBytes(11025));
            w.AddRange(BitConverter.GetBytes(11025 * channels * bits / 8));
            w.AddRange(BitConverter.GetBytes((short)(channels * bits / 8)));
            w.AddRange(BitConverter.GetBytes(bits));
            w.AddRange("data"u8.ToArray());
            w.AddRange(BitConverter.GetBytes(data.Length + 100)); // Longer than the file, as in some of Quake's.
            w.AddRange(data);
            return [.. w];
        }
        PakFile.Write(Path.Combine(game, "pak0.pak"), [("gfx/palette.lmp", new byte[768]), ("maps/start.bsp", bsp), ("sound/a.wav", Wav(1, 8, [128, 255, 0]))]);
        PakFile.Write(Path.Combine(game, "pak1.pak"), [("gfx/palette.lmp", palette), ("gfx.wad", [.. wad]),
            ("sound/b.wav", Wav(2, 16, [0, 0x40, 0, 0xC0, 0xFF, 0x7F, 0xFF, 0x7F]))]);
        using var fs = QuakeFileSystem.OpenDir(game) ?? throw new Exception("paks not opened");
        Eq(2, fs.PakCount, "paks");
        Eq((byte)1, fs.Read("gfx/palette.lmp")![3], "pak1 overrides pak0");
        True(fs.Read("maps/start.bsp") is not null && fs.Read("nothing.wav") is null, "reads through both paks");
        var pics = QuakeFormats.ReadWad(fs.Read("gfx.wad")!);
        Eq(3, pics["NUM_1"].Width, "qpic width");
        Eq((byte)42, pics["conchars"].Pixels[5], "CONCHARS is raw 128x128");
        var bgra = QuakeFormats.ToBgra(pics["NUM_1"], palette, transparent255: true);
        Eq((byte)10, bgra[2], "red from the palette");
        Eq((byte)0, bgra[5 * 4 + 3], "index 255 transparent");
        var tex = QuakeFormats.ReadBspTextures(fs.Read("maps/start.bsp")!);
        Eq("wbrick1_5", tex.Single().Name, "texture name");
        Eq((byte)107, tex[0].Pixels[7], "texture pixels");
        var a = QuakeFormats.ReadWav(fs.Read("sound/a.wav")!)!;
        Eq(3, a.Samples.Length, "8-bit samples (data chunk clamped to the file)");
        True(a.Samples[0] == 0 && a.Samples[1] > 0.99f && a.Samples[2] == -1, "8-bit values");
        var b = QuakeFormats.ReadWav(fs.Read("sound/b.wav")!)!;
        Eq(2, b.Samples.Length, "stereo to mono");
        True(Math.Abs(b.Samples[0]) < 1e-6 && b.Samples[1] > 0.99f, "16-bit values, channels averaged");
        True(QuakeFormats.ReadBspTextures(new byte[200]).Count == 0 && QuakeFormats.ReadWad(new byte[5]).Count == 0 &&
             QuakeFormats.ReadWav([1, 2, 3]) is null, "junk is refused, not thrown");
        True(QuakeFileSystem.OpenDir(Dir("assets/empty")) is null, "no paks: no file system");
    }),
    ("local packages: found beside the installer, checked for a manifest, texture packs ignored", () =>
    {
        var dir = Dir("beside");
        Eq(null, LocalPackages.FindBeside(dir), "nothing there");
        File.WriteAllText(Path.Combine(dir, "QuakeVR.zip"), "not a zip");
        Eq(null, LocalPackages.FindBeside(dir), "a broken zip is not a package");
        True(LocalPackages.Inspect(Path.Combine(dir, "QuakeVR.zip")).Error is { } err && err.Contains("could not be read"), "and says why");
        var noManifest = Fixtures.MakePackage(Dir("beside-src/nomanifest"), "v0", manifest: false);
        True(LocalPackages.Inspect(noManifest).Error!.Contains("no manifest.json"), "a folder without manifest");
        var pkg = Fixtures.MakePackage(Dir("beside-src/pkg"), "v7");
        Fixtures.Zip(pkg, Path.Combine(dir, "QuakeVR-hq-textures-x.zip"));
        Eq(null, LocalPackages.FindBeside(dir), "a texture pack is not the package");
        Fixtures.Zip(pkg, Path.Combine(dir, "QuakeVR-2026-10-07.zip"), "QuakeVR");
        Eq(Path.Combine(dir, "QuakeVR-2026-10-07.zip"), LocalPackages.FindBeside(dir), "a dated package zip");
        Eq("v7", LocalPackages.Inspect(LocalPackages.FindBeside(dir)!).Manifest!.Version, "its version");
        Directory.CreateDirectory(Path.Combine(dir, "QuakeVR"));
        Eq(Path.Combine(dir, "QuakeVR-2026-10-07.zip"), LocalPackages.FindBeside(dir), "an empty QuakeVR folder is skipped");
    }),
};

static string JsonString(string s) => System.Text.Json.JsonSerializer.Serialize(s);

static string Snapshot(string dir) => string.Join("|", Directory.EnumerateFileSystemEntries(dir, "*", SearchOption.AllDirectories).Order()
    .Select(p => File.Exists(p) ? $"{p}:{new FileInfo(p).Length}:{File.GetLastWriteTimeUtc(p).Ticks}" : p));

var failed = 0;
var ran = 0;
foreach (var (name, body) in tests)
{
    if (filter is not null && !name.Contains(filter, StringComparison.OrdinalIgnoreCase))
    {
        continue;
    }
    ++ran;
    var watch = System.Diagnostics.Stopwatch.StartNew();
    try
    {
        body();
        Console.WriteLine($"PASS  {name} ({watch.ElapsedMilliseconds} ms)");
    }
    catch (Exception e)
    {
        ++failed;
        Console.WriteLine($"FAIL  {name}\n      {e.GetType().Name}: {e.Message}");
    }
}
Console.WriteLine($"{ran - failed}/{ran} passed (scratch: {run})");
return failed == 0 ? 0 : 1;
