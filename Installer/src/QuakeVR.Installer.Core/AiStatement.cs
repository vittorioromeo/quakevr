using System.Text;

namespace QuakeVR.Installer.Core;

/// <summary>
/// Vittorio's statement on AI usage (the wizard's "Statement" page, after Welcome; <c>qvr-setup install</c> prints it and
/// needs <c>--accept-statement</c>). Four claims, each answered YES or NO; each starts unanswered and, once answered,
/// can only be switched between YES and NO, never back to unanswered. Going on needs YES to all four. The answers live
/// only in this object for the session: never saved, never sent.
/// </summary>
public sealed class AiStatement
{
    public const string Title = "Statement";
    public const string Subtitle = "A statement from Vittorio regarding AI usage";

    // The author's wording, unchanged (only "reinassance" corrected to "renaissance").
    public static readonly IReadOnlyList<string> Paragraphs =
    [
        "I have developed \"Quake VR: Unleashed\" with heavy AI assistance, rebasing my original fully hand-written older " +
        "\"Quake VR\" project on top of the Ironwail source port, and using an LLM-driven workflow to improve and iterate on " +
        "the existing foundation.",
        "VR gaming and many other areas of our life are in a period of renaissance thanks to AI. As any other tool, it " +
        "can be used to produce both quality products and so called \"slop\".", "I am deeply saddened by the constant undeserved harassment and bullying that AI " +
        "content creators are receiving online, despite the very clear passion and effort they put in their work.",
		"I want to live in a future where products are judged by their quality and by the passion of their creators, not by the tools used to create them.",
        "As a \"Quake VR: Unleashed\" player, your values and views regarding AI usage should be fair and respectful of the hard work put into this project.",
    ];

    public static readonly IReadOnlyList<string> Claims =
    [
        "I recognize that AI-assisted development does *not* imply creating \"slop\".",
        "I recognize that AI-assisted development does *not* imply lack of technical expertise.",
        "I recognize that AI-assisted development does *not* imply less valuable end products.",
        "I recognize that AI-assisted development does *not* imply lack of care and effort from the author.",
    ];

    readonly bool?[] _answers = new bool?[Claims.Count];

    /// <summary>Raised when an answer changes.</summary>
    public event Action? Changed;

    /// <summary>The answer to claim <paramref name="index"/>: null (unanswered), true (YES) or false (NO).</summary>
    public bool? this[int index] => _answers[index];

    /// <summary>Answers a claim YES or NO. There is no way back to unanswered.</summary>
    public void Answer(int index, bool yes)
    {
        if (_answers[index] != yes)
        {
            _answers[index] = yes;
            Changed?.Invoke();
        }
    }

    /// <summary>Whether every claim is answered YES: the only state that lets the player go on.</summary>
    public bool AllYes => _answers.All(a => a == true);

    public int Unanswered => _answers.Count(a => a is null);

    /// <summary>The statement as plain text (the console prints it).</summary>
    public static string Format()
    {
        var sb = new StringBuilder();
        sb.AppendLine(Title).AppendLine(Subtitle).AppendLine();
        foreach (var p in Paragraphs)
        {
            sb.AppendLine(p).AppendLine();
        }
        for (var i = 0; i < Claims.Count; ++i)
        {
            sb.AppendLine($"  {i + 1}. {Claims[i]}");
        }
        return sb.ToString();
    }
}
