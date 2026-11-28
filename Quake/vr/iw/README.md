# Quake/vr/iw: Quake VR's blocks of Ironwail's files

Each `.c` file here is a block of Quake VR code moved as it was out of one of Ironwail's files (named in the file's
first lines), which includes it at the same place:

    #include "vr/iw/menu_main.c" // QVR: the VR main menu (moved out as it was)

The block stays in that file's translation unit: it sees the same statics, compiles as C with the same flags, and runs
the same code, so the move changes no behaviour and costs nothing on hot paths. What it buys is a smaller diff with
Ironwail (one line per block in its file; IRONWAIL_DIFF.md), so merging Ironwail's changes touches less of our code.

These files are not compiled on their own: no build globs this folder (the `vr/*.cpp` globs are not recursive), as with
`vr/vr_qcexec.c`. Edit them as part of the file that includes them; a new block of Ironwail-file code that is wholly
Quake VR's can come here the same way.
