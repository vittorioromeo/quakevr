using System.Security.Cryptography;
using QuakeVR.Installer.Core;
using QuakeVR.Installer.Core.Assets;
using QuakeVR.Installer.Core.Audio;
using QuakeVR.Installer.Core.Detection;
using QuakeVR.Installer.Core.Packaging;
using QuakeVR.Installer.Core.Platform;
using QuakeVR.Installer.Core.Prerequisites;
using QuakeVR.Installer.Core.Reliefs;
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
    ("statement: two claims, unanswered at first, Continue only with YES to both", () =>
    {
        Eq(2, AiStatement.Claims.Count, "claims");
        Eq(5, AiStatement.Paragraphs.Count, "paragraphs");
        True(AiStatement.Paragraphs[1].Contains("renaissance") && !AiStatement.Paragraphs[1].Contains("reinassance"), "spelling fixed");
        var s = new AiStatement();
        var changes = 0;
        s.Changed += () => ++changes;
        for (var i = 0; i < 2; ++i)
        {
            Eq<bool?>(null, s[i], $"claim {i + 1} starts unanswered");
        }
        Eq(2, s.Unanswered, "unanswered");
        True(!s.AllYes, "nothing answered: no Continue");
        s.Answer(0, true);
        True(!s.AllYes, "one unanswered: no Continue");
        s.Answer(1, false);
        True(!s.AllYes, "one NO: no Continue");
        Eq<bool?>(false, s[1], "NO kept");
        s.Answer(1, true);
        True(s.AllYes, "all YES: Continue");
        Eq(0, s.Unanswered, "all answered");
        s.Answer(1, false);
        True(!s.AllYes, "YES switched back to NO: no Continue");
        Eq<bool?>(false, s[1], "a set claim switches between YES and NO only");
        s.Answer(1, false);
        Eq(4, changes, "Changed fires on real changes only");
        // Every mix of the 3^2 states: Continue exactly when both are YES.
        for (var m = 0; m < 9; ++m)
        {
            var t = new AiStatement();
            var allYes = true;
            for (int i = 0, v = m; i < 2; ++i, v /= 3)
            {
                if (v % 3 != 0)
                {
                    t.Answer(i, v % 3 == 1);
                }
                allYes &= v % 3 == 1;
            }
            Eq(allYes, t.AllYes, $"mix {m}");
        }
        var text = AiStatement.Format();
        True(text.Contains(AiStatement.Subtitle) && AiStatement.Claims.All(text.Contains) && AiStatement.Paragraphs.All(text.Contains), "the console's text");
        // The wizard sets each claim's *not* in italics: split around it, and no asterisk left on screen.
        foreach (var claim in AiStatement.Claims)
        {
            var (before, emphasis, after) = AiStatement.SplitEmphasis(claim);
            Eq("not", emphasis, $"emphasis of '{claim}'");
            True(!(before + after).Contains('*') && !AiStatement.Plain(claim).Contains('*'), "no asterisk left");
            Eq(claim.Replace("*", ""), AiStatement.Plain(claim), "plain text");
        }
        Eq(("no marks", "", ""), AiStatement.SplitEmphasis("no marks"), "a text without marks stays whole");
        Eq(("a *b", "", ""), AiStatement.SplitEmphasis("a *b"), "an unclosed mark stays whole");
    }),
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
        Eq(99, PackLists.Resources("hipnotic").Count, "hipnotic list from the engine's .inc");
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
    ("expansions: readiness matches the engine (campaigns[].nativeReady, soloOnly() in Quake/vr/vr_gamedir.cpp)", () =>
    {
        using var stream = typeof(Program).Assembly.GetManifestResourceStream("vr_gamedir.cpp");
        True(stream is not null, "vr_gamedir.cpp embedded");
        var source = new StreamReader(stream!).ReadToEnd();
        var table = System.Text.RegularExpressions.Regex.Match(source, @"Campaign campaigns\[\] = \{(.*?)\n\};", System.Text.RegularExpressions.RegexOptions.Singleline);
        True(table.Success, "campaigns[] found in vr_gamedir.cpp");
        var rows = System.Text.RegularExpressions.Regex.Matches(table.Groups[1].Value, @"\{""(\w+)"",\s*""([^""]*)"",\s*""\w+"",\s*(\d+),\s*(true|false),")
            .Select(m => (Folder: m.Groups[1].Value, Title: m.Groups[2].Value, Index: int.Parse(m.Groups[3].Value), Ready: m.Groups[4].Value == "true"))
            .ToList();
        // (Dwell's and Quoth's rows are mod components, not expansions: Packaging/Dwell.cs, Quoth.cs.)
        Eq(ExpansionDetector.Campaigns.Count + 1, rows.Count(r => r.Folder is not (Components.Dwell or Components.Quoth)),
            "campaigns[] rows (id1 and the installer's expansions)");
        var solo = System.Text.RegularExpressions.Regex.Match(source, @"bool soloOnly\(int index\)\s*\{\s*return ([^;]*);", System.Text.RegularExpressions.RegexOptions.Singleline);
        True(solo.Success && System.Text.RegularExpressions.Regex.IsMatch(solo.Groups[1].Value, @"^index == \d+( \|\| index == \d+)*$"),
            $"soloOnly() is a list of indices (got <{solo.Groups[1].Value}>): teach this test its new form");
        var soloIndices = System.Text.RegularExpressions.Regex.Matches(solo.Groups[1].Value, @"\d+").Select(m => int.Parse(m.Value)).ToHashSet();
        foreach (var c in ExpansionDetector.Campaigns)
        {
            var row = rows.SingleOrDefault(r => r.Folder == c.Folder);
            True(row.Folder is not null, $"{c.Folder} in campaigns[]");
            Eq(row.Title, c.Title, $"{c.Folder} title");
            Eq(row.Ready, c.NativeReady, $"{c.Folder} NativeReady (campaigns[].nativeReady)");
            Eq(soloIndices.Contains(row.Index), c.SoloOnly, $"{c.Folder} SoloOnly (soloOnly({row.Index}))");
        }
        // The experimental campaign: the engine's qvr::ad::campaignIndex row, not nativeReady, labelled "experimental".
        var adIndex = System.Text.RegularExpressions.Regex.Match(source, @"static_assert\(qvr::ad::campaignIndex == (\d+)");
        True(adIndex.Success, "static_assert(qvr::ad::campaignIndex == N) found in vr_gamedir.cpp");
        // (Since the third-party rows, a row's checks.experimental flag gives the label: AD's row sets it.)
        True(source.Contains("c.checks.experimental ? \"experimental\""), "the engine labels a row with checks.experimental \"experimental\"");
        foreach (var c in ExpansionDetector.Campaigns)
        {
            var row = rows.Single(r => r.Folder == c.Folder);
            Eq(row.Index == int.Parse(adIndex.Groups[1].Value), c.Experimental, $"{c.Folder} Experimental (qvr::ad::campaignIndex)");
            True(!c.Experimental || !c.NativeReady, $"{c.Folder}: experimental is a campaign that is not nativeReady");
        }
        // The labels follow the flags (AD's pak2 is this test's pin: the release the game plays).
        var quake = Dir("exp-labels");
        Fixtures.MakeOriginal(quake);
        var rerelease = Path.Combine(quake, "rerelease");
        Fixtures.MakeRerelease(rerelease);
        foreach (var c in ExpansionDetector.Campaigns)
        {
            Fixtures.MakePack(c.InBaseDirs ? quake : rerelease, c.Folder);
        }
        var p2 = Path.Combine(rerelease, "ad", "pak2.pak");
        PakFile.Write(p2, [("progs/ad_patch1.mdl", Fixtures.Mdl())]);
        var sig = ArcaneDimensions.Pak2Signature(p2)!.Value;
        foreach (var e in ExpansionDetector.Detect([quake], [], new AdPak2Pin(sig.Size, sig.DirSha256)))
        {
            var c = ExpansionDetector.Campaigns.Single(x => x.Folder == e.Folder);
            Eq(c.Experimental ? "ready (experimental)" : !c.NativeReady ? "detected, not yet supported" : c.SoloOnly ? "ready (single player)" : "ready", e.Detail, $"{e.Folder} label");
            Eq(ExpansionState.Ready, e.State, $"{e.Folder} state");
        }
        // Another AD release (the real 1.80 patch 1 pin against this fake pak2; then no pak2 at all): said clearly.
        var other = ExpansionDetector.Detect([quake], []).Single(e => e.Folder == "ad");
        Eq(ExpansionState.OtherVersion, other.State, "ad: another release");
        True(other.Detail.Contains("only 1.80 patch 1 is played") && other.Detail.Contains("pak2.pak isn't"), other.Detail);
        File.Move(p2, p2 + ".moved");
        other = ExpansionDetector.Detect([quake], []).Single(e => e.Folder == "ad");
        Eq(ExpansionState.OtherVersion, other.State, "ad: no pak2");
        Eq("1.80 or older (no pak2.pak): only 1.80 patch 1 is played", other.Detail, "ad: no pak2 label");
    }),
    ("expansions: base dirs for hipnotic/rogue, owned roots for dopa/mg1/mg3, all three ready (single player)", () =>
    {
        var quake = Dir("exp-quake");
        Fixtures.MakeOriginal(quake);
        Fixtures.MakePack(quake, "hipnotic");
        var rerelease = Path.Combine(quake, "rerelease");
        Fixtures.MakeRerelease(rerelease);
        Fixtures.MakePack(rerelease, "dopa");
        Fixtures.MakePack(rerelease, "mg1");
        Fixtures.MakePack(rerelease, "mg3");
        // A textures-only dopa folder in the Quake VR folder (higher priority) must not hide the real one.
        var qvr = Dir("exp-qvr");
        Directory.CreateDirectory(Path.Combine(qvr, "dopa", "textures"));
        var list = ExpansionDetector.Detect([quake, qvr], []);
        var byName = list.ToDictionary(e => e.Folder);
        Eq(ExpansionState.Ready, byName["hipnotic"].State, "hipnotic");
        Eq(ExpansionState.NotFound, byName["rogue"].State, "rogue");
        Eq(ExpansionState.Ready, byName["dopa"].State, "dopa");
        Eq(rerelease, byName["dopa"].Root, "dopa from <base>\\rerelease");
        Eq(ExpansionState.Ready, byName["mg1"].State, "mg1");
        Eq("ready (single player)", byName["mg1"].Detail, "mg1 label");
        Eq(ExpansionState.Ready, byName["mg3"].State, "mg3");
        Eq("ready (single player)", byName["mg3"].Detail, "mg3 label");
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
    ("vc++ runtime: the registry key and the DLLs the game imports", () =>
    {
        const string vc = @"HKLM\SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64";
        var sys = Path.Combine(run, "fake-system32"); // (only a name: MemorySystemProbe's files are made up)
        MemorySystemProbe Machine(int minor, params (string Dll, Version V)[] dlls)
        {
            var p = new MemorySystemProbe().SetFolder(KnownFolder.System64, sys).SetValue(vc, "Major", 14).SetValue(vc, "Minor", minor).SetValue(vc, "Bld", 35211);
            foreach (var (dll, v) in dlls)
            {
                p.SetFileVersion(Path.Combine(sys, dll), v);
            }
            return p;
        }
        var good = new Version(14, 44, 35211, 0);
        var all = VcRuntimeDetector.Dlls.Select(d => (d, good)).ToArray();
        Eq("msvcp140.dll|vcruntime140.dll|vcruntime140_1.dll", string.Join("|", VcRuntimeDetector.Dlls), "the exe's runtime DLLs");
        var ok = VcRuntimeDetector.Detect(Machine(44, all));
        True(ok.Ok, "key and DLLs 14.44");
        Eq(new Version(14, 44, 35211), ok.Installed, "version from the key");
        var missing = VcRuntimeDetector.Detect(Machine(44, all.Where(d => d.d != "vcruntime140_1.dll").ToArray()));
        True(!missing.Ok && missing.Describe().Contains("vcruntime140_1.dll is missing"), "a DLL missing despite the key: " + missing.Describe());
        var oldDll = VcRuntimeDetector.Detect(Machine(44, [.. all.Where(d => d.d != "msvcp140.dll"), ("msvcp140.dll", new Version(14, 38, 33135, 0))]));
        True(!oldDll.Ok && oldDll.Describe().Contains("msvcp140.dll is 14.38.33135"), "an old msvcp140.dll: " + oldDll.Describe());
        True(!VcRuntimeDetector.Detect(Machine(40, all)).Ok, "key 14.40");
        var noKey = new MemorySystemProbe().SetFolder(KnownFolder.System64, sys);
        foreach (var (d, v) in all)
        {
            noKey.SetFileVersion(Path.Combine(sys, d), v);
        }
        True(VcRuntimeDetector.Detect(noKey).Ok, "no key but every DLL 14.44: ok");
        Eq(false, VcRuntimeDetector.Detect(new MemorySystemProbe().SetFolder(KnownFolder.System64, sys)).Ok, "nothing at all");
    }),
    ("vc++ redistributable: signature and version checked, exit codes, dry run, never downloaded or run for real", () =>
    {
        // Real Authenticode on files already on this PC: the .NET runtime's own DLL (Microsoft's embedded signature),
        // and a made-up one.
        var verifier = new AuthenticodeVerifier();
        var coreLib = typeof(object).Assembly.Location;
        var ms = verifier.Verify(coreLib);
        True(ms.Trusted && ms.Signer!.Contains("O=Microsoft Corporation"), $"System.Private.CoreLib.dll signed by Microsoft: {ms.Detail} {ms.Signer}");
        True(!ms.IsMicrosoft, "but by its CN=.NET certificate, not the CN=Microsoft Corporation one the redistributable has");
        True(new SignatureInfo(true, "CN=Microsoft Corporation, O=Microsoft Corporation, L=Redmond, S=Washington, C=US", "").IsMicrosoft, "the redistributable's signer");
        var fake = Path.Combine(Dir("vc-fake"), "vc_redist.x64.exe");
        File.WriteAllBytes(fake, [0x4D, 0x5A, 1, 2, 3]);
        True(!verifier.Verify(fake).Trusted, "a made-up exe is not trusted");
        True(!new SignatureInfo(true, "CN=Evil Corp, O=Evil Corp", "").IsMicrosoft, "someone else's valid signature");

        // The flow with fakes: a local server instead of aka.ms, a verifier and a runner that only record.
        using var server = new LocalHttpServer();
        server.Serve("vc_redist.x64.exe", new byte[] { 0x4D, 0x5A, 9, 9, 9 });
        var runs = new List<string>();
        var exitCode = 0;
        var trusted = true;
        var version = new Version(14, 44, 35211, 0);
        var fakeVerifier = new FakeVerifier(_ => new SignatureInfo(trusted, "CN=Microsoft Corporation, O=Microsoft Corporation, L=Redmond", trusted ? "ok" : "bad"));
        var runner = new FakeRunner((exe, a) => { runs.Add($"{Path.GetFileName(exe)} {a}"); return exitCode; });
        using var http = Downloader.CreateClient();
        var redist = new VcRedist(fakeVerifier, runner, _ => version, http);
        var dl = Dir("vc-downloads");
        var missing = new VcRuntimeInfo(null, VcRuntimeDetector.Required);
        VcRedistResult Ensure(bool dryRun = false, bool offline = false, string[]? local = null) => redist.EnsureAsync(missing, new VcRedistOptions
        {
            DryRun = dryRun, Offline = offline, LocalCopies = local ?? [], Mirrors = [server.Url("vc_redist.x64.exe")], DownloadDir = dl,
        }, null, CancellationToken.None).GetAwaiter().GetResult();

        Eq(VcRedistOutcome.AlreadyInstalled, redist.EnsureAsync(new VcRuntimeInfo(new Version(14, 51), VcRuntimeDetector.Required), new VcRedistOptions { DownloadDir = dl },
            null, CancellationToken.None).GetAwaiter().GetResult().Outcome, "nothing to do");
        var dry = Ensure(dryRun: true);
        Eq(VcRedistOutcome.DryRun, dry.Outcome, "dry run");
        True(dry.Message.Contains("/install /quiet /norestart"), "dry run says the command");
        Eq(0, server.Requests, "dry run downloads nothing");
        Eq(0, runs.Count, "dry run runs nothing");
        Eq(VcRedistOutcome.Unavailable, Ensure(offline: true).Outcome, "offline");

        var installed = Ensure();
        Eq(VcRedistOutcome.Installed, installed.Outcome, "installed");
        Eq("vc_redist.x64.exe /install /quiet /norestart", string.Join("|", runs), "run once, quiet, elevated by the runner");
        Eq(1, server.Requests, "downloaded once");
        foreach (var (code, outcome) in new[] { (3010, VcRedistOutcome.RebootRequired), (1641, VcRedistOutcome.RebootRequired), (1638, VcRedistOutcome.AlreadyInstalled),
                     (1602, VcRedistOutcome.Cancelled), (1223, VcRedistOutcome.Cancelled), (1618, VcRedistOutcome.Busy), (1603, VcRedistOutcome.Failed) })
        {
            exitCode = code;
            Eq(outcome, Ensure().Outcome, $"exit code {code}");
        }
        True(VcRedist.FromExitCode(3010, "x").RuntimeReady && !VcRedist.FromExitCode(1223, "x").RuntimeReady, "ready after a restart; not after a refusal");

        exitCode = 0;
        runs.Clear();
        trusted = false;
        var bad = Ensure();
        Eq(VcRedistOutcome.NotTrusted, bad.Outcome, "a download not signed by Microsoft");
        Eq(0, runs.Count, "never run");
        True(!File.Exists(Path.Combine(dl, "vc_redist.x64.exe")), "and not kept");
        trusted = true;
        version = new Version(14, 36, 32532, 0);
        Eq(VcRedistOutcome.NotTrusted, Ensure().Outcome, "an older redistributable is not run");
        version = new Version(14, 51, 36247, 0);

        // A signed copy beside the installer is used without a download; a bad one is skipped for the download.
        var requests = server.Requests;
        var beside = Path.Combine(Dir("vc-beside"), "vc_redist.x64.exe");
        File.WriteAllBytes(beside, [0x4D, 0x5A]);
        runs.Clear();
        Eq(VcRedistOutcome.Installed, Ensure(local: [beside]).Outcome, "local copy");
        Eq(requests, server.Requests, "no download with a good local copy");
        True(runs.Single().StartsWith("vc_redist.x64.exe"), "the local copy ran");
        Eq(VcRedistOutcome.DryRun, Ensure(dryRun: true, local: [beside]).Outcome, "dry run with a local copy");
    }),
    ("launch arguments and shortcut plan", () =>
    {
        Eq("-basedir \"C:\\Games\\Quake\" -basedir \"C:\\Users\\p\\QuakeVR\" -game quakevr",
            LaunchCommand.Arguments(@"C:\Games\Quake\", @"C:\Users\p\QuakeVR", LaunchVariant.Vr), "vr (trailing backslash dropped)");
        True(LaunchCommand.Arguments("a", "b", LaunchVariant.Flat).EndsWith(" -flatscreenmode"), "flat");
        True(LaunchCommand.Arguments("a", "b", LaunchVariant.Log).EndsWith(" -condebug"), "log");
        True(LaunchCommand.Arguments("a", "b", LaunchVariant.Diagnostics).EndsWith(" -diagnostics"), "diagnostics");
        True(LaunchCommand.Arguments("a", "b", LaunchVariant.BugReport).EndsWith(" -bugreport"), "bug report");
        var all = ShortcutPlanner.Plan(new ShortcutOptions { DesktopDir = @"T:\Desk", StartMenuDir = @"T:\Prog" }, @"Q:\Quake", @"T:\QVR");
        Eq(8, all.Count, "desktop VR + flat, 6 in the Start menu");
        Eq(@"T:\Prog\Quake VR Unleashed\Quake VR Unleashed (diagnostics).lnk", all[5].LinkPath, "diagnostics in the Start menu");
        Eq(@"T:\Prog\Quake VR Unleashed\Quake VR Unleashed (Create Bug Report).lnk", all[6].LinkPath, "bug report in the Start menu");
        True(all[6].Arguments.EndsWith(" -bugreport", StringComparison.Ordinal), "the bug report shortcut's -bugreport");
        True(all[5].Arguments.EndsWith(" -diagnostics", StringComparison.Ordinal), "the diagnostics shortcut's -diagnostics");
        Eq(@"T:\Prog\Quake VR Unleashed\Quake VR Unleashed (Flat Screen).lnk", all[3].LinkPath, "flat in the Start menu");
        Eq(@"T:\Desk\Quake VR Unleashed (Flat Screen).lnk", all[1].LinkPath, "flat on the desktop");
        var noDeskFlat = ShortcutPlanner.Plan(new ShortcutOptions { DesktopDir = @"T:\Desk", StartMenuDir = @"T:\Prog", DesktopFlat = false }, @"Q:\Quake", @"T:\QVR");
        Eq(7, noDeskFlat.Count, "checkbox off: only the VR shortcut on the desktop");
        True(!noDeskFlat.Any(x => x.LinkPath.StartsWith(@"T:\Desk\Quake VR Unleashed (Flat", StringComparison.Ordinal)), "no flat shortcut on the desktop");
        Eq(@"T:\QVR", all[0].WorkingDirectory, "start in the Quake VR folder");
        var desktopOnly = ShortcutPlanner.Plan(new ShortcutOptions { DesktopDir = @"T:\Desk", StartMenu = false, Log = false }, @"Q:\Quake", @"T:\QVR");
        Eq(2, desktopOnly.Count, "desktop: VR + flat (no diagnostics shortcut on the desktop)");
        Eq(0, ShortcutPlanner.Plan(new ShortcutOptions { Desktop = false, StartMenu = false }, "q", "v").Count, "none");
        // An old install record (no DesktopFlatShortcut) reads as off: an update keeps that install's old behaviour.
        var oldChoices = System.Text.Json.JsonSerializer.Deserialize<InstallChoices>("{\"DesktopShortcut\":true,\"FlatShortcut\":true}")!;
        True(!oldChoices.DesktopFlatShortcut, "old record: no desktop flat shortcut");
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
        Eq(8, record.Shortcuts.Count, "shortcuts");
        True(record.Choices.DesktopFlatShortcut, "record: desktop flat shortcut");
        True(File.Exists(Path.Combine(shortcuts, "Desktop", "Quake VR Unleashed (Flat Screen).lnk")), "fresh install: desktop flat shortcut");
        True(File.Exists(Path.Combine(options.StartMenuDir!, "Quake VR Unleashed", "Quake VR Unleashed (diagnostics).lnk")), "the diagnostics shortcut");
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
        True(!File.Exists(Path.Combine(options.StartMenuDir!, "Quake VR Unleashed", "Quake VR Unleashed (Flat Screen).lnk")), "stale shortcut removed");
        Eq(7, r2.Shortcuts.Count, "shortcuts after update");
        True(File.Exists(Path.Combine(shortcuts, "Desktop", "Quake VR Unleashed (Flat Screen).lnk")), "update: desktop flat shortcut kept");
        Eq(record.InstalledAt, r2.InstalledAt, "install date kept");

        // A shortcut of the same name the player made elsewhere is never deleted.
        var foreign = Path.Combine(shortcuts, "Desktop", "Other.lnk");
        ShellLink.Save(new ShortcutSpec(foreign, @"C:\Windows\notepad.exe", "", @"C:\", "not ours"));
        True(!Uninstaller.RemoveShortcut(foreign, target), "foreign shortcut kept");

        File.AppendAllText(Path.Combine(target, "quakevr", "default.cfg"), "\n// player edit");
        var u = Uninstaller.Uninstall(target, new UninstallOptions());
        Eq(7, u.ShortcutsRemoved, "shortcuts removed");
        True(!File.Exists(Path.Combine(shortcuts, "Desktop", "Quake VR Unleashed (Flat Screen).lnk")), "uninstall: desktop flat shortcut removed");
        True(!Directory.Exists(Path.Combine(options.StartMenuDir!, "Quake VR Unleashed")), "Start menu folder removed");
        True(File.Exists(foreign), "foreign shortcut still there");
        Eq("quakevr/default.cfg", string.Join("|", u.ChangedKept), "changed shipped file kept");
        True(u.PlayerFilesLeft.Contains("quakevr/ironwail.cfg") && u.PlayerFilesLeft.Contains("quakevr/s0.sav"), "player files left");
        True(!File.Exists(Path.Combine(target, "ironwail.exe")), "engine removed");
        True(!Directory.Exists(Path.Combine(target, "quakevr", "tools")), "empty folders removed");
        Eq(quakeBefore, Snapshot(quake), "Quake folder untouched");
    }),
    ("first-start relight: the game's marker written when ticked, removed when unticked and by uninstall", () =>
    {
        var quake = Dir("fs-quake");
        Fixtures.MakeOriginal(quake);
        var pkg = Fixtures.MakePackage(Dir("fs-pkg"), "v1");
        var target = Path.Combine(run, "fs-QuakeVR");
        InstallRecord Install(bool relight) => new InstallEngine().Install(new InstallPlan
        {
            PackagePath = pkg, TargetDir = target, QuakeDir = quake, RelightOnFirstRun = relight,
        }, null, CancellationToken.None);

        var record = Install(true);
        Eq(Path.Combine(target, "quakevr", "relight_on_first_start.txt"), FirstStartRelight.MarkerPath(target), "marker path");
        True(FirstStartRelight.Pending(target), "marker written");
        True(record.RelightPending, "asked for");
        True(!record.Files.Any(f => f.Path.Contains(FirstStartRelight.MarkerName, StringComparison.OrdinalIgnoreCase)), "marker not a recorded file");
        Eq(0, Uninstaller.Verify(target).Count, "verify clean with the marker");
        True(!LaunchCommand.Arguments(quake, target, LaunchVariant.Vr).Contains("relight"), "no relight argument on the command line");
        // The engine looks for the same name (Quake/vr/vr_relight.hpp), when the checkout is there.
        for (var d = new DirectoryInfo(AppContext.BaseDirectory); d is not null; d = d.Parent)
        {
            var hpp = Path.Combine(d.FullName, "Quake", "vr", "vr_relight.hpp");
            if (File.Exists(hpp))
            {
                True(File.ReadAllText(hpp).Contains($"firstStartMarker = \"{FirstStartRelight.MarkerName}\""), "engine's marker name");
                break;
            }
        }

        Install(false);
        True(!FirstStartRelight.Pending(target), "unticked at an update: marker removed");
        Install(true);
        True(FirstStartRelight.Pending(target), "ticked again");
        var u = Uninstaller.Uninstall(target, new UninstallOptions());
        True(!File.Exists(FirstStartRelight.MarkerPath(target)), "uninstall removes the marker");
        True(u.FolderRemoved, "folder removed (the marker is not a player file)");

        // The installer relights nothing itself (the game does, once): no relit copies of its own.
        Install(true);
        Eq(0, FirstStartRelight.RelitCopies(target), "the installer writes no relit maps");
        True(!Directory.Exists(Path.Combine(target, "quakevr", "relit_custom")), "no relit_custom from the installer");
        // The game's copies from an earlier start are counted (not the moved-aside or half-made ones) and kept by an
        // update and an uninstall, so a later first-start batch skips them.
        foreach (var rel in new[] { @"id1\maps\e1m1.relight", @"id1\maps\e1m1.bsp", @"hipnotic\maps\hip1m1.relight", @"_stale\id1\maps\e1m2.relight", @"_work\id1\e1m3.relight" })
        {
            var f = Path.Combine(target, "quakevr", "relit_custom", rel);
            Directory.CreateDirectory(Path.GetDirectoryName(f)!);
            File.WriteAllText(f, "x");
        }
        Eq(2, FirstStartRelight.RelitCopies(target), "relit copies counted");
        Install(true);
        Eq(2, FirstStartRelight.RelitCopies(target), "an update keeps the game's relit copies");
        Uninstaller.Uninstall(target, new UninstallOptions());
        Eq(2, FirstStartRelight.RelitCopies(target), "an uninstall keeps them (the player's files)");
    }),
    ("game preparation: the engine's progress lines read, its command line, the marker removed only by a finished relight", () =>
    {
        PreparationResult Read(bool relight, params string[] lines)
        {
            var r = new PreparationResult { RelightAsked = relight };
            foreach (var l in lines)
            {
                GamePreparation.Apply(r, l);
            }
            return r;
        }
        string[] maps = ["start 1.0.0 (2026-10-10 abc)", "step maps 1 3 vrcalibration", "map vrcalibration ok 1.5", "step maps 2 3 vrtutorial",
            "map vrtutorial ok 0.6", "step maps 3 3 vrstart", "map vrstart failed 600.0", "result maps ok=2 failed=1"];
        var r = Read(false, [.. maps, "done"]);
        True(r.Started && r.Finished, "started, finished");
        Eq(2, r.MapsOk, "maps ok");
        Eq(1, r.MapsFailed, "maps failed");
        var p = GamePreparation.Apply(new PreparationResult(), "step maps 3 3 vrstart");
        True(p?.Status?.Contains("the hub") == true && p.Fraction is > 0.35 and < 0.45, "the hub's step: its name, two thirds of the first maps' part of the bar without a relight");
        // The hub's guns and models, then the installed campaigns with models of their own (each line logged).
        string[] more = ["step guns", "result guns read=0 cut=12 written=12 1.0", "step models", "result models 102 162 2.8",
            "step campaigns 1 3 dopa", "campaign dopa none 0.3", "step campaigns 2 3 mg1", "campaign mg1 ok 4.1 models=8 limbs=0",
            "step campaigns 3 3 mg3", "campaign mg3 failed 600.0", "result campaigns ok=1 none=1 failed=1"];
        r = Read(false, [.. maps, .. more, "done"]);
        True(r.Finished && r.CampaignsOk == 1 && r.CampaignsNone == 1 && r.CampaignsFailed == 1, "campaigns: one prepared, one with none of its own, one failed");
        foreach (var l in more.Where(l => !l.StartsWith("result campaigns", StringComparison.Ordinal)))
        {
            True(GamePreparation.Apply(new PreparationResult(), l) is not null, $"read: {l}");
        }
        p = GamePreparation.Apply(new PreparationResult(), "step campaigns 2 3 mg1");
        True(p?.Status?.Contains("Dimension of the Machine (2 of 3)") == true && p.Fraction is > 0.75 and < 0.85, "a campaign's step: its title, past the hub's part");
        True(GamePreparation.Apply(new PreparationResult(), "step models")?.Fraction is > 0.6 and < 0.7, "the models' step: after the first maps");
        p = GamePreparation.Apply(new PreparationResult(), "campaign mg1 ok 4.1 models=8 limbs=2");
        True(p?.Log?.Contains("Dimension of the Machine") == true && p.Log.Contains("8 models") && p.Log.Contains("2 monster limbs"), "a campaign's line: its counts");
        True(GamePreparation.Apply(new PreparationResult(), "campaign mg3 failed 600.0")?.Level == LogLevel.Warning, "a failed campaign: a warning");
        p = GamePreparation.Apply(new PreparationResult(), "result guns read=4 cut=12 written=12 1.0");
        True(p?.Log?.Contains("12 made, 4 made before") == true, "the guns' line: its counts");
        p = GamePreparation.Apply(new PreparationResult { RelightAsked = true }, "relight 50 Relighting: 12 of 79 maps (00:41)");
        True(p?.Fraction is > 0.5 and < 0.6 && p.Status == "Relighting: 12 of 79 maps (00:41)", "relight's progress: the maps' part then half the rest; the game's status");
        r = Read(true, [.. maps, "step relight", "relight 99 x", "result relight ended=1 maps=12 relit=10 skipped=2 failed=0 cancelled=0 own=11 | 10 maps relit, 2 skipped in 0:42", "done"]);
        True(r.RelightDone, "relight done");
        Eq(10, r.Relit, "relit");
        Eq(2, r.RelightSkipped, "skipped");
        Eq("10 maps relit, 2 skipped in 0:42", r.RelightStatus, "the game's status line");
        True(!Read(true, "result relight ended=1 maps=12 relit=9 skipped=2 failed=1 cancelled=0 own=11 | x", "done").RelightDone, "a map failed: not done");
        True(!Read(true, "result relight ended=1 maps=12 relit=9 skipped=0 failed=0 cancelled=3 own=11 | x", "done").RelightDone, "cancelled: not done");
        True(!Read(true, "result relight ended=0 maps=0 relit=0 skipped=0 failed=0 cancelled=0 own=0 | ericw-tools' light not found", "done").RelightDone, "no light.exe: not done");
        True(GamePreparation.Apply(new PreparationResult(), "Prepare: something else") is null, "other lines ignored");

        // The command line: the shortcut's, then the run's switches (no headset, no config written), hidden, no dialog.
        var quake = Dir("prep-quake");
        Fixtures.MakeOriginal(quake);
        var target = Path.Combine(run, "prep-QuakeVR");
        var record = new InstallEngine().Install(new InstallPlan
        {
            PackagePath = Fixtures.MakePackage(Dir("prep-pkg"), "v1"), TargetDir = target, QuakeDir = quake, RelightOnFirstRun = true,
        }, null, CancellationToken.None);
        var info = GamePreparation.StartInfo(target, quake, relight: true);
        True(info.Arguments.StartsWith(LaunchCommand.Arguments(quake, target, LaunchVariant.Vr), StringComparison.Ordinal), "the shortcut's arguments first");
        foreach (var a in new[] { "-prepare \"", "-preparerelight", "-vrmock", "-noconfigwrite", "-noautoexec", "-nosound" })
        {
            True(info.Arguments.Contains(a, StringComparison.Ordinal), $"argument {a}");
        }
        True(!GamePreparation.StartInfo(target, quake, relight: false).Arguments.Contains("-preparerelight"), "no relight: no -preparerelight");
        Eq("1", info.Environment["QVR_TEST_HIDDEN"], "hidden");
        Eq("1", info.Environment["QVR_NO_ERROR_DIALOG"], "no error dialog");
        Eq(target, info.WorkingDirectory, "started in the install");
        // The engine writes the lines read here (Quake/vr/vr_prepare.cpp), when the checkout is there.
        for (var d = new DirectoryInfo(AppContext.BaseDirectory); d is not null; d = d.Parent)
        {
            var cpp = Path.Combine(d.FullName, "Quake", "vr", "vr_prepare.cpp");
            if (File.Exists(cpp))
            {
                var src = File.ReadAllText(cpp);
                True(src.Contains("\"-prepare\"") && src.Contains("\"-preparerelight\"") &&
                     src.Contains("result relight ended=%d maps=%d relit=%d skipped=%d failed=%d cancelled=%d") &&
                     src.Contains("\"step maps %d %d %s\"") && src.Contains("\"map %s ok %.1f\"") && src.Contains("line(\"done\")") &&
                     src.Contains("line(\"step guns\")") && src.Contains("\"result guns read=%d cut=%d written=%d %.1f\"") &&
                     src.Contains("line(\"step models\")") && src.Contains("\"result models %d %d %.1f\"") &&
                     src.Contains("\"step campaigns %d %d %s\"") && src.Contains("\"campaign %s ok %.1f models=%d limbs=%d\"") &&
                     src.Contains("\"campaign %s none %.1f\"") && src.Contains("\"campaign %s failed %.1f\""), "the engine's lines");
                // The installer's relight (this run's and the first-start marker's) is the stock game's and the official
                // packs' maps only: not the Map Library's nor AD's (the author's decision, 2026-11-17).
                var relightCpp = File.ReadAllText(Path.Combine(d.FullName, "Quake", "vr", "vr_relight.cpp"));
                var mapsCpp = File.ReadAllText(Path.Combine(d.FullName, "Quake", "vr", "vr_relight_maps.cpp"));
                True(src.Contains("\"vr_relight_batch official\\n\"") && !src.Contains("vr_relight_batch everything"), "the preparation relights the official set");
                True(relightCpp.Contains("Cbuf_InsertText(\"vr_relight_batch official\\n\")") && !relightCpp.Contains("Cbuf_InsertText(\"vr_relight_batch everything"),
                     "the first-start marker relights the official set");
                True(relightCpp.Contains("{\"official\", maps::Set::Official}") && mapsCpp.Contains("set == Set::Official && f.rank >="),
                     "the official set: the batch's word, the stock and official games only");
                break;
            }
        }

        // No game to start (the fixture's package has none that runs): never fatal, the marker stays.
        True(record.RelightPending && FirstStartRelight.Pending(target), "relight pending after the install");
        var noGame = GamePreparation.Run(target, quake, true, null, CancellationToken.None);
        True(!noGame.Finished && noGame.Error is not null, $"no game: not finished ({noGame.Error})");
        True(FirstStartRelight.Pending(target) && InstallRecord.Load(target)!.RelightPending, "no game: the marker and RelightPending stay");
        // A finished run whose relight did not finish leaves them; a finished relight removes them.
        GamePreparation.Conclude(target, Read(true, "result relight ended=1 maps=3 relit=2 skipped=0 failed=1 cancelled=0 own=0 | x", "done"));
        True(FirstStartRelight.Pending(target), "a failed map: the marker stays (the game relights at its first start)");
        GamePreparation.Conclude(target, Read(true, "result relight ended=1 maps=3 relit=3 skipped=0 failed=0 cancelled=0 own=0 | x"));
        True(FirstStartRelight.Pending(target), "not finished (no done line): the marker stays");
        GamePreparation.Conclude(target, Read(true, "result relight ended=1 maps=3 relit=3 skipped=0 failed=0 cancelled=0 own=0 | x", "done"));
        True(!FirstStartRelight.Pending(target) && !InstallRecord.Load(target)!.RelightPending, "a finished relight: the marker removed, RelightPending cleared");
        Eq(0, Uninstaller.Verify(target).Count, "verify clean after");
    }),
    ("apps & features: the entry in a test registry root, Setup's copy in the install, removed by the uninstall", () =>
    {
        // The registry is a made-up root (a JSON file); the real one is never written. HKLM is refused outright.
        Throws<ArgumentException>(() => new WindowsRegistryWriter().SetString(@"HKLM\Software\QuakeVRTest", "x", "y"), "HKLM refused");
        var memory = new JsonFileRegistry();
        memory.SetString(@"HKCU\A", "S", "text");
        memory.SetDword(@"HKCU\A", "D", 42);
        Eq("text", memory.GetValue(@"hkcu\a", "s"), "string back, case-insensitive");
        Eq(42, memory.GetValue(@"HKCU\A", "D"), "dword back");
        memory.DeleteKey(@"HKCU\A");
        Eq(0, memory.Keys.Count, "key deleted");

        // A made-up Setup (a development build: exe, its DLLs and JSON files) with things beside it that are not Setup's.
        var setupDir = Dir("aaf-setup-build");
        var setupExe = Path.Combine(setupDir, "QuakeVR-Setup.exe");
        foreach (var (name, body) in new[] { ("QuakeVR-Setup.exe", "exe"), ("QuakeVR-Setup.dll", "dll"), ("QuakeVR.Installer.Core.dll", "core"),
                     ("QuakeVR-Setup.runtimeconfig.json", "{}"), ("QuakeVR-Setup.deps.json", "{}"), ("installer-settings.json", "{}"),
                     ("QuakeVR.zip", "a package"), ("vc_redist.x64.exe", "not setup"), ("notes.txt", "x") })
        {
            File.WriteAllText(Path.Combine(setupDir, name), body);
        }
        True(!SetupCopy.IsSingleFile(setupExe), "a development build");
        var setupFiles = SetupCopy.FilesOf(setupExe, singleFile: false);
        Eq("setup/QuakeVR-Setup.deps.json|setup/QuakeVR-Setup.dll|setup/QuakeVR-Setup.exe|setup/QuakeVR-Setup.runtimeconfig.json|setup/QuakeVR.Installer.Core.dll|setup/installer-settings.json",
            string.Join("|", setupFiles.Select(f => f.Relative).Order(StringComparer.Ordinal)), "Setup's files only");
        Eq("setup/QuakeVR-Setup.exe|setup/installer-settings.json", string.Join("|", SetupCopy.FilesOf(setupExe, singleFile: true).Select(f => f.Relative)), "single file: the exe (and the settings)");

        var quake = Dir("aaf-quake");
        Fixtures.MakeOriginal(quake);
        var pkg = Fixtures.MakePackage(Dir("aaf-pkg"), "v1");
        var target = Path.Combine(run, "aaf-QuakeVR");
        var registryFile = Path.Combine(run, "aaf-registry.json");
        var registry = new JsonFileRegistry(registryFile);
        var logs = new List<string>();
        var progress = new SyncProgress<InstallProgress>(p => { if (p.Log is not null) { logs.Add(p.Log); } });
        var record = new InstallEngine().Install(new InstallPlan
        {
            PackagePath = pkg, TargetDir = target, QuakeDir = quake, SetupFiles = setupFiles, Registry = registry,
        }, progress, CancellationToken.None);
        var copy = Path.Combine(target, "setup", "QuakeVR-Setup.exe");
        True(File.Exists(copy) && File.Exists(Path.Combine(target, "setup", "QuakeVR-Setup.dll")), "Setup copied into the install");
        True(!File.Exists(Path.Combine(target, "setup", "QuakeVR.zip")) && !File.Exists(Path.Combine(target, "setup", "vc_redist.x64.exe")), "nothing else copied");
        Eq(6, record.Files.Count(f => f.Component == Components.Setup), "Setup's files recorded");
        Eq(0, Uninstaller.Verify(target).Count, "verify clean");
        Eq(target, SetupCopy.InstallOf(copy), "the copy knows its install");

        // The entry, in the test root (registry-file).
        string? Value(string name) => new JsonFileRegistry(registryFile).GetValue(UninstallEntry.Key, name)?.ToString();
        Eq(@"HKCU\Software\Microsoft\Windows\CurrentVersion\Uninstall\QuakeVRUnleashed", UninstallEntry.Key, "key");
        Eq("Quake VR: Unleashed", Value("DisplayName"), "DisplayName");
        Eq("Vittorio Romeo", Value("Publisher"), "Publisher");
        Eq("v1", Value("DisplayVersion"), "DisplayVersion");
        Eq(Path.Combine(target, "ironwail.exe") + ",0", Value("DisplayIcon"), "DisplayIcon: the game's exe icon");
        Eq(target, Value("InstallLocation"), "InstallLocation");
        Eq($"\"{copy}\" --uninstall --target \"{target}\"", Value("UninstallString"), "UninstallString: the copy in the install");
        Eq($"\"{copy}\" --uninstall --quiet --target \"{target}\"", Value("QuietUninstallString"), "QuietUninstallString");
        Eq(UninstallEntry.EstimatedSizeKb(record).ToString(), Value("EstimatedSize"), "EstimatedSize (KiB)");
        True(new JsonFileRegistry(registryFile).GetValue(UninstallEntry.Key, "EstimatedSize") is int, "EstimatedSize is a DWORD");
        Eq(record.InstalledAt.ToString("yyyyMMdd"), Value("InstallDate"), "InstallDate");
        Eq("1", Value("NoModify"), "NoModify");
        True(logs.Any(l => l.Contains("Installed apps")), "logged");

        // An update run from the install's own copy: its files are in use, kept as they are (and still recorded).
        var fromCopy = SetupCopy.FilesOf(copy, singleFile: false);
        var r2 = new InstallEngine().Install(new InstallPlan
        {
            PackagePath = Fixtures.MakePackage(Dir("aaf-pkg2"), "v2"), TargetDir = target, QuakeDir = quake, SetupFiles = fromCopy, Registry = registry,
        }, null, CancellationToken.None);
        Eq(6, r2.Files.Count(f => f.Component == Components.Setup), "Setup's files still recorded after an update from the copy");
        Eq("v2", Value("DisplayVersion"), "entry updated");
        // An update without a Setup copy (the console without --setup-from) keeps the old one.
        var r3 = new InstallEngine().Install(new InstallPlan { PackagePath = pkg, TargetDir = target, QuakeDir = quake }, null, CancellationToken.None);
        Eq(6, r3.Files.Count(f => f.Component == Components.Setup), "kept by an update without one");
        True(File.Exists(copy), "copy still there");

        // Another install's entry is left alone; this one's goes with the uninstall, with the copy and its folder.
        var other = new JsonFileRegistry();
        other.SetString(UninstallEntry.Key, "InstallLocation", @"D:\Elsewhere\QuakeVR");
        True(!UninstallEntry.Remove(other, target), "another folder's entry kept");
        var u = Uninstaller.Uninstall(target, new UninstallOptions { Registry = registry });
        True(u.EntryRemoved, "entry removed");
        Eq(0, new JsonFileRegistry(registryFile).Keys.Count, "the test root is empty again");
        True(!Directory.Exists(Path.Combine(target, "setup")), "Setup's copy removed");
        True(u.FolderRemoved, "folder removed");

        // The %TEMP% copy an uninstall restarts from (here a scratch folder).
        var temp = SetupCopy.CopyToTemp(setupFiles, Dir("aaf-temp"));
        True(File.Exists(temp) && File.Exists(Path.Combine(Path.GetDirectoryName(temp)!, "QuakeVR-Setup.dll")), "temp copy");
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
    ("local release server (qvr-setup serve): feed, cut download resumed, ranges, nothing outside its folder", () =>
    {
        var assets = Dir("serve-assets");
        var body = RandomNumberGenerator.GetBytes(400_000);
        var sha = Convert.ToHexStringLower(SHA256.HashData(body));
        File.WriteAllBytes(Path.Combine(assets, "QuakeVR.zip"), body);
        File.WriteAllText(Path.Combine(Path.GetDirectoryName(assets)!, "secret.txt"), "outside");
        using var server = new LocalFeedServer(assets) { DropAfter = 100_000 };
        File.WriteAllText(Path.Combine(assets, "latest.json"), $$"""
            { "schema": 1, "version": "9.9.9 (test)", "package": { "file": "QuakeVR.zip", "size": {{body.Length}}, "sha256": "{{sha}}", "urls": ["{{server.Url("QuakeVR.zip")}}"] } }
            """);
        using var http = Downloader.CreateClient();
        var feed = ReleaseFeed.FetchAsync(http, [server.Url("latest.json")], CancellationToken.None).GetAwaiter().GetResult();
        Eq("9.9.9 (test)", feed.Version, "feed read from the server");
        var dest = Path.Combine(Dir("serve-dl"), "QuakeVR.zip");
        new Downloader(http).DownloadAsync(feed.Package!.Mirrors, dest, feed.Package.Size, feed.Package.Sha256, null, CancellationToken.None).GetAwaiter().GetResult();
        Eq(1, server.Drops, "the first download was cut");
        Eq(1, server.RangeRequests, "and resumed with a Range request");
        True(File.ReadAllBytes(dest).AsSpan().SequenceEqual(body), "resumed content");
        // Explicit ranges, HEAD, 416, 404 and the folder's boundary.
        using var range = new HttpRequestMessage(HttpMethod.Get, server.Url("QuakeVR.zip"));
        range.Headers.Range = new System.Net.Http.Headers.RangeHeaderValue(10, 19);
        using var r1 = http.Send(range);
        Eq(System.Net.HttpStatusCode.PartialContent, r1.StatusCode, "bytes=10-19");
        True(r1.Content.ReadAsByteArrayAsync().GetAwaiter().GetResult().AsSpan().SequenceEqual(body.AsSpan(10, 10)), "range content");
        using var past = new HttpRequestMessage(HttpMethod.Get, server.Url("QuakeVR.zip"));
        past.Headers.Range = new System.Net.Http.Headers.RangeHeaderValue(body.Length, null);
        Eq(System.Net.HttpStatusCode.RequestedRangeNotSatisfiable, http.Send(past).StatusCode, "past the end: 416");
        using var head = http.Send(new HttpRequestMessage(HttpMethod.Head, server.Url("QuakeVR.zip")));
        Eq((long?)body.Length, head.Content.Headers.ContentLength, "HEAD's length");
        Eq(System.Net.HttpStatusCode.NotFound, http.Send(new HttpRequestMessage(HttpMethod.Get, server.Url("nothing.zip"))).StatusCode, "404");
        Eq(System.Net.HttpStatusCode.NotFound, http.Send(new HttpRequestMessage(HttpMethod.Get, server.Url("..%2Fsecret.txt"))).StatusCode, "no way out of the folder");
        True(LocalFeedServer.TryParseRange("bytes=-5", 100, out var sf, out _) && sf == 95, "suffix range");
        True(!LocalFeedServer.TryParseRange("bytes=1-2,5-6", 100, out _, out _), "multiple ranges are not parsed");
        // The sandbox's layout and QVR_SETUP_FEED.
        var sb = new Sandbox(Path.Combine(assets, "box"));
        Eq(Path.Combine(sb.Root, "QuakeVR"), sb.Target, "sandbox target");
        Eq(Path.Combine(sb.Root, "_shortcuts"), sb.ShortcutsDir, "sandbox shortcuts");
        var old = Environment.GetEnvironmentVariable(InstallerSettings.FeedEnvVar);
        Environment.SetEnvironmentVariable(InstallerSettings.FeedEnvVar, " http://127.0.0.1:1/a.json ;http://127.0.0.1:1/b.json");
        Eq("http://127.0.0.1:1/a.json|http://127.0.0.1:1/b.json", string.Join("|", InstallerSettings.FeedsFromEnvironment()), "QVR_SETUP_FEED");
        Environment.SetEnvironmentVariable(InstallerSettings.FeedEnvVar, old);
        True(new InstallerSettings().HasDefaultFeeds, "the defaults are the release hosts'");
        True(!new InstallerSettings { FeedUrls = ["http://127.0.0.1:1/latest.json"] }.HasDefaultFeeds, "another feed is a test feed");
        const string latest = "https://github.com/vittorioromeo/quakevr/releases/latest/download/latest.json";
        Eq(latest, string.Join("|", InstallerSettings.DefaultFeedUrls("1.0.0")), "a final release: one feed, the Latest release's");
        Eq(latest, string.Join("|", InstallerSettings.DefaultFeedUrls("")), "no version: the Latest release's");
        // A prerelease is never GitHub's Latest: its own tag's feed first (it finds its own package), then Latest's.
        Eq("https://github.com/vittorioromeo/quakevr/releases/download/v1.0.0-test.1/latest.json|" + latest,
            string.Join("|", InstallerSettings.DefaultFeedUrls("1.0.0-test.1")), "a prerelease: its own tag's feed first");
        Eq("https://github.com/vittorioromeo/quakevr/releases/download/v1.1.0-rc.2/latest.json|" + latest,
            string.Join("|", InstallerSettings.DefaultFeedUrls("1.1.0-rc.2 (2026-10-10 abcdef12)")), "a prerelease's version text: the tag from its number and suffix");
        // The build's own version decides the defaults (make_release.ps1 builds with /p:Version=<the release's>).
        Eq(string.Join("|", InstallerSettings.DefaultFeedUrls(InstallerBuild.Version)), string.Join("|", new InstallerSettings().FeedUrls), "the defaults follow this build's version");
        True(ReleaseVersion.Parse(InstallerBuild.Version).Number is not null && !InstallerBuild.Version.Contains('+'), $"this build's version reads ({InstallerBuild.Version}), without +commit");
        Eq(InstallerBuild.Version, InstallerBuild.Read(typeof(InstallerSettings).Assembly), "Core's version, read again");
        True(new InstallerSettings { FeedUrls = InstallerSettings.DefaultFeedUrls(InstallerBuild.Version) }.HasDefaultFeeds, "this build's own feeds are no test");
        True(new InstallerSettings { FeedUrls = ["https://github.com/vittorioromeo/quakevr/releases/latest/download/latest.json"] }.HasDefaultFeeds, "the GitHub feed is no test");
        True(!new InstallerSettings { FeedUrls = ["https://vittorioromeo.com/quakevr/latest.json"] }.HasDefaultFeeds, "the site's old feed is no release host any more");
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
    ("HD textures: no feed -> the built-in pack, a feed with hdtextures wins, the SHA-256 still checked", () =>
    {
        // The pinned pack itself: GitHub first (the support-files release), the release it was first published on second.
        var pinned = BuiltInComponents.HdTextures();
        Eq("quakevr-hq-textures-png-2026-10-03.zip", pinned.File, "pinned file");
        Eq(614_919_925L, pinned.Size, "pinned size");
        Eq(64, pinned.Sha256.Length, "pinned SHA-256");
        True(pinned.Urls.Count == 2 && pinned.Urls[0].Contains("/download/assets-") && pinned.Urls[1].Contains("/download/textures-") &&
             pinned.Urls.All(u => u.StartsWith("https://github.com/vittorioromeo/quakevr/releases/download/", StringComparison.Ordinal) && u.EndsWith("/" + pinned.File, StringComparison.Ordinal)),
            "pinned URLs: " + string.Join(", ", pinned.Urls));
        // The same pack as the release scripts' (Misc/release/support_assets.json), when the self-test runs in the repo.
        var repo = new DirectoryInfo(AppContext.BaseDirectory);
        while (repo is not null && !File.Exists(Path.Combine(repo.FullName, "Misc", "release", "support_assets.json")))
        {
            repo = repo.Parent;
        }
        if (repo is not null)
        {
            using var doc = System.Text.Json.JsonDocument.Parse(File.ReadAllText(Path.Combine(repo.FullName, "Misc", "release", "support_assets.json")));
            var hd = doc.RootElement.GetProperty("files").GetProperty("hdtextures");
            Eq(hd.GetProperty("file").GetString(), pinned.File, "support_assets.json's file");
            Eq(hd.GetProperty("size").GetInt64(), pinned.Size, "support_assets.json's size");
            Eq(hd.GetProperty("sha256").GetString(), pinned.Sha256, "support_assets.json's SHA-256");
            Eq($"/download/{doc.RootElement.GetProperty("tag").GetString()}/{pinned.File}", new Uri(pinned.Urls[0]).AbsolutePath.Replace("/vittorioromeo/quakevr/releases", ""), "support_assets.json's tag first");
        }
        True(new InstallerSettings().Component(null, Components.HdTextures) is { BuiltIn: true } d && d.File.Sha256 == pinned.Sha256, "the defaults: the pinned pack");

        // A local server: the built-in pack (as installer-settings.json would point it) and a feed's own.
        using var server = new LocalHttpServer();
        var builtInBody = RandomNumberGenerator.GetBytes(30_000);
        var feedBody = RandomNumberGenerator.GetBytes(20_000);
        server.Serve("assets/tex.zip", builtInBody);
        server.Serve("feed/tex2.zip", feedBody);
        string Sha(byte[] b) => Convert.ToHexStringLower(SHA256.HashData(b));
        var settings = new InstallerSettings
        {
            FeedUrls = [server.Url("latest.json").ToString()],
            BuiltInComponents = new() { [Components.HdTextures] = new FeedFile { File = "tex.zip", Size = builtInBody.Length, Sha256 = Sha(builtInBody), Urls = [server.Url("missing/tex.zip").ToString(), server.Url("assets/tex.zip").ToString()] } },
        };
        using var http = Downloader.CreateClient();
        string Fetch(ResolvedComponent c, string dir)
        {
            var dest = Path.Combine(Dir(dir), c.File.File);
            new Downloader(http).DownloadAsync(c.File.Mirrors, dest, c.File.Size, c.File.Sha256, null, CancellationToken.None, attemptsPerMirror: 1).GetAwaiter().GetResult();
            return dest;
        }

        // No feed (latest.json is a 404): the built-in pack, from its second URL.
        ReleaseFeed? noFeed = null;
        try
        {
            noFeed = ReleaseFeed.FetchAsync(http, settings.FeedUrls.Select(u => new Uri(u)), CancellationToken.None).GetAwaiter().GetResult();
        }
        catch (InstallException)
        {
        }
        Eq(null, noFeed, "no feed");
        var c1 = settings.Component(noFeed, Components.HdTextures)!;
        True(c1.BuiltIn, "no feed: built in");
        True(File.ReadAllBytes(Fetch(c1, "hd-builtin")).AsSpan().SequenceEqual(builtInBody), "the built-in pack downloaded");

        // A feed without hdtextures: still the built-in pack.
        var sha = new string('b', 64);
        server.Serve("latest.json", $$"""
            { "schema": 1, "version": "v2", "package": { "file": "QuakeVR.zip", "size": 1, "sha256": "{{sha}}", "urls": ["{{server.Url("QuakeVR.zip")}}"] } }
            """);
        var bare = ReleaseFeed.FetchAsync(http, [server.Url("latest.json")], CancellationToken.None).GetAwaiter().GetResult();
        True(settings.Component(bare, Components.HdTextures) is { BuiltIn: true }, "a feed without hdtextures: built in");

        // A feed with hdtextures: the feed's, even though a built-in one exists.
        server.Serve("latest.json", $$"""
            { "schema": 1, "version": "v3", "package": { "file": "QuakeVR.zip", "size": 1, "sha256": "{{sha}}", "urls": ["{{server.Url("QuakeVR.zip")}}"] },
              "components": { "hdtextures": { "file": "tex2.zip", "size": {{feedBody.Length}}, "sha256": "{{Sha(feedBody)}}", "urls": ["{{server.Url("feed/tex2.zip")}}"] } } }
            """);
        var full = ReleaseFeed.FetchAsync(http, [server.Url("latest.json")], CancellationToken.None).GetAwaiter().GetResult();
        var c2 = settings.Component(full, Components.HdTextures)!;
        True(!c2.BuiltIn && c2.File.File == "tex2.zip", "the feed wins");
        True(File.ReadAllBytes(Fetch(c2, "hd-feed")).AsSpan().SequenceEqual(feedBody), "the feed's pack downloaded");

        // The built-in pack is checked as a feed's is: a wrong SHA-256 fails the download.
        settings.BuiltInComponents[Components.HdTextures].Sha256 = new string('0', 64);
        var bad = settings.Component(null, Components.HdTextures)!;
        var e = Throws<InstallException>(() => Fetch(bad, "hd-bad"), "a wrong built-in hash");
        True(e.Message.Contains("SHA-256"), e.Message);

        // No built-in and no feed: nothing.
        Eq(null, new InstallerSettings { BuiltInComponents = [] }.Component(null, Components.HdTextures), "nothing at all");
    }),
    ("arcane dimensions: the pin (built in = support_assets.json's thirdParty = the doc's hosts in order; the engine's pak2 pin)", () =>
    {
        var pinned = BuiltInComponents.ArcaneDimensions();
        Eq("ad_v1_80p1final.zip", pinned.File, "pinned file");
        Eq(320_914_417L, pinned.Size, "pinned size");
        Eq("7ad993da6c760c446ca31fad71e9f5b6c9eee99b6354f161aa746b572fe70a7d", pinned.Sha256, "pinned SHA-256");
        Eq("www.quaddicted.com|www.quaddicted.com|www.simonoc.com|born-robotic.net", string.Join("|", pinned.Mirrors.Select(u => u.Host)), "hosts in order");
        True(pinned.Urls[0].Contains("/by-sha256/7a/" + pinned.Sha256 + "/"), "content-addressed first");
        True(pinned.Urls.All(u => u.StartsWith("https://", StringComparison.Ordinal) && u.EndsWith("/" + pinned.File, StringComparison.Ordinal) && !u.Contains("github.com")),
            "sock's hosts only, never ours: " + string.Join(", ", pinned.Urls));
        True(new InstallerSettings().Component(null, Components.ArcaneDimensions) is { BuiltIn: true } d && d.File.Sha256 == pinned.Sha256, "the defaults: the pin");
        var repo = new DirectoryInfo(AppContext.BaseDirectory);
        while (repo is not null && !File.Exists(Path.Combine(repo.FullName, "Misc", "release", "support_assets.json")))
        {
            repo = repo.Parent;
        }
        if (repo is not null)
        {
            using var doc = System.Text.Json.JsonDocument.Parse(File.ReadAllText(Path.Combine(repo.FullName, "Misc", "release", "support_assets.json")));
            True(!doc.RootElement.GetProperty("files").TryGetProperty("ad", out _), "AD is not one of our hosted support files");
            var ad = doc.RootElement.GetProperty("thirdParty").GetProperty("ad");
            Eq(pinned.File, ad.GetProperty("file").GetString(), "support_assets.json's file");
            Eq(pinned.Size, ad.GetProperty("size").GetInt64(), "support_assets.json's size");
            Eq(pinned.Sha256, ad.GetProperty("sha256").GetString(), "support_assets.json's SHA-256");
            Eq(string.Join("|", pinned.Urls), string.Join("|", ad.GetProperty("mirrors").EnumerateArray().Select(m => m.GetString())), "support_assets.json's hosts");
            var inc = File.ReadAllText(Path.Combine(repo.FullName, "Quake", "vr", "vr_ad_data.inc"));
            True(inc.Contains($"#define VR_AD_PAK2_SIZE {ArcaneDimensions.Latest.Size}"), "the engine's pak2 size");
            True(inc.Contains($"#define VR_AD_PAK2_DIRSHA \"{ArcaneDimensions.Latest.DirSha256}\""), "the engine's pak2 directory SHA-256");
        }
        // A feed's ad wins (latest.json components.ad), as for the HD textures.
        var feed = ReleaseFeed.Parse($$"""
            { "schema": 1, "version": "v9", "components": { "ad": { "file": "ad.zip", "size": 3, "sha256": "{{new string('c', 64)}}", "urls": ["https://example.invalid/ad.zip"] } } }
            """);
        True(new InstallerSettings().Component(feed, Components.ArcaneDimensions) is { BuiltIn: false } f && f.File.File == "ad.zip", "the feed's ad wins");
    }),
    ("arcane dimensions: an existing copy (1.80p1 used, an older one or an incomplete one offers the download, the game's root order)", () =>
    {
        // A fake AD: every required file in pak0, and a pak2 whose signature is this test's pin.
        AdPak2Pin MakeAd(string root, bool pak2 = true, string? leaveOut = null)
        {
            Fixtures.MakePack(root, "ad", leaveOut);
            var p2 = Path.Combine(root, "ad", "pak2.pak");
            if (pak2)
            {
                PakFile.Write(p2, [("progs/ad_patch1.mdl", Fixtures.Mdl())]);
            }
            var sig = ArcaneDimensions.Pak2Signature(p2);
            return sig is { } s ? new AdPak2Pin(s.Size, s.DirSha256) : new AdPak2Pin(0, "");
        }
        var steam = Dir("ad-steam-rerelease");
        var quake = Dir("ad-quake");
        var pin = MakeAd(quake);
        var roots = ArcaneDimensions.SearchRoots([steam], quake);
        var found = ArcaneDimensions.FindExisting(roots, pin)!;
        Eq(ModCopyState.Ready, found.State, "1.80p1 in the Quake folder");
        True(PathUtil.SamePath(found.Folder, Path.Combine(quake, "ad")), "its folder: " + found.Folder);
        True(ArcaneDimensions.Describe(found).StartsWith($"Found in {found.Folder}: will be used", StringComparison.Ordinal), ArcaneDimensions.Describe(found));
        Eq(ModCopyState.OtherVersion, ArcaneDimensions.FindExisting(roots)!.State, "the same pak2 against the real pin: another version");
        // The real release's pin, on a pak2 whose header and directory differ: refused.
        True(!ArcaneDimensions.IsLatest(Path.Combine(quake, "ad")), "a fake pak2 is not 1.80p1");

        var older = Dir("ad-older");
        MakeAd(older, pak2: false);
        var o = ArcaneDimensions.FindExisting(ArcaneDimensions.SearchRoots([], older), pin)!;
        Eq(ModCopyState.OtherVersion, o.State, "no pak2: 1.80 or older");
        True(ArcaneDimensions.Describe(o).Contains("isn't 1.80 patch 1") && ArcaneDimensions.Describe(o).Contains("download"), ArcaneDimensions.Describe(o));
        var broken = Dir("ad-incomplete");
        MakeAd(broken, leaveOut: PackLists.Resources("ad")[0]);
        Eq(ModCopyState.Incomplete, ArcaneDimensions.FindExisting(ArcaneDimensions.SearchRoots([], broken), pin)!.State, "a missing file: incomplete");
        Eq(null, ArcaneDimensions.FindExisting(ArcaneDimensions.SearchRoots([], Dir("ad-none")), pin), "none");
        // The game's order: the Quake folder over a store root; an empty ad folder (a texture pack's) is skipped.
        MakeAd(steam, pak2: false);
        Eq(ModCopyState.Ready, ArcaneDimensions.FindExisting(roots, pin)!.State, "the Quake folder's copy wins over the store's older one");
        var emptyAd = Dir("ad-texonly");
        Directory.CreateDirectory(Path.Combine(emptyAd, "ad", "textures"));
        Eq(ModCopyState.Ready, ArcaneDimensions.FindExisting(ArcaneDimensions.SearchRoots([quake], emptyAd), pin)!.State, "a folder without AD's data: the next root's copy");
        // A real 1.80 patch 1 on this PC (read only: pak2's header and directory), when there is one: the engine's check.
        foreach (var real in new[] { @"C:\Program Files (x86)\Steam\steamapps\common\Quake\ad" })
        {
            if (File.Exists(Path.Combine(real, "pak2.pak")))
            {
                True(ArcaneDimensions.IsLatest(real), $"the real 1.80p1 in {real} passes the engine's pak2 check");
                Console.WriteLine($"    (real AD 1.80p1 checked: {real})");
            }
        }
    }),
    ("arcane dimensions: a sandboxed install from a local server (size and SHA-256 pinned, ad/ unpacked and recorded, update keeps, repair restores, uninstall removes)", () =>
    {
        // The fake release zip: AD's layout (one ad/ folder), small; its size and SHA-256 pinned below.
        var src = Dir("ad-zip-src");
        Fixtures.MakePack(src, "ad");
        PakFile.Write(Path.Combine(src, "ad", "pak2.pak"), [("progs/ad_patch1.mdl", Fixtures.Mdl())]);
        File.WriteAllText(Path.Combine(src, "ad", "quake.rc"), "exec default.cfg");
        File.WriteAllText(Path.Combine(src, "ad", ArcaneDimensions.ReadmeFile), "Arcane Dimensions readme (must stay with the files)");
        var zip = Fixtures.Zip(src, Path.Combine(run, "ad_v1_80p1final.zip"));
        var body = File.ReadAllBytes(zip);
        var sha = Convert.ToHexStringLower(SHA256.HashData(body));
        using var server = new LocalHttpServer();
        server.Serve("quaddicted/ad_v1_80p1final.zip", body);
        server.Serve("evil/ad_v1_80p1final.zip", [.. body.Select(b => (byte)(b ^ 0x5a))]);
        var pinnedFile = new FeedFile { File = "ad_v1_80p1final.zip", Size = body.Length, Sha256 = sha,
            Urls = [server.Url("missing/ad_v1_80p1final.zip").ToString(), server.Url("evil/ad_v1_80p1final.zip").ToString(), server.Url("quaddicted/ad_v1_80p1final.zip").ToString()] };
        var quake = Dir("adi-quake");
        var target = Path.Combine(run, "adi-QuakeVR");
        var downloads = Dir("adi-downloads");
        new InstallEngine().Install(new InstallPlan { PackagePath = Fixtures.MakePackage(Dir("adi-pkg1"), "1.0.0"), TargetDir = target, QuakeDir = quake }, null, CancellationToken.None);
        var coreCount = InstallRecord.Load(target)!.Files.Count;
        using var http = Downloader.CreateClient();
        var logs = new List<string>();
        var outcome = ArcaneDimensions.AddAsync(http, target, pinnedFile, downloads, null, new SyncProgress<InstallProgress>(p => { if (p.Log is not null) { logs.Add(p.Log); } }), null,
            CancellationToken.None, attemptsPerMirror: 1).GetAwaiter().GetResult();
        True(outcome.Installed, outcome.Message);
        foreach (var name in new[] { "pak0.pak", "pak2.pak", "quake.rc", ArcaneDimensions.ReadmeFile })
        {
            var installed = Path.Combine(target, "ad", name);
            True(File.Exists(installed), $"ad/{name} installed");
            True(File.ReadAllBytes(installed).AsSpan().SequenceEqual(File.ReadAllBytes(Path.Combine(src, "ad", name))), $"ad/{name} unmodified");
        }
        var r = InstallRecord.Load(target)!;
        Eq(4, r.Files.Count(f => f.Component == Components.ArcaneDimensions), "4 AD files recorded");
        Eq(coreCount, r.Files.Count(f => f.Component != Components.ArcaneDimensions), "the program files' records kept");
        Eq(sha, r.ArcaneDimensionsSha256, "its SHA-256 recorded");
        True(r.Choices.ArcaneDimensions && r.Directories.Contains("ad"), "chosen, its folder recorded");
        True(!Directory.EnumerateFiles(target, "*.qvrnew", SearchOption.AllDirectories).Any(), "no staging files left");

        // Anything else is refused: a zip of another size, the right size with another SHA-256.
        var wrongSize = Path.Combine(Dir("adi-wrong"), "ad_v1_80p1final.zip");
        File.WriteAllBytes(wrongSize, body[..^1]);
        True(Throws<InstallException>(() => ArcaneDimensions.Install(target, wrongSize, pinnedFile, null, CancellationToken.None), "wrong size").Message.Contains("bytes"), "size checked first");
        var wrongSha = Path.Combine(Dir("adi-wrong2"), "ad_v1_80p1final.zip");
        File.WriteAllBytes(wrongSha, [.. body.Select(b => (byte)(b ^ 1))]);
        True(Throws<InstallException>(() => ArcaneDimensions.Install(target, wrongSha, pinnedFile, null, CancellationToken.None), "wrong hash").Message.Contains("SHA-256"), "then the SHA-256");
        // The real pin refuses the small zip too (what --ad-zip with any other file meets).
        True(Throws<InstallException>(() => ArcaneDimensions.Install(target, zip, BuiltInComponents.ArcaneDimensions(), null, CancellationToken.None), "the real pin").Message.Contains("320914417 bytes"), "the real pin's size");

        // An update keeps AD; its plan keeps, replaces (another pin) or restores (a repair, a file damaged).
        var r2 = new InstallEngine().Install(new InstallPlan { Mode = InstallMode.Update, PackagePath = Fixtures.MakePackage(Dir("adi-pkg2"), "1.1.0"), TargetDir = target, QuakeDir = quake }, null, CancellationToken.None);
        Eq(4, r2.Files.Count(f => f.Component == Components.ArcaneDimensions), "the update keeps AD recorded");
        True(File.Exists(Path.Combine(target, "ad", "pak0.pak")) && r2.ArcaneDimensionsSha256 == sha && r2.Choices.ArcaneDimensions, "and on disk, with its SHA-256");
        Eq(ModAction.Keep, ArcaneDimensions.Plan(target, r2, pinnedFile, verify: false).Action, "same zip: kept");
        Eq(ModAction.Replace, ArcaneDimensions.Plan(target, r2, new FeedFile { File = "x.zip", Size = 1, Sha256 = new string('d', 64) }, verify: false).Action, "another zip: replaced");
        File.WriteAllText(Path.Combine(target, "ad", "quake.rc"), "damaged");
        Eq(ModAction.Restore, ArcaneDimensions.Plan(target, r2, pinnedFile, verify: true).Action, "a repair restores");
        var restored = ArcaneDimensions.AddAsync(http, target, pinnedFile, downloads, null, null, null, CancellationToken.None, 1).GetAwaiter().GetResult();
        True(restored.Installed && File.ReadAllText(Path.Combine(target, "ad", "quake.rc")) == "exec default.cfg", "restored from the downloaded zip: " + restored.Message);
        Eq(ModAction.NotInstalled, ArcaneDimensions.Plan(Dir("adi-none"), new InstallRecord(), pinnedFile, verify: true).Action, "none installed");

        // A file of the player's in ad\ that Setup did not put there: never overwritten.
        var t2 = Path.Combine(run, "adi-own-QuakeVR");
        new InstallEngine().Install(new InstallPlan { PackagePath = Fixtures.MakePackage(Dir("adi-pkg3"), "1.0.0"), TargetDir = t2, QuakeDir = quake }, null, CancellationToken.None);
        Directory.CreateDirectory(Path.Combine(t2, "ad"));
        File.WriteAllText(Path.Combine(t2, "ad", "quake.rc"), "the player's own");
        var own = ArcaneDimensions.AddAsync(http, t2, pinnedFile, downloads, null, null, null, CancellationToken.None, 1).GetAwaiter().GetResult();
        True(!own.Installed && own.Message.Contains("Setup did not put it there"), own.Message);
        Eq("the player's own", File.ReadAllText(Path.Combine(t2, "ad", "quake.rc")), "the player's file kept");
        True(!File.Exists(Path.Combine(t2, "ad", "pak0.pak")), "nothing of AD installed");

        // The uninstall removes ad\ (Setup's) with the rest.
        Uninstaller.Uninstall(target, new UninstallOptions());
        True(!Directory.Exists(Path.Combine(target, "ad")), "uninstall removed ad\\");
        True(!Directory.Exists(target), "and the whole (empty) folder");
    }),
    ("dwell: the pin (built in = support_assets.json's thirdParty = DWELL.md's hosts in order), off by default, the feed's dwell wins", () =>
    {
        var pinned = BuiltInComponents.Dwell();
        Eq("dwellv2p2.zip", pinned.File, "pinned file");
        Eq(384_629_995L, pinned.Size, "pinned size");
        Eq("15bbc5dc0eb3fee5763835cea1aa62ab263545ca8c04aaa019982d45d1e8d11d", pinned.Sha256, "pinned SHA-256");
        Eq("www.quaddicted.com|www.quaddicted.com", string.Join("|", pinned.Mirrors.Select(u => u.Host)), "hosts in order");
        True(pinned.Urls[0].EndsWith("/by-sha256/15/" + pinned.Sha256 + "/dwellv2p2.zip", StringComparison.Ordinal), "content-addressed first");
        True(pinned.Urls[1].EndsWith("/filebase/dwellv2p2.zip", StringComparison.Ordinal), "the filebase second");
        True(pinned.Urls.All(u => u.StartsWith("https://", StringComparison.Ordinal) && !u.Contains("github.com")), "the authors' archive only, never ours");
        True(new InstallerSettings().Component(null, Components.Dwell) is { BuiltIn: true } d && d.File.Sha256 == pinned.Sha256, "the defaults: the pin");
        True(Dwell.OnByDefault, "on by default: Dwell's maps play (one constant: Dwell.OnByDefault)");
        var repo = new DirectoryInfo(AppContext.BaseDirectory);
        while (repo is not null && !File.Exists(Path.Combine(repo.FullName, "Misc", "release", "support_assets.json")))
        {
            repo = repo.Parent;
        }
        if (repo is not null)
        {
            using var doc = System.Text.Json.JsonDocument.Parse(File.ReadAllText(Path.Combine(repo.FullName, "Misc", "release", "support_assets.json")));
            True(!doc.RootElement.GetProperty("files").TryGetProperty("dwell", out _), "Dwell is not one of our hosted support files");
            var j = doc.RootElement.GetProperty("thirdParty").GetProperty("dwell");
            Eq(pinned.File, j.GetProperty("file").GetString(), "support_assets.json's file");
            Eq(pinned.Size, j.GetProperty("size").GetInt64(), "support_assets.json's size");
            Eq(pinned.Sha256, j.GetProperty("sha256").GetString(), "support_assets.json's SHA-256");
            Eq(string.Join("|", pinned.Urls), string.Join("|", j.GetProperty("mirrors").EnumerateArray().Select(m => m.GetString())), "support_assets.json's hosts");
        }
        var feed = ReleaseFeed.Parse($$"""
            { "schema": 1, "version": "v9", "components": { "dwell": { "file": "dw.zip", "size": 3, "sha256": "{{new string('c', 64)}}", "urls": ["https://example.invalid/dw.zip"] } } }
            """);
        True(new InstallerSettings().Component(feed, Components.Dwell) is { BuiltIn: false } f && f.File.File == "dw.zip", "the feed's dwell wins");
    }),
    ("dwell: an existing copy (v2.2 used by its progs.dat hash, an older one or an incomplete one offers the download, dwell or dwellv2p2, the game's root order)", () =>
    {
        void MakeDwell(string root, string folder, string progs, bool pak = true)
        {
            var d = Path.Combine(root, folder);
            Directory.CreateDirectory(d);
            File.WriteAllText(Path.Combine(d, "progs.dat"), progs);
            if (pak)
            {
                File.WriteAllText(Path.Combine(d, "pak0.pak"), "pak");
            }
        }
        string Sha(string root, string folder) => PackageManifest.HashFile(Path.Combine(root, folder, "progs.dat"));
        var steam = Dir("dw-steam-rerelease");
        var quake = Dir("dw-quake");
        True(Dwell.FindExisting(Dwell.SearchRoots([steam], quake)) is null, "none: null");
        MakeDwell(quake, "dwell", "v2.2 progs");
        var pin = Sha(quake, "dwell");
        var roots = Dwell.SearchRoots([steam], quake);
        var found = Dwell.FindExisting(roots, pin)!;
        Eq(ModCopyState.Ready, found.State, "v2.2 in the Quake folder");
        True(found.Usable && Dwell.Describe(found).Contains("will be used (nothing downloaded)"), "used in place");
        Eq(ModCopyState.OtherVersion, Dwell.FindExisting(roots, new string('0', 64))!.State, "another progs.dat: an older version, reported not used");
        True(!Dwell.FindExisting(roots, new string('0', 64))!.Usable && Dwell.Describe(Dwell.FindExisting(roots, new string('0', 64))!).Contains("isn't v2.2"), "and said so");
        var alt = Dir("dw-quake-alt");
        MakeDwell(alt, "dwellv2p2", "v2.2 progs");
        Eq(ModCopyState.Ready, Dwell.FindExisting(Dwell.SearchRoots([], alt), pin)!.State, "the dwellv2p2 folder counts too");
        var inc = Dir("dw-quake-inc");
        MakeDwell(inc, "dwell", "v2.2 progs", pak: false);
        Eq(ModCopyState.Incomplete, Dwell.FindExisting(Dwell.SearchRoots([], inc), pin)!.State, "no pak0.pak: incomplete");
        var empty = Dir("dw-quake-empty");
        Directory.CreateDirectory(Path.Combine(empty, "dwell", "music"));
        True(Dwell.FindExisting(Dwell.SearchRoots([], empty), pin) is null, "a folder with none of its data: none");
        // The game's order: the Quake folder (last) beats a store root.
        MakeDwell(steam, "dwell", "another progs");
        Eq(ModCopyState.Ready, Dwell.FindExisting(Dwell.SearchRoots([steam], quake), pin)!.State, "the highest-priority root's copy wins");
        True(Dwell.FindExisting(Dwell.SearchRoots([steam], quake), pin)!.Folder.StartsWith(PathUtil.TryNormalize(quake)!, StringComparison.OrdinalIgnoreCase), "the Quake folder's");
    }),
    ("dwell: a sandboxed install of a Dwell-shaped zip (dwellv2p2/ becomes dwell/, READMEs kept, Track107 lower-cased, recorded; update keeps, repair restores, uninstall removes, AD unaffected)", () =>
    {
        var src = Dir("dwi-zip-src");
        var top = Path.Combine(src, "dwellv2p2");
        Directory.CreateDirectory(Path.Combine(top, "music"));
        Directory.CreateDirectory(Path.Combine(top, "maps"));
        File.WriteAllText(Path.Combine(top, "progs.dat"), "progs");
        File.WriteAllText(Path.Combine(top, "pak0.pak"), "pak");
        File.WriteAllText(Path.Combine(top, "quake.rc"), "exec default.cfg");
        File.WriteAllText(Path.Combine(top, "music", "Track107.ogg"), "ogg107");
        File.WriteAllText(Path.Combine(top, "music", "track101.ogg"), "ogg101");
        File.WriteAllText(Path.Combine(top, "maps", "d1m1.bsp"), "bsp");
        File.WriteAllText(Path.Combine(src, "README_dwellv2p2.txt"), "readme");
        File.WriteAllText(Path.Combine(src, "README_changelist.txt"), "changes");
        var zip = Fixtures.Zip(src, Path.Combine(run, "dwellv2p2.zip"));
        var body = File.ReadAllBytes(zip);
        var sha = Convert.ToHexStringLower(SHA256.HashData(body));
        using var server = new LocalHttpServer();
        server.Serve("quaddicted/dwellv2p2.zip", body);
        var pinnedFile = new FeedFile { File = "dwellv2p2.zip", Size = body.Length, Sha256 = sha,
            Urls = [server.Url("missing/dwellv2p2.zip").ToString(), server.Url("quaddicted/dwellv2p2.zip").ToString()] };
        var quake = Dir("dwi-quake");
        var target = Path.Combine(run, "dwi-QuakeVR");
        var downloads = Dir("dwi-downloads");
        new InstallEngine().Install(new InstallPlan { PackagePath = Fixtures.MakePackage(Dir("dwi-pkg1"), "1.0.0"), TargetDir = target, QuakeDir = quake }, null, CancellationToken.None);
        var coreCount = InstallRecord.Load(target)!.Files.Count;
        using var http = Downloader.CreateClient();
        var outcome = Dwell.AddAsync(http, target, pinnedFile, downloads, null, null, null, CancellationToken.None, attemptsPerMirror: 1).GetAwaiter().GetResult();
        True(outcome.Installed, outcome.Message);
        foreach (var (name, content) in new[] { ("progs.dat", "progs"), ("pak0.pak", "pak"), ("quake.rc", "exec default.cfg"), ("maps/d1m1.bsp", "bsp"), ("music/track101.ogg", "ogg101"),
                     ("music/track107.ogg", "ogg107"), (Dwell.ReadmeFile, "readme"), ("README_changelist.txt", "changes") })
        {
            var installed = Path.Combine(target, "dwell", name);
            True(File.Exists(installed), $"dwell/{name} installed");
            Eq(content, File.ReadAllText(installed), $"dwell/{name} unmodified");
        }
        True(!Directory.Exists(Path.Combine(target, "dwellv2p2")), "no dwellv2p2 folder in the install");
        True(Directory.EnumerateFiles(Path.Combine(target, "dwell", "music")).Select(Path.GetFileName).Contains("track107.ogg", StringComparer.Ordinal), "Track107.ogg is lower case on disk");
        var r = InstallRecord.Load(target)!;
        Eq(8, r.Files.Count(f => f.Component == Components.Dwell), "8 Dwell files recorded");
        True(r.Files.Any(f => f.Path == "dwell/music/track107.ogg"), "the renamed track recorded under its new name");
        Eq(coreCount, r.Files.Count(f => f.Component != Components.Dwell), "the program files' records kept");
        Eq(sha, r.DwellSha256, "its SHA-256 recorded");
        True(r.Choices.Dwell && r.Directories.Contains("dwell") && !r.Choices.ArcaneDimensions && r.ArcaneDimensionsSha256 is null, "chosen, its folder recorded, AD untouched");
        True(!Directory.EnumerateFiles(target, "*.qvrnew", SearchOption.AllDirectories).Any(), "no staging files left");

        var wrongSize = Path.Combine(Dir("dwi-wrong"), "dwellv2p2.zip");
        File.WriteAllBytes(wrongSize, body[..^1]);
        True(Throws<InstallException>(() => Dwell.Install(target, wrongSize, pinnedFile, null, CancellationToken.None), "wrong size").Message.Contains("bytes"), "size checked first");
        True(Throws<InstallException>(() => Dwell.Install(target, zip, BuiltInComponents.Dwell(), null, CancellationToken.None), "the real pin").Message.Contains("384629995 bytes"), "the real pin refuses a small zip");
        // A zip that is not Dwell (no progs.dat) is refused even with a matching pin.
        var other = Dir("dwi-other-src");
        Directory.CreateDirectory(Path.Combine(other, "x"));
        File.WriteAllText(Path.Combine(other, "x", "a.txt"), "a");
        var otherZip = Fixtures.Zip(other, Path.Combine(run, "other.zip"));
        var otherPin = new FeedFile { File = "other.zip", Size = new FileInfo(otherZip).Length, Sha256 = PackageManifest.HashFile(otherZip), Urls = ["https://example.invalid/other.zip"] };
        True(Throws<InstallException>(() => Dwell.Install(target, otherZip, otherPin, null, CancellationToken.None), "not dwell").Message.Contains("not Dwell"), "no progs.dat: not Dwell");

        var r2 = new InstallEngine().Install(new InstallPlan { Mode = InstallMode.Update, PackagePath = Fixtures.MakePackage(Dir("dwi-pkg2"), "1.1.0"), TargetDir = target, QuakeDir = quake }, null, CancellationToken.None);
        Eq(8, r2.Files.Count(f => f.Component == Components.Dwell), "the update keeps Dwell recorded");
        True(File.Exists(Path.Combine(target, "dwell", "pak0.pak")) && r2.DwellSha256 == sha && r2.Choices.Dwell, "and on disk, with its SHA-256");
        Eq(ModAction.Keep, Dwell.Plan(target, r2, pinnedFile, verify: false).Action, "same zip: kept");
        Eq(ModAction.Replace, Dwell.Plan(target, r2, new FeedFile { File = "x.zip", Size = 1, Sha256 = new string('d', 64) }, verify: false).Action, "another zip: replaced");
        File.WriteAllText(Path.Combine(target, "dwell", "quake.rc"), "damaged");
        Eq(ModAction.Restore, Dwell.Plan(target, r2, pinnedFile, verify: true).Action, "a repair restores");
        var restored = Dwell.AddAsync(http, target, pinnedFile, downloads, null, null, null, CancellationToken.None, 1).GetAwaiter().GetResult();
        True(restored.Installed && File.ReadAllText(Path.Combine(target, "dwell", "quake.rc")) == "exec default.cfg", "restored from the downloaded zip: " + restored.Message);
        Eq(ModAction.NotInstalled, Dwell.Plan(Dir("dwi-none"), new InstallRecord(), pinnedFile, verify: true).Action, "none installed");

        var t2 = Path.Combine(run, "dwi-own-QuakeVR");
        new InstallEngine().Install(new InstallPlan { PackagePath = Fixtures.MakePackage(Dir("dwi-pkg3"), "1.0.0"), TargetDir = t2, QuakeDir = quake }, null, CancellationToken.None);
        Directory.CreateDirectory(Path.Combine(t2, "dwell"));
        File.WriteAllText(Path.Combine(t2, "dwell", "quake.rc"), "the player's own");
        var own = Dwell.AddAsync(http, t2, pinnedFile, downloads, null, null, null, CancellationToken.None, 1).GetAwaiter().GetResult();
        True(!own.Installed && own.Message.Contains("Setup did not put it there"), own.Message);
        Eq("the player's own", File.ReadAllText(Path.Combine(t2, "dwell", "quake.rc")), "the player's file kept");
        True(!File.Exists(Path.Combine(t2, "dwell", "pak0.pak")), "nothing of Dwell installed");

        Uninstaller.Uninstall(target, new UninstallOptions());
        True(!Directory.Exists(Path.Combine(target, "dwell")), "uninstall removed dwell\\");
        True(!Directory.Exists(target), "and the whole (empty) folder");
    }),
    ("dwell: the real dwellv2p2.zip from the local downloads folder (no network): 447 files unpacked as released, Track107 lower-cased, hash-validated as existing", () =>
    {
        var real = "C:/OHWorkspace/qvr-kit/downloads/dwell/dwellv2p2.zip";
        if (!File.Exists(real))
        {
            Console.WriteLine("    (skipped: the real zip is not in the local downloads folder)");
            return;
        }
        var quake = Dir("dwr-quake");
        var target = Path.Combine(run, "dwr-QuakeVR");
        new InstallEngine().Install(new InstallPlan { PackagePath = Fixtures.MakePackage(Dir("dwr-pkg"), "1.0.0"), TargetDir = target, QuakeDir = quake }, null, CancellationToken.None);
        using var http = Downloader.CreateClient();
        var outcome = Dwell.AddAsync(http, target, BuiltInComponents.Dwell(), Dir("dwr-downloads"), real, null, null, CancellationToken.None, 1).GetAwaiter().GetResult();
        True(outcome.Installed, outcome.Message);
        var r = InstallRecord.Load(target)!;
        var files = r.Files.Where(f => f.Component == Components.Dwell).ToList();
        Eq(447, files.Count, "all 447 files recorded");
        Eq(Dwell.InstalledSize, files.Sum(f => f.Size), "their size is Dwell.InstalledSize");
        var folder = Path.Combine(target, "dwell");
        Eq(Dwell.ProgsSha256, PackageManifest.HashFile(Path.Combine(folder, "progs.dat")), "progs.dat is v2.2's (the pinned hash)");
        True(File.Exists(Path.Combine(folder, Dwell.ReadmeFile)) && File.Exists(Path.Combine(folder, "README_changelist.txt")), "READMEs kept");
        True(Directory.EnumerateFiles(Path.Combine(folder, "music")).Select(Path.GetFileName).Contains("track107.ogg", StringComparer.Ordinal), "track107.ogg lower case");
        True(!Directory.EnumerateFiles(Path.Combine(folder, "music")).Select(Path.GetFileName).Contains("Track107.ogg", StringComparer.Ordinal), "and no capitalised one");
        // The installed copy is what "I already have it" accepts, from the Quake VR folder's root.
        Eq(ModCopyState.Ready, Dwell.FindExisting(Dwell.SearchRoots([], target))!.State, "the real hash validates the real install");
        Uninstaller.Uninstall(target, new UninstallOptions());
        True(!Directory.Exists(folder), "uninstall removed dwell\\");
    }),
    ("quoth: the pin (built in = support_assets.json's thirdParty = Quaddicted's two URLs in order), on by default, needs AD, the feed's quoth wins", () =>
    {
        var pinned = BuiltInComponents.Quoth();
        Eq("quoth2pt2full_2.zip", pinned.File, "pinned file");
        Eq(54_792_367L, pinned.Size, "pinned size");
        Eq("9fce1af35ef7f635990be949d75f7faa18cc198bc19d9adc7d0575e234ab1139", pinned.Sha256, "pinned SHA-256");
        Eq("www.quaddicted.com|www.quaddicted.com", string.Join("|", pinned.Mirrors.Select(u => u.Host)), "hosts in order");
        True(pinned.Urls[0].EndsWith("/by-sha256/9f/" + pinned.Sha256 + "/quoth2pt2full_2.zip", StringComparison.Ordinal), "content-addressed first");
        True(pinned.Urls[1].EndsWith("/filebase/quoth2pt2full_2.zip", StringComparison.Ordinal), "the filebase second");
        True(pinned.Urls.All(u => u.StartsWith("https://", StringComparison.Ordinal) && !u.Contains("github.com")), "Quaddicted only, never ours (its licence)");
        True(new InstallerSettings().Component(null, Components.Quoth) is { BuiltIn: true } d && d.File.Sha256 == pinned.Sha256, "the defaults: the pin");
        True(Quoth.OnByDefault && Quoth.NeedsAdText.Contains("Arcane Dimensions"), "on by default, with AD; the reason without it");
        var repo = new DirectoryInfo(AppContext.BaseDirectory);
        while (repo is not null && !File.Exists(Path.Combine(repo.FullName, "Misc", "release", "support_assets.json")))
        {
            repo = repo.Parent;
        }
        if (repo is not null)
        {
            using var doc = System.Text.Json.JsonDocument.Parse(File.ReadAllText(Path.Combine(repo.FullName, "Misc", "release", "support_assets.json")));
            True(!doc.RootElement.GetProperty("files").TryGetProperty("quoth", out _), "Quoth is not one of our hosted support files");
            var j = doc.RootElement.GetProperty("thirdParty").GetProperty("quoth");
            Eq(pinned.File, j.GetProperty("file").GetString(), "support_assets.json's file");
            Eq(pinned.Size, j.GetProperty("size").GetInt64(), "support_assets.json's size");
            Eq(pinned.Sha256, j.GetProperty("sha256").GetString(), "support_assets.json's SHA-256");
            Eq(string.Join("|", pinned.Urls), string.Join("|", j.GetProperty("mirrors").EnumerateArray().Select(m => m.GetString())), "support_assets.json's hosts");
            // The engine pins the same pak2.pak (vr_quoth_data.inc's VR_QUOTH_PIN_SHA).
            var inc = File.ReadAllText(Path.Combine(repo.FullName, "Quake", "vr", "vr_quoth_data.inc"));
            True(inc.Contains($"VR_QUOTH_PIN_SHA \"{Quoth.Pak2Sha256}\""), "Quoth.Pak2Sha256 = the engine's pin");
        }
        var feed = ReleaseFeed.Parse($$"""
            { "schema": 1, "version": "v9", "components": { "quoth": { "file": "q.zip", "size": 3, "sha256": "{{new string('c', 64)}}", "urls": ["https://example.invalid/q.zip"] } } }
            """);
        True(new InstallerSettings().Component(feed, Components.Quoth) is { BuiltIn: false } f && f.File.File == "q.zip", "the feed's quoth wins");
    }),
    ("quoth: an existing copy (2.2 used by its pak2.pak hash, another version or a missing pak offers the download, the game's root order)", () =>
    {
        void MakeQuoth(string root, string pak2, bool all = true)
        {
            var d = Path.Combine(root, "quoth");
            Directory.CreateDirectory(d);
            File.WriteAllText(Path.Combine(d, "pak2.pak"), pak2);
            File.WriteAllText(Path.Combine(d, "pak0.pak"), "pak0");
            if (all)
            {
                File.WriteAllText(Path.Combine(d, "pak1.pak"), "pak1");
            }
        }
        var steam = Dir("qo-steam");
        var quake = Dir("qo-quake");
        True(Quoth.FindExisting(Quoth.SearchRoots([steam], quake)) is null, "none: null");
        MakeQuoth(quake, "2.2 pak2");
        var pin = PackageManifest.HashFile(Path.Combine(quake, "quoth", "pak2.pak"));
        var roots = Quoth.SearchRoots([steam], quake);
        var found = Quoth.FindExisting(roots, pin)!;
        True(found.State == ModCopyState.Ready && Quoth.Describe(found).Contains("will be used (nothing downloaded)"), "2.2 in the Quake folder: used in place");
        var other = Quoth.FindExisting(roots, new string('0', 64))!;
        True(other.State == ModCopyState.OtherVersion && Quoth.Describe(other).Contains("isn't 2.2"), "another pak2.pak: another version, said so");
        var inc = Dir("qo-inc");
        MakeQuoth(inc, "2.2 pak2", all: false);
        Eq(ModCopyState.Incomplete, Quoth.FindExisting(Quoth.SearchRoots([], inc), pin)!.State, "no pak1.pak: incomplete");
        var empty = Dir("qo-empty");
        Directory.CreateDirectory(Path.Combine(empty, "quoth", "maps"));
        True(Quoth.FindExisting(Quoth.SearchRoots([], empty), pin) is null, "a folder with none of its paks: none");
        MakeQuoth(steam, "another pak2");
        True(Quoth.FindExisting(Quoth.SearchRoots([steam], quake), pin)!.Folder.StartsWith(PathUtil.TryNormalize(quake)!, StringComparison.OrdinalIgnoreCase), "the Quake folder's copy wins");
    }),
    ("quoth: the real quoth2pt2full_2.zip from the local downloads folder (no network): install (6 files as released), reinstall kept, repair restores, update keeps, uninstall removes", () =>
    {
        var real = "C:/OHWorkspace/qvr-kit/downloads/modsurvey/quoth/quoth2pt2full_2.zip";
        if (!File.Exists(real))
        {
            Console.WriteLine("    (skipped: the real zip is not in the local downloads folder)");
            return;
        }
        var quake = Dir("qr-quake");
        var target = Path.Combine(run, "qr-QuakeVR");
        new InstallEngine().Install(new InstallPlan { PackagePath = Fixtures.MakePackage(Dir("qr-pkg"), "1.0.0"), TargetDir = target, QuakeDir = quake }, null, CancellationToken.None);
        using var http = Downloader.CreateClient();
        var pin = BuiltInComponents.Quoth();
        var outcome = Quoth.AddAsync(http, target, pin, Dir("qr-downloads"), real, null, null, CancellationToken.None, 1).GetAwaiter().GetResult();
        True(outcome.Installed, outcome.Message);
        var r = InstallRecord.Load(target)!;
        var files = r.Files.Where(f => f.Component == Components.Quoth).ToList();
        Eq("launch.bat|pak0.pak|pak1.pak|pak2.pak|quoth.cfg|quoth2_2.txt", string.Join("|", files.Select(f => f.Path["quoth/".Length..]).Order(StringComparer.Ordinal)), "the zip's six files under quoth/");
        Eq(Quoth.InstalledSize, files.Sum(f => f.Size), "their size is Quoth.InstalledSize");
        True(r.Choices.Quoth && r.QuothSha256 == pin.Sha256 && r.QuothFile == pin.File && !r.Choices.ArcaneDimensions, "recorded: chosen, the zip's name and SHA-256; AD untouched");
        var folder = Path.Combine(target, "quoth");
        Eq(Quoth.Pak2Sha256, PackageManifest.HashFile(Path.Combine(folder, "pak2.pak")), "pak2.pak is 2.2's (the engine's pin)");
        Eq(ModCopyState.Ready, Quoth.FindExisting(Quoth.SearchRoots([], target))!.State, "the real install is a usable 2.2 copy");
        // Again (the Update screen's repair): same zip kept; a damaged file restored from it.
        Eq(ModAction.Keep, Quoth.Plan(target, r, pin, verify: true).Action, "same zip, intact: kept");
        File.WriteAllText(Path.Combine(folder, "quoth.cfg"), "damaged");
        Eq(ModAction.Restore, Quoth.Plan(target, r, pin, verify: true).Action, "a damaged file: restored");
        var again = Quoth.AddAsync(http, target, pin, Dir("qr-downloads"), real, null, null, CancellationToken.None, 1).GetAwaiter().GetResult();
        True(again.Installed && PackageManifest.HashFile(Path.Combine(folder, "quoth.cfg")) == files.Single(f => f.Path == "quoth/quoth.cfg").Sha256, "reinstalled: " + again.Message);
        var r2 = new InstallEngine().Install(new InstallPlan { Mode = InstallMode.Update, PackagePath = Fixtures.MakePackage(Dir("qr-pkg2"), "1.1.0"), TargetDir = target, QuakeDir = quake }, null, CancellationToken.None);
        True(r2.Files.Count(f => f.Component == Components.Quoth) == 6 && r2.Choices.Quoth && r2.QuothSha256 == pin.Sha256, "a Quake VR update keeps Quoth recorded");
        Uninstaller.Uninstall(target, new UninstallOptions());
        True(!Directory.Exists(folder) && !Directory.Exists(target), "uninstall removed quoth\\ and the folder");
    }),
    ("reliefs: images and packing (the engine's hash contract, PNG round trip, TGA bottom-left and RLE, normals, calibrated height)", () =>
    {
        var flat = new byte[3 * 2 * 4];
        for (var i = 3; i < flat.Length; i += 4)
        {
            flat[i] = 255;
        }
        Eq("cde616e3c2a2eae36a3194dc72b99edf279909d7417330ab6ab166948be3a42f", ReliefMaker.ContentHash(3, 2, flat), "pbr_generate.py's self-test digest (SHA-256 of qvr-pbr-v1 NUL, dims, RGBA)");
        var rng = new Random(5);
        var px = new byte[37 * 23 * 4];
        rng.NextBytes(px);
        var back = ReliefImages.DecodePng(ReliefImages.EncodePng(new RgbaImage(37, 23, px)))!;
        True(back.Width == 37 && back.Height == 23 && back.Pixels.AsSpan().SequenceEqual(px), "PNG: encoded then decoded, the same RGBA");
        var tga = ReliefImages.Decode("x.tga", ReliefFixtures.Tga24(3, 2, (x, y) => ((byte)(x * 10), (byte)(y * 20), 7)))!;
        True(tga.Pixels[0] == 0 && tga.Pixels[1] == 0 && tga.Pixels[(1 * 3 + 2) * 4] == 20 && tga.Pixels[(1 * 3 + 2) * 4 + 1] == 20 &&
             tga.Pixels[(1 * 3 + 2) * 4 + 3] == 255, "TGA 24-bit bottom-left: rows top to bottom, opaque");
        var rle = ReliefImages.Decode("x.tga", ReliefFixtures.Tga32Rle(4, 2, 5, (1, 2, 3, 4), (9, 8, 7, 6)))!;
        True(rle.Pixels.AsSpan(4 * 4, 4).SequenceEqual(new byte[] { 1, 2, 3, 4 }) && rle.Pixels.AsSpan(5 * 4, 4).SequenceEqual(new byte[] { 9, 8, 7, 6 }), "TGA 32-bit RLE top-left");
        True(ReliefImages.Decode("x.jpg", [1, 2, 3]) is null, "a JPG is not decoded (no relief for it)");
        var n = ReliefMaker.NormalBytes([0.5f, 0.5f, 1f, 0.5f, 0.5f, 0f], 2);
        True(n[2] == 255 && n[5] == 255 && n[0] == 128, "normal bytes: flat is (128, 128, 255); a backward normal clamps into the hemisphere");
        var pred = Enumerable.Range(0, 100).Select(i => i / 100f).ToArray();
        var height = ReliefMaker.CalibratedHeight(pred, new bool[100], ReliefMaker.Depth);
        True(height.Min() == 128 && height.Max() == 255 && height[99] == 255, "calibrated height: the 5..95 percentile span fills the 0.5 depth bound");
        True(ReliefMaker.CalibratedHeight(new float[100], new bool[100], ReliefMaker.Depth).All(v => v == 255), "a flat prediction stays flat");
        var mask = new bool[9];
        mask[4] = true;
        var filled = ReliefMaker.FillMasked([.. Enumerable.Repeat((byte)77, 36)], mask, 3, 3);
        True(filled.All(v => v == 77), "cutout colours filled from their neighbours");
    }),
    ("reliefs: a small texture set end to end on the CPU (made, skipped: stock, Quake VR's bundle, replaced; animation frames share; holes flat; again: kept; one missing: made; uninstall removes the folder)", () =>
    {
        var quake = Dir("rl-quake");
        var target = Path.Combine(run, "rl-QuakeVR");
        new InstallEngine().Install(new InstallPlan { PackagePath = Fixtures.MakePackage(Dir("rl-pkg"), "1.0.0"), TargetDir = target, QuakeDir = quake }, null, CancellationToken.None);
        var mod = Path.Combine(target, Quoth.Folder);
        Directory.CreateDirectory(Path.Combine(mod, "maps"));
        Directory.CreateDirectory(Path.Combine(mod, "textures"));
        byte Brick(int x, int y) => (byte)((y % 16 == 0 || (x + (y / 16 % 2) * 16) % 32 == 0) ? 20 : 100 + (x * 3 + y * 5) % 40);
        File.WriteAllBytes(Path.Combine(mod, "maps", "test.bsp"), ReliefFixtures.Bsp(
            ("brick", 64, 64, Brick),
            ("+0lit", 32, 32, (x, y) => (byte)(60 + Brick(x, y) / 4)),
            ("+1lit", 32, 32, (x, y) => (byte)(61 + Brick(x, y) / 4)),
            ("{fence", 32, 32, (x, y) => x % 8 < 3 ? (byte)255 : Brick(x, y)),
            ("sky1", 32, 32, Brick), ("clip", 16, 16, Brick), ("*water", 16, 16, Brick),
            ("stockone", 16, 16, (x, y) => (byte)(x * y)),
            ("shipped", 16, 16, (x, y) => (byte)(x + y)),
            ("ovr", 16, 16, (x, y) => (byte)(x ^ y))));
        File.WriteAllBytes(Path.Combine(mod, "textures", "ovr.tga"), ReliefFixtures.Tga24(24, 24, (x, y) => ((byte)(x * 9), (byte)(y * 9), (byte)((x / 4 + y / 4) % 2 * 200))));
        var palette = ReliefFixtures.Palette();
        string Hash(int w, int h, Func<int, int, byte> pixel)
        {
            var rgba = new byte[w * h * 4];
            for (var i = 0; i < w * h; ++i)
            {
                var index = pixel(i % w, i / w);
                (rgba[i * 4], rgba[i * 4 + 1], rgba[i * 4 + 2], rgba[i * 4 + 3]) = (palette[index * 3], palette[index * 3 + 1], palette[index * 3 + 2], 255);
            }
            return ReliefMaker.ContentHash(w, h, rgba);
        }
        var bundle = Directory.CreateDirectory(Path.Combine(target, "quakevr", "textures", "pbr")).FullName;
        File.WriteAllText(Path.Combine(bundle, "stock.txt"), "qvr-pbr-stock-v1\n" + Hash(16, 16, (x, y) => (byte)(x * y))[..32] + "\n");
        File.WriteAllBytes(Path.Combine(bundle, Hash(16, 16, (x, y) => (byte)(x + y))[..32] + "_norm.png"), [0]);
        var summary = ReliefMaker.Run(target, [mod], [Path.Combine(target, "quakevr")], palette, forceCpu: true, null, CancellationToken.None);
        Eq("8 textures, 5 made, 3 not needed, 0 failed, CPU", $"{summary.Textures} textures, {summary.Made} made, {summary.NotNeeded} not needed, {summary.Failed} failed, {summary.Device}", summary.Describe());
        var outDir = ReliefMaker.TextureDir(target);
        var lines = File.ReadAllLines(Path.Combine(outDir, "provenance.txt"));
        Eq("qvr-pbr-provenance-v1", lines[0], "the engine's provenance header");
        Eq(5, lines.Length - 1, "one provenance record a companion");
        foreach (var line in lines.Skip(1))
        {
            var parts = line.Split(' ');
            var png = ReliefImages.DecodePng(File.ReadAllBytes(Path.Combine(outDir, parts[0] + "_norm.png")))!;
            Eq(parts[1], ReliefMaker.ContentHash(png.Width, png.Height, png.Pixels), $"{parts[0]}: the record binds the decoded companion");
            Eq("3", parts[2], "generated normal and height");
            var worst = 0.0;
            for (var i = 0; i < png.Width * png.Height; ++i)
            {
                double x = png.Pixels[i * 4] / 127.5 - 1, y = png.Pixels[i * 4 + 1] / 127.5 - 1, z = png.Pixels[i * 4 + 2] / 127.5 - 1;
                worst = Math.Max(worst, Math.Abs(Math.Sqrt(x * x + y * y + z * z) - 1) + (z < 0 ? 1 : 0));
                True(png.Pixels[i * 4 + 3] >= 127, "height within the 0.5 depth bound");
            }
            True(worst <= .02, $"{parts[0]}: unit normals in the hemisphere ({worst:0.000})");
        }
        string Companion(string hash) => Path.Combine(outDir, hash[..32] + "_norm.png");
        var fence = ReliefImages.DecodePng(File.ReadAllBytes(Companion(Hash(32, 32, (x, y) => x % 8 < 3 ? (byte)255 : Brick(x, y)))))!;
        True(fence.Pixels.AsSpan(0, 4).SequenceEqual(new byte[] { 128, 128, 255, 255 }), "a fence's holes are flat");
        True(File.ReadAllBytes(Companion(Hash(32, 32, (x, y) => (byte)(60 + Brick(x, y) / 4)))).AsSpan()
             .SequenceEqual(File.ReadAllBytes(Companion(Hash(32, 32, (x, y) => (byte)(61 + Brick(x, y) / 4))))), "a lighting-only animation frame shares its first frame's relief");
        True(!File.Exists(Companion(Hash(16, 16, (x, y) => (byte)(x ^ y)))), "a miptex its TGA replaces gets none (the engine draws the TGA)");
        True(Directory.GetFiles(outDir, "*_norm.png").Any(p => ReliefImages.DecodePng(File.ReadAllBytes(p))!.Width == 24), "the TGA replacement got one");
        var again = ReliefMaker.Run(target, [mod], [Path.Combine(target, "quakevr")], palette, forceCpu: true, null, CancellationToken.None);
        Eq("0 made, 5 kept", $"{again.Made} made, {again.Kept} kept", "again: nothing made");
        File.Delete(Companion(Hash(64, 64, Brick)));
        var third = ReliefMaker.Run(target, [mod], [Path.Combine(target, "quakevr")], palette, forceCpu: true, null, CancellationToken.None);
        Eq("1 made, 4 kept, 5 records", $"{third.Made} made, {third.Kept} kept, {File.ReadAllLines(Path.Combine(outDir, "provenance.txt")).Length - 1} records", "a missing one: made again, the index complete");
        Directory.Delete(mod, true);
        Directory.Delete(Path.Combine(target, "quakevr", "textures"), true);
        Uninstaller.Uninstall(target, new UninstallOptions());
        True(!Directory.Exists(ReliefMaker.Folder(target)) && !Directory.Exists(target), "uninstall removed the reliefs folder and the install");
    }),
    ("reliefs-only: an existing folder like a developer's (Quake with quakevr, dwellv2p2 and quoth; no install.json): offered, makes them, writes only reliefs; a second run makes 0; Setup's own install offered; folders without Quake VR or the mods not", () =>
    {
        var quake = Dir("ro-Quake");
        Directory.CreateDirectory(Path.Combine(quake, "id1", "gfx"));
        File.WriteAllBytes(Path.Combine(quake, "id1", "gfx", "palette.lmp"), ReliefFixtures.Palette());
        Directory.CreateDirectory(Path.Combine(quake, "quakevr", "textures", "pbr"));
        byte Stone(int x, int y) => (byte)(90 + (x * 7 + y * 3) % 30 + ((x / 8 + y / 8) % 2) * 20);
        foreach (var (mod, name, seed) in new[] { (Dwell.AltFolder, "d", 1), (Quoth.Folder, "q", 2) })
        {
            Directory.CreateDirectory(Path.Combine(quake, mod, "maps"));
            File.WriteAllBytes(Path.Combine(quake, mod, "maps", name + ".bsp"), ReliefFixtures.Bsp(
                ($"{name}wall", 32, 32, (x, y) => (byte)(Stone(x, y) + seed)),
                ($"{name}floor", 16, 16, (x, y) => (byte)(40 + (x ^ y) + seed))));
        }
        string[] Outside() => [.. Directory.EnumerateFileSystemEntries(quake, "*", SearchOption.AllDirectories)
            .Where(p => !p.StartsWith(ReliefMaker.Folder(quake), StringComparison.OrdinalIgnoreCase))
            .Select(p => $"{p} {(File.Exists(p) ? new FileInfo(p).Length : -1)}").Order()];
        var before = Outside();
        var plain = Dir("ro-plain-quake");
        Directory.CreateDirectory(Path.Combine(plain, Quoth.Folder, "maps"));
        var noMods = Dir("ro-no-mods");
        Directory.CreateDirectory(Path.Combine(noMods, "quakevr"));
        var found = ReliefTarget.Find([(plain, null), (noMods, null), (quake, null), (quake, null)]);
        Eq(1, found.Count, "only the folder with quakevr and a mod is offered, once");
        var t = found[0];
        Eq("Dwell and Quoth", t.ModsText, "both mods found");
        Eq($"{Path.Combine(quake, Dwell.AltFolder)};{Path.Combine(quake, Quoth.Folder)}", string.Join(";", t.ModDirs), "dwellv2p2 and quoth found by name");
        True(!t.OwnInstall && t.QuakeVrDir is not null && t.Existing == 0, "a hand install: quakevr, no install.json, no reliefs yet");
        var first = t.Run(forceCpu: true, null, CancellationToken.None);
        Eq("4 made, 0 already existed", ReliefTarget.Outcome(first), first.Describe());
        Eq(4, ReliefTarget.For(quake).Existing, "the folder now has them");
        True(before.SequenceEqual(Outside()), "nothing outside <folder>\\reliefs was written or changed");
        var second = ReliefTarget.For(quake).Run(forceCpu: true, null, CancellationToken.None);
        Eq("0 made, 4 already existed", ReliefTarget.Outcome(second), "a second run makes none");
        // Setup's own install: offered from its install.json (dwell before dwellv2p2 when both are there).
        var own = Path.Combine(run, "ro-QuakeVR");
        new InstallEngine().Install(new InstallPlan { PackagePath = Fixtures.MakePackage(Dir("ro-pkg"), "1.0.0"), TargetDir = own, QuakeDir = quake }, null, CancellationToken.None);
        Directory.CreateDirectory(Path.Combine(own, Dwell.Folder, "maps"));
        Directory.CreateDirectory(Path.Combine(own, Dwell.AltFolder, "maps"));
        var mine = ReliefTarget.For(own);
        True(mine.Offered && mine.OwnInstall, "Setup's install with Dwell is offered");
        Eq(Path.Combine(own, Dwell.Folder), mine.ModDirs.Single(), "dwell wins over dwellv2p2");
        True(mine.PaletteDirs.Contains(Path.Combine(quake, "id1")), "the install record's Quake id1 is where the palette is read");
    }),
    ("arcane dimensions: a failed or cancelled download never fails the install", () =>
    {
        using var server = new LocalHttpServer();
        var quake = Dir("adf-quake");
        var target = Path.Combine(run, "adf-QuakeVR");
        new InstallEngine().Install(new InstallPlan { PackagePath = Fixtures.MakePackage(Dir("adf-pkg"), "1.0.0"), TargetDir = target, QuakeDir = quake }, null, CancellationToken.None);
        var before = File.ReadAllText(Path.Combine(target, InstallRecord.FileName));
        using var http = Downloader.CreateClient();
        var missing = new FeedFile { File = "ad_v1_80p1final.zip", Size = 1000, Sha256 = new string('e', 64), Urls = [server.Url("gone/ad_v1_80p1final.zip").ToString()] };
        var failed = ArcaneDimensions.AddAsync(http, target, missing, Dir("adf-dl"), null, null, null, CancellationToken.None, 1).GetAwaiter().GetResult();
        True(!failed.Installed && !failed.Cancelled, "not installed");
        True(failed.Message.Contains("Quake VR itself is installed") && failed.Message.Contains("run Setup again"), failed.Message);
        Eq(before, File.ReadAllText(Path.Combine(target, InstallRecord.FileName)), "install.json untouched");
        True(!Directory.Exists(Path.Combine(target, "ad")) && File.Exists(Path.Combine(target, "ironwail.exe")), "no ad folder; the game still installed");
        using var cts = new CancellationTokenSource();
        cts.Cancel();
        var cancelled = ArcaneDimensions.AddAsync(http, target, missing, Dir("adf-dl2"), null, null, null, cts.Token, 1).GetAwaiter().GetResult();
        True(cancelled.Cancelled && !cancelled.Installed && cancelled.Message.Contains("cancelled"), cancelled.Message);
        Eq(before, File.ReadAllText(Path.Combine(target, InstallRecord.FileName)), "install.json untouched after a cancel");
    }),
    ("sounds: the mixer never steps the output (voice fades, stolen voices, the loop's seam, the mute, a smooth limiter)", () =>
    {
        // The limiter: unchanged below the knee, continuous (and so is its slope) at the knee, never above full scale.
        Eq(0.5f, SoundMixer.Limit(0.5f), "below the knee");
        Eq(-0.5f, SoundMixer.Limit(-0.5f), "below the knee, negative");
        const float k = SoundMixer.KneeStart;
        True(Math.Abs(SoundMixer.Limit(k + 1e-4f) - SoundMixer.Limit(k - 1e-4f)) < 3e-4, "continuous at the knee");
        var previous = 0f;
        for (var x = 0f; x < 8; x += 0.001f)
        {
            var y = SoundMixer.Limit(x);
            True(y >= previous && y <= 1 && y - previous <= 0.001f + 1e-6f, $"monotonic, below 1, slope at most 1 at {x}");
            previous = y;
        }
        True(SoundMixer.Limit(1) > 0.88f && SoundMixer.Limit(1) < 0.92f, "1 bends to about 0.9");

        // Quake's 8-bit sounds rarely start or end on zero: a clip of pure DC (0.5) is the worst case.
        var dc = new SoundClip("dc", Enumerable.Repeat(0.5f, SoundMixer.Rate / 4).ToArray());
        short[] Render(SoundMixer m, int frames, Action<int>? at = null)
        {
            const int block = 441;
            var pcm = new short[frames / block * block];
            for (var f = 0; f < pcm.Length; f += block)
            {
                at?.Invoke(f);
                m.Mix(pcm.AsSpan(f, block));
            }
            return pcm;
        }
        var mixer = new SoundMixer();
        mixer.Play(dc, 1);
        var one = Render(mixer, SoundMixer.Rate / 2);
        var a = SoundMixer.Analyze(one);
        True(a.Peak > 0.48, $"the clip plays (peak {a.Peak})");
        True(a.MaxJump < 0.5 / SoundMixer.FadeInFrames * 1.5, $"fades in and out (largest jump {a.MaxJump})");
        True(!mixer.Busy, "ended");
        Eq(0, mixer.Edges, "no edges");

        // Five plays of one sound 20 ms apart: two voices are stolen, and fade out.
        mixer = new SoundMixer();
        var stolen = Render(mixer, SoundMixer.Rate / 2, f =>
        {
            if (f % 882 == 0 && f < 882 * 5)
            {
                mixer.Play(dc, 0.2f);
            }
        });
        a = SoundMixer.Analyze(stolen);
        True(a.MaxJump < 0.2 / SoundMixer.FadeInFrames * 1.5 + 0.2 / SoundMixer.StealFrames * 3, $"stolen voices fade (largest jump {a.MaxJump})");
        Eq(0, mixer.Edges, "no edges with stolen voices");

        // A loop whose end and start differ (a ramp from -0.5 to 0.5): no seam; then the mute ramps.
        var ramp = new SoundClip("ramp", Enumerable.Range(0, SoundMixer.Rate / 5).Select(i => i / (float)(SoundMixer.Rate / 5) - 0.5f).ToArray());
        mixer = new SoundMixer();
        mixer.SetLoop(ramp, 1, 0.001);
        var muted = false;
        var loop = Render(mixer, SoundMixer.Rate, f =>
        {
            if (!muted && f >= SoundMixer.Rate * 3 / 4)
            {
                mixer.MasterVolume = 0;
                muted = true;
            }
        });
        a = SoundMixer.Analyze(loop);
        True(a.MaxJump < 0.01, $"the loop wraps and mutes without a step (largest jump {a.MaxJump})");
        True(!mixer.Busy, "muted: nothing to send");
        Eq(0, loop[^1], "silent once muted");
        Eq(0, mixer.Edges, "no edges in the loop");

        // The mute at Play: a 0.3 s fade (the installer's MuteForGame), heard half-way, silent at its end.
        mixer = new SoundMixer();
        mixer.SetLoop(ramp, 1, 0.001);
        Render(mixer, SoundMixer.Rate / 10, _ => { });
        mixer.MasterFadeSeconds = 0.3;
        mixer.MasterVolume = 0;
        var half = Render(mixer, SoundMixer.Rate * 15 / 100, _ => { });
        var rest = Render(mixer, SoundMixer.Rate * 20 / 100, _ => { });
        True(half.Skip(half.Length - 441).Any(x => x != 0), "the fade is still heard half-way");
        True(rest.Skip(rest.Length - SoundMixer.Rate * 4 / 100).All(x => x == 0), "silent once the fade ends");
        True(Math.Max(SoundMixer.Analyze(half).MaxJump, SoundMixer.Analyze(rest).MaxJump) < 0.01, "the fade has no step");
        True(!mixer.Busy, "faded out: nothing to send");

        // The loop's seam itself: the crossfaded copy's last sample flows into its first.
        var seamless = SoundMixer.Seamless(ramp.Samples);
        Eq(ramp.Samples.Length - SoundMixer.SeamFrames, seamless.Length, "seamless length");
        True(Math.Abs(seamless[0] - seamless[^1]) < 0.01, $"seam {seamless[^1]} -> {seamless[0]}");

        // Rendered to a WAV that reads back.
        var wav = Path.Combine(Dir("sounds"), "mix.wav");
        SoundMixer.WriteWav(wav, stolen);
        var back = QuakeFormats.ReadWav(File.ReadAllBytes(wav));
        True(back is not null && back.SampleRate == SoundMixer.Rate && back.Samples.Length == stolen.Length, "the WAV reads back");
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
    ("vispatch: downloaded from a mirror list, pinned hash, .tgz unpacked safely, installed and recorded, removed on uninstall", () =>
    {
        // Made-up archives in VisPatch's layout (id1.vis at the top; a pack's <game>/vispatch.dat; a readme; an entry
        // trying to leave the folder), served by a local server: no real download.
        byte[] Tgz(params (string Name, byte[] Data)[] entries)
        {
            using var ms = new MemoryStream();
            using (var gz = new System.IO.Compression.GZipStream(ms, System.IO.Compression.CompressionLevel.Fastest, leaveOpen: true))
            using (var tar = new System.Formats.Tar.TarWriter(gz, System.Formats.Tar.TarEntryFormat.Ustar))
            {
                foreach (var (name, data) in entries)
                {
                    tar.WriteEntry(new System.Formats.Tar.UstarTarEntry(System.Formats.Tar.TarEntryType.RegularFile, name) { DataStream = new MemoryStream(data) });
                }
            }
            return ms.ToArray();
        }
        var id1 = Tgz(("id1.vis", RandomNumberGenerator.GetBytes(5000)), ("../evil.vis", [1, 2, 3]), ("readme.txt", [4]));
        var hip = Tgz(("hipnotic/vispatch.dat", RandomNumberGenerator.GetBytes(3000)));
        var rogue = Tgz(("rogue.vis", RandomNumberGenerator.GetBytes(4000)), ("rogue.txt", [5]));
        string Sha(byte[] b) => Convert.ToHexStringLower(SHA256.HashData(b));
        var archives = new List<VisPatchArchive>
        {
            new("id1", "id1_vis.tgz", id1.Length, Sha(id1)),
            new("hipnotic", "hipnotic_vis.tgz", hip.Length, Sha(hip)),
            new("rogue", "rogue_vis.tgz", rogue.Length, Sha(rogue)),
        };
        using var server = new LocalHttpServer();
        server.Serve("sf/id1_vis.tgz", id1);
        server.Serve("sf/hipnotic_vis.tgz", hip);
        server.Serve("sf/rogue_vis.tgz", rogue);
        Eq("id1|rogue", string.Join("|", VisPatch.For(["rogue"], archives).Select(a => a.Game)), "id1 and the owned packs only");
        var templates = new[] { server.Url("dead/{file}").ToString().Replace("%7B", "{").Replace("%7D", "}"),
                                server.Url("sf/{file}").ToString().Replace("%7B", "{").Replace("%7D", "}") };
        using var http = Downloader.CreateClient();
        var downloads = Dir("vis-dl");
        var paths = new List<string>();
        foreach (var a in VisPatch.For(["hipnotic", "rogue"], archives))
        {
            var dest = Path.Combine(downloads, a.File);
            new Downloader(http).DownloadAsync(VisPatch.Mirrors(a, templates), dest, a.Size, a.Sha256, null, CancellationToken.None, attemptsPerMirror: 1)
                .GetAwaiter().GetResult();
            True(VisPatch.Matches(dest, a), $"{a.File} checked");
            paths.Add(dest);
        }
        Eq("quakevr/tools/vispatch/id1.vis", string.Join("|", VisPatch.Extract(paths[0]).Select(f => f.Relative)), "only the data, nothing outside");
        var quake = Dir("vis-quake");
        var target = Path.Combine(run, "vis-QuakeVR");
        var r = new InstallEngine().Install(new InstallPlan
        {
            PackagePath = Fixtures.MakePackage(Dir("vis-pkg"), "v1"), TargetDir = target, QuakeDir = quake, RelightOnFirstRun = true, VisPatchArchives = paths,
        }, null, CancellationToken.None);
        var vis = r.Files.Where(f => f.Component == Components.VisPatch).Select(f => f.Path).Order().ToList();
        Eq("quakevr/tools/vispatch/hipnotic/vispatch.dat|quakevr/tools/vispatch/id1.vis|quakevr/tools/vispatch/rogue.vis", string.Join("|", vis), "installed where the game looks");
        True(r.Choices.VisPatch, "recorded as chosen");
        True(!File.Exists(Path.Combine(target, "quakevr", "tools", "vispatch", "rogue.txt")) && !File.Exists(Path.Combine(target, "quakevr", "tools", "evil.vis")), "nothing else");
        // An update without the archives keeps the data (still recorded); uninstall removes it.
        var u = new InstallEngine().Install(new InstallPlan
        {
            PackagePath = Fixtures.MakePackage(Dir("vis-pkg2"), "v2"), TargetDir = target, QuakeDir = quake,
        }, null, CancellationToken.None);
        Eq(3, u.Files.Count(f => f.Component == Components.VisPatch), "kept by an update");
        Uninstaller.Uninstall(target, new UninstallOptions());
        True(!File.Exists(Path.Combine(target, "quakevr", "tools", "vispatch", "id1.vis")), "removed on uninstall");
        // A damaged archive is refused, not half-read.
        var bad = Path.Combine(downloads, "bad_vis.tgz");
        File.WriteAllBytes(bad, id1[..(id1.Length / 2)]);
        Throws<InvalidDataException>(() => VisPatch.Extract(bad), "truncated archive");
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
    ("update detection: none on a fresh machine; the folder given, Setup's copy, the Apps & Features entry, the default folder; a moved entry", () =>
    {
        var registryFile = Path.Combine(run, "det-registry.json");
        var registry = new JsonFileRegistry(registryFile);
        var defaultDir = Path.Combine(run, "det-default", "QuakeVR");
        var fresh = InstallDetection.Find(registry, null, Path.Combine(run, "det-setup", "QuakeVR-Setup.exe"), defaultDir);
        True(fresh.Picked is null && fresh.Found.Count == 0 && fresh.Notes.Count == 0, "a fresh machine: no install");
        True(fresh.Format().Contains("not installed"), "printed: " + fresh.Format());

        var quake = Dir("det-quake");
        Fixtures.MakeOriginal(quake);
        var setupDir = Dir("det-setup-build");
        File.WriteAllText(Path.Combine(setupDir, "QuakeVR-Setup.exe"), "exe");
        var registered = Path.Combine(run, "det-QuakeVR");
        new InstallEngine().Install(new InstallPlan
        {
            PackagePath = Fixtures.MakePackage(Dir("det-pkg"), "0.9.0 (2026-10-01 aaaaaaa1)"), TargetDir = registered, QuakeDir = quake,
            SetupFiles = SetupCopy.FilesOf(Path.Combine(setupDir, "QuakeVR-Setup.exe"), singleFile: true), Registry = registry,
        }, null, CancellationToken.None);
        var d = InstallDetection.Find(registry, null, null, defaultDir);
        Eq(registered, d.Picked?.Dir, "the registered install");
        Eq(InstallOrigin.Registered, d.Picked!.Origin, "found from the entry");
        Eq("0.9.0 (2026-10-01 aaaaaaa1)", d.RegisteredVersion, "the entry's version");

        new InstallEngine().Install(new InstallPlan { PackagePath = Fixtures.MakePackage(Dir("det-pkg2"), "0.9.0 (2026-10-01 aaaaaaa1)"), TargetDir = defaultDir, QuakeDir = quake },
            null, CancellationToken.None);
        d = InstallDetection.Find(registry, null, null, defaultDir);
        Eq(2, d.Found.Count, "two installs");
        Eq(registered, d.Picked!.Dir, "the registered one is picked over the default folder's");
        True(d.Format().Contains("<- picked") && d.Format().Contains(defaultDir), "both printed");
        Eq(defaultDir, InstallDetection.Find(registry, defaultDir, null, defaultDir).Picked!.Dir, "the folder given wins");
        var running = InstallDetection.Find(registry, null, Path.Combine(defaultDir, "setup", "QuakeVR-Setup.exe"), null);
        Eq(defaultDir, running.Picked!.Dir, "Setup's copy in an install: that install");
        Eq(InstallOrigin.RunningFrom, running.Picked.Origin, "found from the copy");

        // Moved by hand: the entry points at an empty place; the default folder's is picked, and the moved one is found by browsing.
        var moved = Path.Combine(run, "det-moved");
        Directory.Move(registered, moved);
        d = InstallDetection.Find(registry, null, null, defaultDir);
        Eq(defaultDir, d.Picked!.Dir, "the default folder's install");
        True(d.Notes.Any(n => n.Contains("moved or deleted")), "the moved entry is said");
        Eq(moved, InstallDetection.Find(registry, moved, null, defaultDir).Picked!.Dir, "browsed to");

        var broken = Dir("det-broken");
        File.WriteAllText(Path.Combine(broken, InstallRecord.FileName), "{ not json");
        d = InstallDetection.Find(null, broken, null, null);
        True(d.Picked is null && d.Notes.Any(n => n.Contains("could not be read")), "an unreadable install.json is passed over");
    }),
    ("versions: releases, pre-releases, builds and the old stamps ordered; update or repair", () =>
    {
        Eq(VersionOrder.Newer, ReleaseVersion.Compare("0.9.0 (2026-10-08 aaaaaaa1)", "0.9.1 (2026-10-09 bbbbbbb2)"), "a newer release");
        Eq(VersionOrder.Older, ReleaseVersion.Compare("0.9.1 (2026-10-09 bbbbbbb2)", "0.9.0 (2026-10-08 aaaaaaa1)"), "an older one");
        Eq(VersionOrder.Same, ReleaseVersion.Compare("0.9.1 (2026-10-09 bbbbbbb2)", "0.9.1 (2026-10-09 bbbbbbb2)"), "the same");
        Eq(VersionOrder.Same, ReleaseVersion.Compare("0.9.1 (2026-10-09 bbbbbbb2-dirty)", "0.9.1 (2026-10-09 bbbbbbb2)"), "the same commit");
        Eq(VersionOrder.Older, ReleaseVersion.Compare("0.10.0", "0.9.9"), "numbers, not text");
        Eq(VersionOrder.Newer, ReleaseVersion.Compare("0.9.1-dev (2026-10-09 ccccccc3)", "0.9.1 (2026-10-08 bbbbbbb2)"), "a release after its pre-release");
        Eq(VersionOrder.Older, ReleaseVersion.Compare("0.9.1 (2026-10-08 bbbbbbb2)", "0.9.1-dev (2026-10-09 ccccccc3)"), "a pre-release before its release");
        Eq(VersionOrder.Newer, ReleaseVersion.Compare("0.9.1-dev (2026-10-08 aaaaaaa1)", "0.9.1-dev (2026-10-09 bbbbbbb2)"), "a later dev build");
        Eq(VersionOrder.Other, ReleaseVersion.Compare("0.9.1-dev (2026-10-09 aaaaaaa1)", "0.9.1-dev (2026-10-09 bbbbbbb2)"), "another build of the same day");
        Eq(VersionOrder.Newer, ReleaseVersion.Compare("2026-10-06 c131f4bf", "0.9.0 (2026-10-01 aaaaaaa1)"), "numbered after the old stamps");
        Eq(VersionOrder.Newer, ReleaseVersion.Compare("2026-10-06 c131f4bf", "2026-10-07 d131f4bf"), "old stamps by date");
        Eq(VersionOrder.Newer, ReleaseVersion.Compare("1.0.0-beta.9", "1.0.0-beta.10"), "pre-release parts by number");
        Eq(VersionOrder.Newer, ReleaseVersion.Compare("1.0.0-beta.2", "1.0.0-rc.1"), "beta before rc");
        Eq("0.9.1-dev", ReleaseVersion.Parse("0.9.1-dev (2026-10-09 bbbbbbb2)").Short, "short form");
        Eq(InstallMode.Update, MaintenancePlanner.ModeFor("0.9.0", "0.9.1"), "newer: update");
        Eq(InstallMode.Update, MaintenancePlanner.ModeFor("0.9.1-dev (2026-10-09 aaaaaaa1)", "0.9.1-dev (2026-10-09 bbbbbbb2)"), "another build: update");
        Eq(InstallMode.Repair, MaintenancePlanner.ModeFor("0.9.1", "0.9.1"), "same: repair");
        Eq(InstallMode.Repair, MaintenancePlanner.ModeFor("0.9.1", "0.9.0"), "older: repair");
    }),
    ("update: only the program files that differ are copied, the player's files untouched (hashed before and after), changed ones backed up", () =>
    {
        var quake = Dir("upd-quake");
        Fixtures.MakeOriginal(quake);
        var quakeBefore = Snapshot(quake);
        var target = Path.Combine(run, "upd-QuakeVR");
        var shortcuts = Dir("upd-shortcuts");
        var options = new ShortcutOptions { DesktopDir = Path.Combine(shortcuts, "Desktop"), StartMenuDir = Path.Combine(shortcuts, "Programs") };
        var registryFile = Path.Combine(run, "upd-registry.json");
        var setupExe = Path.Combine(Dir("upd-setup"), "QuakeVR-Setup.exe");
        File.WriteAllText(setupExe, "setup exe");
        const string v1 = "0.9.0 (2026-10-01 aaaaaaa1)", v2 = "0.9.1 (2026-10-08 bbbbbbb2)", v3 = "0.9.2 (2026-10-09 ccccccc3)";
        var r1 = new InstallEngine().Install(new InstallPlan
        {
            PackagePath = Fixtures.MakePackage(Dir("upd-pkg1"), v1), TargetDir = target, QuakeDir = quake, Shortcuts = options, RelightOnFirstRun = true,
            Registry = new JsonFileRegistry(registryFile), SetupFiles = SetupCopy.FilesOf(setupExe, singleFile: true),
        }, null, CancellationToken.None);
        FirstStartRelight.Set(target, false); // the game started and relit (its marker gone)

        // The player plays: settings, saves, screenshots, voice notes, Map Library maps, relit maps, ticks, caches...
        var playerFiles = new Dictionary<string, string>
        {
            ["quakevr/ironwail.cfg"] = "player config", ["quakevr/autoexec.cfg"] = "bind x", ["quakevr/s0.sav"] = "save",
            ["quakevr/autosave/auto1.sav"] = "auto", ["quakevr/screenshots/shot0.png"] = "png", ["quakevr/notes/note1.wav"] = "wav",
            ["qvr_addons/0123456789abcdef/maps/mymap.bsp"] = "bsp", ["cache/maps_installed.txt"] = "qvr_addons/0123456789abcdef/maps/mymap.bsp",
            ["cache/maps/0123.zip"] = "zip", ["quakevr/relit_custom/id1/maps/e1m1.relight"] = "relit", ["quakevr/checklist_ticks.txt"] = "ticks",
            ["quakevr/bodycal/body.cfg"] = "height 1.8", ["quakevr/retro_overrides.txt"] = "retro",
        };
        foreach (var (rel, body) in playerFiles)
        {
            var p = Path.Combine(target, rel.Replace('/', '\\'));
            Directory.CreateDirectory(Path.GetDirectoryName(p)!);
            File.WriteAllText(p, body);
        }
        File.AppendAllText(Path.Combine(target, "quakevr", "vr_defaults.cfg"), "\nvr_default fov 110"); // a shipped file the player changed
        File.WriteAllText(Path.Combine(target, "quakevr", "newfile.txt"), "mine"); // the player's file where v2 ships one
        Dictionary<string, string> Hashes() => playerFiles.Keys.ToDictionary(k => k, k => PackageManifest.HashFile(Path.Combine(target, k.Replace('/', '\\'))));
        var before = Hashes();
        var userBefore = UserData.Find(target, r1);
        Eq("4 settings, 2 saves, 2 installed-map files, 4 personal, 2 other", UserData.Summary(userBefore), "the player's files sorted");
        var hubTime = File.GetLastWriteTimeUtc(Path.Combine(target, "quakevr", "maps", "vrhub.bsp"));

        // v2: a new engine and progs (their text has the version), SDL2.dll dropped, newfile.txt added.
        var pkg2 = Fixtures.MakePackage(Dir("upd-pkg2"), v2, new() { ["SDL2.dll"] = "", ["quakevr/newfile.txt"] = "new" });
        var manifest2 = PackageManifest.Parse(File.ReadAllText(Path.Combine(pkg2, PackageManifest.FileName)));
        var plan = MaintenancePlanner.Plan(target, InstallRecord.Load(target)!, manifest2, MaintenancePlanner.ModeFor(r1.Version, v2));
        Eq(InstallMode.Update, plan.Mode, "an older install: update");
        string Paths(PlannedAction a) => string.Join("|", plan.Files.Where(f => f.Action == a).Select(f => f.Path).Order(StringComparer.Ordinal));
        Eq("ironwail.exe|quakevr/progs.dat", Paths(PlannedAction.Replace), "replaced: the files v2 changed");
        Eq("quakevr/newfile.txt", Paths(PlannedAction.Add), "added");
        Eq("quakevr/vr_defaults.cfg", Paths(PlannedAction.Restore), "restored: the shipped file the player changed");
        Eq("SDL2.dll", Paths(PlannedAction.Remove), "removed: no longer shipped");
        Eq(7, plan.Count(PlannedAction.Unchanged), "the rest unchanged, not copied");
        Eq("quakevr/newfile.txt|quakevr/vr_defaults.cfg", string.Join("|", plan.BackupFirst.Order(StringComparer.Ordinal)), "backed up first");
        var programPaths = manifest2.Files.Select(f => f.Path).Union(r1.Files.Select(f => f.Path), StringComparer.OrdinalIgnoreCase).ToHashSet(StringComparer.OrdinalIgnoreCase);
        True(plan.Files.All(f => programPaths.Contains(f.Path)), "the plan lists only program files");
        True(!plan.Files.Any(f => playerFiles.ContainsKey(f.Path)), "none of the player's");
        Eq(0, plan.RelightInputsChanged.Count, "relight inputs unchanged");
        Eq(playerFiles.Count + 1, plan.UserFiles.Count, "the player's files counted (newfile.txt is theirs until v2 ships it)");
        True(plan.Format().Contains("update 0.9.0"), "printed");

        var copied = new List<string>();
        var logs = new List<string>();
        var engine = new InstallEngine();
        var r2 = engine.Install(new InstallPlan
        {
            Mode = InstallMode.Update, PackagePath = pkg2, TargetDir = target, QuakeDir = quake, Shortcuts = options, RelightOnFirstRun = r1.Choices.RelightOnFirstRun,
            Registry = new JsonFileRegistry(registryFile),
        }, new SyncProgress<InstallProgress>(p => { if (p.Log is not null) { logs.Add(p.Log); } }), CancellationToken.None);
        Eq(v2, r2.Version, "updated");
        True(logs.Any(l => l.Contains("4 program file(s) to copy")), "4 files copied: " + string.Join(" / ", logs));
        Eq(hubTime, File.GetLastWriteTimeUtc(Path.Combine(target, "quakevr", "maps", "vrhub.bsp")), "an unchanged file is not rewritten");
        Eq(v2, new JsonFileRegistry(registryFile).GetValue(UninstallEntry.Key, "DisplayVersion"), "Apps & Features version updated");
        Eq(0, Uninstaller.Verify(target).Count, "verify clean after the update");
        Eq(r1.Files.Count, r2.Files.Count, "11 files recorded (one dropped, one added)");
        var after = Hashes();
        True(before.All(kv => after[kv.Key] == kv.Value), "every player file identical after the update");
        True(!File.Exists(Path.Combine(target, "SDL2.dll")), "dropped file removed");
        Eq("new", File.ReadAllText(Path.Combine(target, "quakevr", "newfile.txt")), "v2's newfile.txt");
        var backup = engine.LastBackup ?? throw new Exception("no backup");
        True(backup.Dir.StartsWith(Path.Combine(target, "backups"), StringComparison.OrdinalIgnoreCase) && backup.Dir.EndsWith(" update", StringComparison.Ordinal), "dated backup: " + backup.Dir);
        Eq("mine", File.ReadAllText(Path.Combine(backup.Dir, "quakevr", "newfile.txt")), "the player's newfile.txt backed up");
        True(File.ReadAllText(Path.Combine(backup.Dir, "quakevr", "vr_defaults.cfg")).Contains("fov 110"), "the player's vr_defaults.cfg backed up");
        Eq(0, Backup.Verify(backup.Dir).Count, "backup checked");
        True(!FirstStartRelight.Pending(target) && !r2.RelightPending && r2.Choices.RelightOnFirstRun, "relight inputs unchanged: no relight, still chosen");
        Eq(8, r2.Shortcuts.Count, "shortcuts kept");
        Eq(quakeBefore, Snapshot(quake), "Quake folder untouched");

        // v3 ships a new light: the relight is asked again. An update from the console (no shortcut folders) keeps the shortcuts.
        var r3 = new InstallEngine().Install(new InstallPlan
        {
            Mode = InstallMode.Update, PackagePath = Fixtures.MakePackage(Dir("upd-pkg3"), v3, new() { ["SDL2.dll"] = "", ["quakevr/newfile.txt"] = "new", ["quakevr/tools/ericw-tools/light.exe"] = "fake light 2" }),
            TargetDir = target, QuakeDir = quake, RelightOnFirstRun = true,
        }, null, CancellationToken.None);
        True(FirstStartRelight.Pending(target) && r3.RelightPending, "a new light.exe: relight at the next start");
        Eq(8, r3.Shortcuts.Count, "console update: shortcuts kept");
        True(File.Exists(Path.Combine(shortcuts, "Desktop", "Quake VR Unleashed.lnk")), "desktop shortcut still there");
        True(before.All(kv => Hashes()[kv.Key] == kv.Value), "player files still identical");
        // A fresh folder with Mode = Update is a normal full install.
        var freshEngine = new InstallEngine();
        var fresh = freshEngine.Install(new InstallPlan { Mode = InstallMode.Update, PackagePath = pkg2, TargetDir = Path.Combine(run, "upd-fresh"), QuakeDir = quake },
            null, CancellationToken.None);
        True(freshEngine.LastPlan is null && fresh.Files.Count == 11, "no install there: a full install");
    }),
    ("repair: the same version restores missing and changed program files; an older package never downgrades", () =>
    {
        var quake = Dir("rep-quake");
        Fixtures.MakeOriginal(quake);
        var target = Path.Combine(run, "rep-QuakeVR");
        const string v1 = "0.9.1 (2026-10-08 bbbbbbb2)";
        var pkg = Fixtures.MakePackage(Dir("rep-pkg"), v1);
        new InstallEngine().Install(new InstallPlan { PackagePath = pkg, TargetDir = target, QuakeDir = quake }, null, CancellationToken.None);
        File.WriteAllText(Path.Combine(target, "quakevr", "ironwail.cfg"), "mine");
        File.Delete(Path.Combine(target, "quakevr", "progs.dat"));
        File.WriteAllText(Path.Combine(target, "ironwail.pak"), "damaged");
        Eq(2, Uninstaller.Verify(target).Count, "two problems");
        var manifest = PackageManifest.Parse(File.ReadAllText(Path.Combine(pkg, PackageManifest.FileName)));
        Eq(InstallMode.Repair, MaintenancePlanner.ModeFor(v1, manifest.Version), "same version: repair");
        var plan = MaintenancePlanner.Plan(target, InstallRecord.Load(target)!, manifest, InstallMode.Repair);
        Eq("ironwail.pak|quakevr/progs.dat", string.Join("|", plan.ToCopy.Select(f => f.Path).Order(StringComparer.Ordinal)), "only the damaged files");
        True(plan.ToCopy.All(f => f.Action == PlannedAction.Restore), "restored");
        var engine = new InstallEngine();
        var r = engine.Install(new InstallPlan { Mode = InstallMode.Repair, PackagePath = pkg, TargetDir = target, QuakeDir = quake }, null, CancellationToken.None);
        Eq(0, Uninstaller.Verify(target).Count, "repaired");
        Eq(v1, r.Version, "same version");
        Eq("damaged", File.ReadAllText(Path.Combine(engine.LastBackup!.Dir, "ironwail.pak")), "the changed file backed up first");
        Eq("mine", File.ReadAllText(Path.Combine(target, "quakevr", "ironwail.cfg")), "settings kept");
        True(!FirstStartRelight.Pending(target), "a repair asks for no relight");

        // An older package: the files identical in both are restored, the rest of the installed version kept; no downgrade.
        var older = Fixtures.MakePackage(Dir("rep-pkg-old"), "0.9.0 (2026-10-01 aaaaaaa1)");
        File.WriteAllText(Path.Combine(target, "ironwail.pak"), "damaged again");
        File.WriteAllText(Path.Combine(target, "quakevr", "progs.dat"), "damaged progs");
        var oldManifest = PackageManifest.Parse(File.ReadAllText(Path.Combine(older, PackageManifest.FileName)));
        Eq(InstallMode.Repair, MaintenancePlanner.ModeFor(v1, oldManifest.Version), "older: repair");
        var oplan = MaintenancePlanner.Plan(target, InstallRecord.Load(target)!, oldManifest, InstallMode.Repair);
        True(oplan.KeepsInstalledVersion, "keeps the installed version");
        Eq("ironwail.pak", string.Join("|", oplan.ToCopy.Select(f => f.Path)), "only the identical file restored");
        Eq("quakevr/progs.dat", string.Join("|", oplan.Files.Where(f => f.Action == PlannedAction.CannotRestore).Select(f => f.Path)), "progs.dat cannot be restored from 0.9.0");
        True(oplan.Files.Any(f => f.Path == "ironwail.exe" && f.Action == PlannedAction.KeepInstalled), "0.9.1's engine kept");
        var ro = new InstallEngine().Install(new InstallPlan { Mode = InstallMode.Repair, PackagePath = older, TargetDir = target, QuakeDir = quake }, null, CancellationToken.None);
        Eq(v1, ro.Version, "still 0.9.1");
        Eq("fake engine " + v1, File.ReadAllText(Path.Combine(target, "ironwail.exe")), "not downgraded");
        Eq("quakevr/progs.dat", string.Join("|", Uninstaller.Verify(target).Select(p => p.Path)), "the one it could not restore still reported");
    }),
    ("reinstall from scratch: each choice moves its files into a dated, checked backup first; the rest untouched", () =>
    {
        var quake = Dir("rei-quake");
        Fixtures.MakeOriginal(quake);
        var target = Path.Combine(run, "rei-QuakeVR");
        var pkg = Fixtures.MakePackage(Dir("rei-pkg"), "0.9.1 (2026-10-08 bbbbbbb2)");
        new InstallEngine().Install(new InstallPlan { PackagePath = pkg, TargetDir = target, QuakeDir = quake }, null, CancellationToken.None);
        var files = new Dictionary<string, (string Body, UserDataKind Kind)>
        {
            ["quakevr/ironwail.cfg"] = ("config", UserDataKind.Settings), ["quakevr/autoexec.cfg"] = ("bind", UserDataKind.Settings),
            ["quakevr/retro_overrides.txt"] = ("retro", UserDataKind.Settings), ["quakevr/bodycal/body.txt"] = ("1.8", UserDataKind.Settings),
            ["quakevr/s0.sav"] = ("save", UserDataKind.Saves), ["quakevr/autosave/a.sav"] = ("auto", UserDataKind.Saves),
            ["qvr_addons/0123456789abcdef/maps/m.bsp"] = ("bsp", UserDataKind.Maps), ["cache/maps_installed.txt"] = ("list", UserDataKind.Maps),
            ["quakevr/screenshots/s.png"] = ("png", UserDataKind.Personal), ["quakevr/notes/n.wav"] = ("wav", UserDataKind.Personal),
            ["quakevr/relit_custom/id1/maps/e1m1.relight"] = ("relit", UserDataKind.Other), ["quakevr/checklist_ticks.txt"] = ("ticks", UserDataKind.Personal),
            ["quakevr/tips_seen.txt"] = ("tips", UserDataKind.Personal), ["qconsole.log"] = ("log", UserDataKind.Personal),
            ["quakevr/maps/mine.bsp"] = ("bsp", UserDataKind.Personal), ["quakevr/relit/id1/maps/e1m1.bsp"] = ("relit", UserDataKind.Other),
            ["cache/maps/0123.zip"] = ("zip", UserDataKind.Other), ["quakevr/cache/x.bin"] = ("cache", UserDataKind.Other),
        };
        void Write()
        {
            foreach (var (rel, (body, _)) in files)
            {
                var p = Path.Combine(target, rel.Replace('/', '\\'));
                Directory.CreateDirectory(Path.GetDirectoryName(p)!);
                File.WriteAllText(p, body);
            }
        }
        Write();
        foreach (var (rel, (_, kind)) in files)
        {
            Eq(kind, UserData.Classify(rel), "kind of " + rel);
        }
        Eq(UserDataKind.Other, UserData.Classify("cache/x.cfg"), "a cache's cfg is not a setting");
        var hashes = files.Keys.ToDictionary(k => k, k => PackageManifest.HashFile(Path.Combine(target, k.Replace('/', '\\'))));
        var none = new ReinstallOptions();
        Eq(0, Reinstaller.Plan(target, none).Count, "nothing chosen: nothing to back up");
        Eq(null, Reinstaller.Prepare(target, none, DateTimeOffset.Now), "and no backup folder");
        True(!Reinstaller.Plan(target, new ReinstallOptions { ResetSettings = true }).Any(f => f.Path == "quakevr/default.cfg"), "shipped configs are program files");

        var now = new DateTimeOffset(2026, 10, 8, 18, 30, 0, TimeSpan.Zero);
        void Check(ReinstallOptions o, string what)
        {
            var expected = files.Where(f => o.Includes(f.Value.Kind)).Select(f => f.Key).Order(StringComparer.Ordinal).ToList();
            Eq(string.Join("|", expected), string.Join("|", Reinstaller.Plan(target, o).Select(f => f.Path)), what + ": planned");
            var b = Reinstaller.Prepare(target, o, now) ?? throw new Exception(what + ": no backup");
            True(Path.GetFileName(b.Dir).StartsWith("2026-10-08 183000 reinstall", StringComparison.Ordinal), what + ": dated folder " + b.Dir);
            Eq(0, Backup.Verify(b.Dir).Count, what + ": backup checked");
            var listed = Backup.Load(b.Dir);
            Eq(string.Join("|", expected), string.Join("|", listed.Files.Select(f => f.Path)), what + ": backup.json lists them");
            foreach (var rel in expected)
            {
                Eq(hashes[rel], listed.Files.Single(f => f.Path == rel).Sha256, what + ": " + rel + " hash in backup.json");
                Eq(hashes[rel], PackageManifest.HashFile(Path.Combine(b.Dir, rel.Replace('/', '\\'))), what + ": " + rel + " in the backup");
                True(!File.Exists(Path.Combine(target, rel.Replace('/', '\\'))), what + ": " + rel + " gone from the install");
            }
            foreach (var rel in files.Keys.Except(expected))
            {
                Eq(hashes[rel], PackageManifest.HashFile(Path.Combine(target, rel.Replace('/', '\\'))), what + ": " + rel + " untouched");
            }
            if (o.RemoveSaves)
            {
                True(!Directory.Exists(Path.Combine(target, "qvr_addons")), what + ": the emptied map folders removed");
            }
            // Then the normal full install over it, with the same backup.
            var engine = new InstallEngine();
            var r = engine.Install(new InstallPlan { PackagePath = pkg, TargetDir = target, QuakeDir = quake, Backup = b }, null, CancellationToken.None);
            True(r.Files.Count == 11 && Uninstaller.Verify(target).Count == 0 && engine.LastBackup == b, what + ": installed again");
            Write(); // the next case starts from everything again
        }
        Check(new ReinstallOptions { ResetSettings = true }, "reset settings");
        Check(new ReinstallOptions { RemoveSaves = true }, "remove saves");
        Check(new ReinstallOptions { ResetSettings = true, RemoveSaves = true }, "both");
        Check(new ReinstallOptions { RemovePersonal = true }, "remove personal files");
        Check(new ReinstallOptions { ResetSettings = true, RemoveSaves = true, RemovePersonal = true }, "a clean start");
        Eq(5, Directory.GetDirectories(Path.Combine(target, "backups")).Length, "five backups, none overwritten (-2, -3... for the same second)");
        True(UserData.Find(target, InstallRecord.Load(target)).All(f => !f.Path.StartsWith("backups/", StringComparison.Ordinal)), "backups are never the player's files to reset");
    }),
    ("update: the HD pack unchanged is skipped (not downloaded), another pack replaces it and asks for the relight", () =>
    {
        var quake = Dir("uhd-quake");
        Fixtures.MakeOriginal(quake);
        var target = Path.Combine(run, "uhd-QuakeVR");
        var zip = Fixtures.TextureZip(Path.Combine(run, "uhd-textures.zip"));
        var zipSha = PackageManifest.HashFile(zip);
        var r1 = new InstallEngine().Install(new InstallPlan
        {
            PackagePath = Fixtures.MakePackage(Dir("uhd-pkg1"), "0.9.0 (2026-10-01 aaaaaaa1)"), TargetDir = target, QuakeDir = quake, HdTexturesZip = zip, RelightOnFirstRun = true,
        }, null, CancellationToken.None);
        Eq(zipSha, r1.HdTexturesSha256, "the pack's SHA-256 recorded");
        Eq("uhd-textures.zip", r1.HdTexturesFile, "and its name");
        FirstStartRelight.Set(target, false);
        var same = new FeedFile { File = "uhd-textures.zip", Size = new FileInfo(zip).Length, Sha256 = zipSha, Urls = ["http://127.0.0.1/x"] };
        var other = new FeedFile { File = "new-textures.zip", Size = 10, Sha256 = new string('a', 64), Urls = ["http://127.0.0.1/y"] };
        Eq(HdTexturesAction.Keep, MaintenancePlanner.HdTextures(target, r1, same, verify: false).Action, "same pack: kept");
        Eq(HdTexturesAction.Replace, MaintenancePlanner.HdTextures(target, r1, other, verify: false).Action, "another pack: replaced");
        Eq(HdTexturesAction.Keep, MaintenancePlanner.HdTextures(target, r1, same, verify: true).Action, "a repair with the files intact: kept");
        var noSha = InstallRecord.Load(target)!;
        noSha.HdTexturesSha256 = null;
        Eq(HdTexturesAction.KeepUnknown, MaintenancePlanner.HdTextures(target, noSha, same, verify: false).Action, "an older record: kept");
        var texture = Path.Combine(target, "id1", "textures", "wall1.png");
        var textureTime = File.GetLastWriteTimeUtc(texture);

        // The update with the same pack: no zip given, the textures stay as they are and stay recorded.
        var r2 = new InstallEngine().Install(new InstallPlan
        {
            Mode = InstallMode.Update, PackagePath = Fixtures.MakePackage(Dir("uhd-pkg2"), "0.9.1 (2026-10-08 bbbbbbb2)"), TargetDir = target, QuakeDir = quake, RelightOnFirstRun = true,
        }, null, CancellationToken.None);
        Eq(textureTime, File.GetLastWriteTimeUtc(texture), "textures not rewritten");
        Eq(r1.Files.Count(f => f.Component == Components.HdTextures), r2.Files.Count(f => f.Component == Components.HdTextures), "still recorded");
        Eq(zipSha, r2.HdTexturesSha256, "the pack's SHA-256 kept");
        True(!FirstStartRelight.Pending(target), "no relight: nothing it reads changed");

        // A repair sees a missing texture: the pack is fetched again.
        File.Delete(texture);
        Eq(HdTexturesAction.Restore, MaintenancePlanner.HdTextures(target, r2, same, verify: true).Action, "a repair restores the pack");
        Eq(HdTexturesAction.Keep, MaintenancePlanner.HdTextures(target, r2, same, verify: false).Action, "an update does not check them");

        // Another pack: installed, recorded, and the relight asked again.
        var zip2 = Path.Combine(run, "uhd-textures2.zip");
        using (var z = System.IO.Compression.ZipFile.Open(zip2, System.IO.Compression.ZipArchiveMode.Create))
        {
            using var s = z.CreateEntry("id1/textures/wall1.png").Open();
            s.Write("png v2"u8);
        }
        var r3 = new InstallEngine().Install(new InstallPlan
        {
            Mode = InstallMode.Update, PackagePath = Fixtures.MakePackage(Dir("uhd-pkg3"), "0.9.2 (2026-10-09 ccccccc3)"), TargetDir = target, QuakeDir = quake, RelightOnFirstRun = true,
            HdTexturesZip = zip2,
        }, null, CancellationToken.None);
        Eq("png v2", File.ReadAllText(texture), "the new pack's texture");
        Eq(PackageManifest.HashFile(zip2), r3.HdTexturesSha256, "the new pack recorded");
        True(FirstStartRelight.Pending(target), "a new pack: relight at the next start");
    }),
};

static string JsonString(string s) => System.Text.Json.JsonSerializer.Serialize(s);

static string Snapshot(string dir) => string.Join("|", Directory.EnumerateFileSystemEntries(dir, "*", SearchOption.AllDirectories).Order()
    .Select(p => File.Exists(p) ? $"{p}:{new FileInfo(p).Length}:{File.GetLastWriteTimeUtc(p).Ticks}" : p));

tests.AddRange(QuestTests.All(Dir));

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
