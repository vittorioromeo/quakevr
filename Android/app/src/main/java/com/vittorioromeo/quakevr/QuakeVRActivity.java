// QuakeVRActivity.java -- Quake VR's Android activity (docs/vr-port/QUEST_PLAN.md, C3): SDL2's own activity
// (org.libsdl.app.SDLActivity, from SDL2's source: the window, the EGL context, input, audio), loading the engine's
// library, libquakevr.so (its SDL_main is the engine's main: Quake/main_sdl.c).
package com.vittorioromeo.quakevr;

import org.libsdl.app.SDLActivity;

public class QuakeVRActivity extends SDLActivity
{
    // SDL2 first, then the engine (the last one is the library whose SDL_main runs).
    @Override
    protected String[] getLibraries()
    {
        return new String[] {"SDL2", "quakevr"};
    }

    // The engine's command line: "-game quakevr" (the PC's launch line: Quake VR's own folder over id1) and whatever
    // "adb shell am start ... --es args '<arguments>'" adds (e.g. "+map e1m1"), split at spaces; arguments with a
    // -game of their own replace the default (e.g. "-game ad -game quakevr").
    @Override
    protected String[] getArguments()
    {
        final String args = getIntent() != null ? getIntent().getStringExtra("args") : null;
        final String line = args == null ? "" : args.trim();
        final boolean hasGame = (" " + line + " ").contains(" -game ");
        final String full = (hasGame ? "" : "-game quakevr ") + line;
        return full.trim().split("\\s+");
    }
}
