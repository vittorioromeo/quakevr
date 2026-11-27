using System.Text;

namespace QuakeVR.Installer.Core;

/// <summary>
/// Vittorio's statement on AI usage (the wizard's "Statement" page, after Welcome; <c>qvr-setup install</c> and
/// <c>qvr-setup statement</c> print it). Static text: one sentence (<see cref="Pledge"/>) in the claims' card, nothing to
/// answer, nothing recorded; Next is enabled at once.
/// </summary>
public sealed class AiStatement
{
    public const string Title = "Statement";
    public const string Subtitle = "A statement from Vittorio regarding AI usage";

    // The author's wording, unchanged (only "reinassance" corrected to "renaissance").
    public static readonly IReadOnlyList<string> Paragraphs =
    [
        "I have developed \"Quake VR: UNLEASHED\" with heavy AI assistance, rebasing my original fully hand-written older " +
        "\"Quake VR\" project on top of the Ironwail source port, and using an agent-driven workflow to improve and iterate on " +
        "the existing foundation.",
        "VR gaming and many other areas of our life are in a period of renaissance thanks to AI. As any other tool, it " +
        "can be used to produce both quality products and so called \"slop\".", "I am deeply saddened by the constant undeserved harassment and bullying that AI " +
        "content creators are receiving online, despite the very clear passion and effort they put in their work.",
		"I want to live in a future where products are judged by their quality and by the passion of their creators, not by the tools used to create them.",
        "As a \"Quake VR: UNLEASHED\" player, your values and views regarding AI usage should be fair and respectful of the hard work put into this project.",
    ];

    /// <summary>The one sentence under the paragraphs; the <c>*not*</c> is set in italics by the wizard.</summary>
    public const string Pledge = "By continuing, I recognize that AI-assisted development does *not* imply a low-quality result nor a lack of care, passion, or expertise.";

    /// <summary>
    /// Splits a claim around its one emphasized span, written <c>*like this*</c>: the wizard sets the middle part in
    /// italics. A text without a closed pair comes back whole as <paramref name="text"/>'s first part.
    /// </summary>
    public static (string Before, string Emphasis, string After) SplitEmphasis(string text)
    {
        var open = text.IndexOf('*');
        var close = open < 0 ? -1 : text.IndexOf('*', open + 1);
        if (close < 0)
        {
            return (text, "", "");
        }
        return (text[..open], text[(open + 1)..close], text[(close + 1)..]);
    }

    /// <summary>The pledge without its emphasis marks (screen readers get this).</summary>
    public static string Plain(string text)
    {
        var (before, emphasis, after) = SplitEmphasis(text);
        return before + emphasis + after;
    }

    /// <summary>The statement as plain text (the console prints it; the pledge keeps its <c>*emphasis*</c> marks).</summary>
    public static string Format()
    {
        var sb = new StringBuilder();
        sb.AppendLine(Title).AppendLine(Subtitle).AppendLine();
        foreach (var p in Paragraphs)
        {
            sb.AppendLine(p).AppendLine();
        }
        sb.AppendLine($"  {Pledge}");
        return sb.ToString();
    }
}
