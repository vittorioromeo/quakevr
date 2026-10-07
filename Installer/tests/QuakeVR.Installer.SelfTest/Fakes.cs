using QuakeVR.Installer.Core.Prerequisites;

namespace QuakeVR.Installer.SelfTest;

/// <summary>A signature check that answers what the test says (no file is trusted or run for real).</summary>
sealed class FakeVerifier(Func<string, SignatureInfo> verify) : ISignatureVerifier
{
    public SignatureInfo Verify(string path) => verify(path);
}

/// <summary>An "elevated" run that only records the command and returns the test's exit code.</summary>
sealed class FakeRunner(Func<string, string, int> run) : IElevatedRunner
{
    public int Run(string exe, string arguments) => run(exe, arguments);
}
