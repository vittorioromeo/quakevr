using System.Text;

namespace QuakeVR.Installer.Core.Detection;

/// <summary>An object of Valve's KeyValues text format (libraryfolders.vdf, appmanifest_*.acf): ordered, keys
/// compared without case, each value a string or a nested object.</summary>
public sealed class VdfObject
{
    readonly List<KeyValuePair<string, object>> _entries = [];

    public IReadOnlyList<KeyValuePair<string, object>> Entries => _entries;

    internal void Add(string key, object value) => _entries.Add(new(key, value));

    public string? GetString(string key) =>
        _entries.FirstOrDefault(e => string.Equals(e.Key, key, StringComparison.OrdinalIgnoreCase)).Value as string;

    public VdfObject? GetObject(string key) =>
        _entries.FirstOrDefault(e => string.Equals(e.Key, key, StringComparison.OrdinalIgnoreCase) && e.Value is VdfObject)
            .Value as VdfObject;
}

/// <summary>A tolerant reader for Valve's KeyValues text: quoted or bare tokens, <c>\\ \" \n \t</c> escapes,
/// <c>//</c> comments, and platform conditionals (<c>[$WIN32]</c>, skipped). Malformed input ends the object early
/// instead of throwing: a broken Steam file must not stop the other stores' detection.</summary>
public static class Vdf
{
    public static VdfObject Parse(string text)
    {
        var pos = 0;
        var root = new VdfObject();
        ParseObject(text, ref pos, root);
        return root;
    }

    static void ParseObject(string text, ref int pos, VdfObject target)
    {
        while (true)
        {
            var key = NextToken(text, ref pos, out var kind);
            if (kind == TokenKind.End || kind == TokenKind.Close)
            {
                return;
            }
            if (kind != TokenKind.String)
            {
                continue; // a stray '{': skip it
            }
            var value = NextToken(text, ref pos, out kind);
            if (kind == TokenKind.Open)
            {
                var child = new VdfObject();
                ParseObject(text, ref pos, child);
                target.Add(key!, child);
            }
            else if (kind == TokenKind.String)
            {
                target.Add(key!, value!);
            }
            else
            {
                return;
            }
        }
    }

    enum TokenKind { String, Open, Close, End }

    static string? NextToken(string text, ref int pos, out TokenKind kind)
    {
        while (true)
        {
            while (pos < text.Length && char.IsWhiteSpace(text[pos]))
            {
                ++pos;
            }
            if (pos >= text.Length)
            {
                kind = TokenKind.End;
                return null;
            }
            var c = text[pos];
            if (c == '/' && pos + 1 < text.Length && text[pos + 1] == '/')
            {
                while (pos < text.Length && text[pos] != '\n')
                {
                    ++pos;
                }
                continue;
            }
            if (c == '[')
            {
                // A conditional such as [$WIN32]: not a key or a value.
                while (pos < text.Length && text[pos] != ']')
                {
                    ++pos;
                }
                ++pos;
                continue;
            }
            if (c == '{')
            {
                ++pos;
                kind = TokenKind.Open;
                return null;
            }
            if (c == '}')
            {
                ++pos;
                kind = TokenKind.Close;
                return null;
            }
            var sb = new StringBuilder();
            if (c == '"')
            {
                ++pos;
                while (pos < text.Length && text[pos] != '"')
                {
                    if (text[pos] == '\\' && pos + 1 < text.Length)
                    {
                        ++pos;
                        sb.Append(text[pos] switch { 'n' => '\n', 't' => '\t', _ => text[pos] });
                    }
                    else
                    {
                        sb.Append(text[pos]);
                    }
                    ++pos;
                }
                ++pos; // the closing quote
            }
            else
            {
                while (pos < text.Length && !char.IsWhiteSpace(text[pos]) && text[pos] is not ('{' or '}' or '"'))
                {
                    sb.Append(text[pos++]);
                }
            }
            kind = TokenKind.String;
            return sb.ToString();
        }
    }
}
