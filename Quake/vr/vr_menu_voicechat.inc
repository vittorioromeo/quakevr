// vr_menu_voicechat.inc -- the Voice Chat page (VR Settings > HUD, Haptics and Sound > Voice Chat; MP_PLAN.md, B10v
// part 2): your voice, the voices you hear, and a row for each other player (mute, his volume, who talks).
// Included in vr_menu.cpp, so it shares that file's (anonymous-namespace) state.

unsigned long long voicePlayersBuilt = ~0ull; // the players the page was built for (voicechat::players())

constexpr float voiceVolumeStep = 0.25f;

[[nodiscard]] const char* voicePlayerRow(int slot)
{
    return voicechat::playerLine(slot);
}

[[nodiscard]] const char* voicePlayerHelp(int)
{
    return "Mute: his voice not played (he is still sent to the others). Quieter and Louder: his voice alone, 0 to 400% "
           "(on top of Voices' Volume). Talking: ((o)) over him in the game too (Talking Indicator). Kept for this "
           "game by his name: a new player in his place starts heard at 100%.";
}

[[nodiscard]] const char* voicePlayerButtonsText(int)
{
    return "";
}

[[nodiscard]] const char* voicePlayerButtonLabel(int slot, int b)
{
    return b == 0 ? (voicechat::muted(slot) ? "Unmute" : "Mute") : b == 1 ? "Quieter" : "Louder";
}

[[nodiscard]] bool voicePlayerButtonDim(int slot, int b)
{
    const float v = voicechat::playerVolume(slot);
    return !voicechat::listed(slot) || (b == 1 && v <= 0.f) || (b == 2 && v >= 4.f);
}

[[nodiscard]] int voicePlayerButtonTint(int slot, int b)
{
    return b == 0 && voicechat::muted(slot) ? 2 : 0;
}

void voicePlayerButtonPress(int slot, int b)
{
    if(!voicechat::listed(slot))
    {
        return;
    }
    if(b == 0)
    {
        voicechat::setMuted(slot, !voicechat::muted(slot));
        return;
    }
    const float v = voicechat::playerVolume(slot) + (b == 1 ? -voiceVolumeStep : voiceVolumeStep);
    voicechat::setPlayerVolume(slot, floorf(v / voiceVolumeStep + 0.5f) * voiceVolumeStep);
}

// The row's own text tinted: green while he talks, red muted. (review C8: his talking itself, not "talking" searched for
// in the row's text: a player named so was lit all the time)
[[nodiscard]] int voicePlayerMark(int slot)
{
    return voicechat::muted(slot) ? 2 : voicechat::listed(slot) && voicechat::talkingNow(slot) ? 1 : 0;
}

[[nodiscard]] za::Vector<Item> pageVoiceChat()
{
    voicePlayersBuilt = voicechat::players();
    za::Vector<Item> items{
        info(voicechat::ownLine),
        header("Your Voice"),
        toggle("Voice Chat", vr_voicechat)
            .help("vr_voicechat: in a multiplayer game, your microphone sent while you talk and the others heard at their "
                  "heads. Push-to-talk: the off hand's upper button at the mouth (Voice Notes off), or the Push-to-Talk key "
                  "below. Single player: nothing."),
        cycle("Talk With", vr_voicechat_mode, {{0.f, "Push-to-Talk"}, {1.f, "Voice Activation"}})
            .help("vr_voicechat_mode: Push-to-Talk, you are sent while you hold the button or the key; Voice Activation, "
                  "while a voice is heard (no button)."),
        slider("Activation Threshold", vr_voicechat_vad, 0.1f, 0.95f, 0.05f, "%.2f")
            .help("vr_voicechat_vad: how sure the noise suppressor must be it hears a voice before voice activation sends."),
        row(opt::vr_flat_key_voicechat),
        toggle("Noise Suppression", vr_voicechat_denoise)
            .help("vr_voicechat_denoise: your microphone's noise (fans, keys, breath) taken out by RNNoise before it is sent."),
        slider("Microphone Gain", vr_voicechat_mic_gain, 0.f, 4.f, 0.25f, "%.2f")
            .help("vr_voicechat_mic_gain: your microphone's level before it is sent (the level on the line above, while you talk)."),
        command("Microphones", "vr_note_devices")
            .help("vr_note_devices: the microphones (the console lists them). Voice chat records from Voice Notes' one: "
                  "vr_note_device \"<part of its name>\" (empty: the system's default)."),
        header("Voices You Hear"),
        slider("Volume", vr_voicechat_volume, 0.f, 2.f, 0.1f, "%.1f")
            .help("vr_voicechat_volume: the other players' voices (each one's own: his row below)."),
        slider("Fade With Distance", vr_voicechat_attenuation, 0.f, 2.f, 0.1f, "%.1f")
            .help("vr_voicechat_attenuation: how fast a voice fades with distance (1 as a gunshot, silent at 1000 units; 0.5 at 2000)."),
        toggle("Radio", vr_voicechat_radio)
            .help("vr_voicechat_radio: every voice at full volume wherever its player is; off, at his head, fading with distance."),
        toggle("Talking Indicator", vr_voicechat_indicator)
            .help("vr_voicechat_indicator: ((o)) over a talking player's head, TALKING low in your view while you send."),
        header("Players"),
    };
    int shown = 0;
    for(int slot = 0; slot < MAX_SCOREBOARD; slot++)
    {
        if(!voicechat::listed(slot))
        {
            continue;
        }
        Item r = row(voicePlayerRow, voicePlayerHelp, nullptr, slot, -1);
        r.markArg = voicePlayerMark;
        r.buttons = 3;
        r.buttonPress = voicePlayerButtonPress;
        r.buttonDim = voicePlayerButtonDim;
        items.pushBack(r);
        Item b = row(voicePlayerButtonsText, voicePlayerHelp, nullptr, slot, -1);
        b.partOf = 1;
        b.buttons = 3;
        b.buttonLabel = voicePlayerButtonLabel;
        b.buttonDim = voicePlayerButtonDim;
        b.buttonTint = voicePlayerButtonTint;
        items.pushBack(b);
        shown++;
    }
    if(shown == 0)
    {
        items.pushBack(info([]() -> const char* { return "No other players (in a multiplayer game, each one is listed here)"; }));
    }
    items.pushBack(header("Testing").developer());
    items.pushBack(command("Players (Console)", "vr_voicechat_players").developer()
            .help("vr_voicechat_players: each player's number, whether you hear or muted him, his volume, who talks. Also: "
                  "vr_voicechat_mute <number or name>, vr_voicechat_player_volume <number or name> <0-4>."));
    items.pushBack(command("Report", "vr_voicechat_report").developer()
            .help("vr_voicechat_report: what you send (frames, bytes, spurts, the level) and each voice heard (frames played, "
                  "recovered by FEC, concealed, late, the jitter, the gain at his distance); hosting: vr_voicechat_relay."));
    items.pushBack(command("Self-Test", "vr_voicechat_selftest").developer()
            .help("vr_voicechat_selftest: Opus and RNNoise in a loop (SNR, bitrate, FEC, concealment, noise) and the jitter buffer."));
    items.pushBack(slider("Voice Budget (Hosting)", vr_net_voice_bytes, 0.f, 16000.f, 500.f, "%.0f B/s").developer()
            .help("vr_net_voice_bytes: hosting, each player's voice chat bytes a second at most (at most half his rate cap); "
                  "frames waiting over 0.3 s are dropped. 0: no voice relayed."));
    return items;
}
