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

    // The engine's command line: none by default; "adb shell am start ... --es args '<arguments>'" adds some
    // (e.g. "+map e1m1"), split at spaces.
    @Override
    protected String[] getArguments()
    {
        final String args = getIntent() != null ? getIntent().getStringExtra("args") : null;
        if (args == null || args.trim().isEmpty())
        {
            return new String[0];
        }
        return args.trim().split("\\s+");
    }
}
