using System.Runtime.InteropServices;
using System.Security.Cryptography.X509Certificates;

namespace QuakeVR.Installer.Core.Prerequisites;

/// <summary>What a file's Authenticode signature says.</summary>
/// <param name="Trusted">WinVerifyTrust accepts it: signed, unchanged since, by a certificate chaining to a root this PC
/// trusts.</param>
/// <param name="Signer">The signing certificate's subject (null when unsigned).</param>
public sealed record SignatureInfo(bool Trusted, string? Signer, string Detail)
{
    /// <summary>Trusted, and signed by Microsoft Corporation (the subject's O= and CN=, as Microsoft's own
    /// redistributables are).</summary>
    public bool IsMicrosoft => Trusted && Signer is not null &&
        Signer.Contains("O=Microsoft Corporation", StringComparison.Ordinal) &&
        Signer.Contains("CN=Microsoft Corporation", StringComparison.Ordinal);
}

public interface ISignatureVerifier
{
    SignatureInfo Verify(string path);
}

/// <summary>
/// The real check: <c>WinVerifyTrust</c> (WINTRUST_ACTION_GENERIC_VERIFY_V2, the file's embedded signature, no UI, no
/// revocation fetch: a download must not hang on a CRL server), then the signer's subject from the signature.
/// </summary>
public sealed class AuthenticodeVerifier : ISignatureVerifier
{
    public SignatureInfo Verify(string path)
    {
        if (!File.Exists(path))
        {
            return new SignatureInfo(false, null, "file not found");
        }
        var status = WinVerify(Path.GetFullPath(path));
        string? signer = null;
        try
        {
#pragma warning disable SYSLIB0057 // (the signer of an Authenticode signature: no replacement API in .NET 9)
            using var cert = X509Certificate.CreateFromSignedFile(path);
#pragma warning restore SYSLIB0057
            signer = cert.Subject;
        }
        catch (System.Security.Cryptography.CryptographicException)
        {
        }
        return status == 0
            ? new SignatureInfo(true, signer, "signature valid")
            : new SignatureInfo(false, signer, $"signature not trusted (WinVerifyTrust 0x{status:X8})");
    }

    static readonly Guid GenericVerifyV2 = new("00AAC56B-CD44-11d0-8CC2-00C04FC295EE");

    static int WinVerify(string path)
    {
        var pathPtr = Marshal.StringToCoTaskMemUni(path);
        var fileInfo = new WintrustFileInfo { cbStruct = (uint)Marshal.SizeOf<WintrustFileInfo>(), pcwszFilePath = pathPtr };
        var filePtr = Marshal.AllocCoTaskMem(Marshal.SizeOf<WintrustFileInfo>());
        try
        {
            Marshal.StructureToPtr(fileInfo, filePtr, false);
            var data = new WintrustData
            {
                cbStruct = (uint)Marshal.SizeOf<WintrustData>(),
                dwUIChoice = 2, // WTD_UI_NONE
                fdwRevocationChecks = 0, // WTD_REVOKE_NONE
                dwUnionChoice = 1, // WTD_CHOICE_FILE
                pFile = filePtr,
                dwStateAction = 1, // WTD_STATEACTION_VERIFY
                dwProvFlags = 0x1000, // WTD_CACHE_ONLY_URL_RETRIEVAL
            };
            var action = GenericVerifyV2;
            var result = WinVerifyTrust(IntPtr.Zero, ref action, ref data);
            data.dwStateAction = 2; // WTD_STATEACTION_CLOSE
            _ = WinVerifyTrust(IntPtr.Zero, ref action, ref data);
            return result;
        }
        finally
        {
            Marshal.FreeCoTaskMem(filePtr);
            Marshal.FreeCoTaskMem(pathPtr);
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    struct WintrustFileInfo
    {
        public uint cbStruct;
        public IntPtr pcwszFilePath;
        public IntPtr hFile;
        public IntPtr pgKnownSubject;
    }

    [StructLayout(LayoutKind.Sequential)]
    struct WintrustData
    {
        public uint cbStruct;
        public IntPtr pPolicyCallbackData;
        public IntPtr pSIPClientData;
        public uint dwUIChoice;
        public uint fdwRevocationChecks;
        public uint dwUnionChoice;
        public IntPtr pFile;
        public uint dwStateAction;
        public IntPtr hWVTStateData;
        public IntPtr pwszURLReference;
        public uint dwProvFlags;
        public uint dwUIContext;
        public IntPtr pSignatureSettings;
    }

    [DllImport("wintrust.dll", ExactSpelling = true)]
    static extern int WinVerifyTrust(IntPtr hwnd, ref Guid action, ref WintrustData data);
}
