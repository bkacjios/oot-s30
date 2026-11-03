#ifndef S30_PLATFORM_H
#define S30_PLATFORM_H

// Directory with the asset segment files (<name>.bin), set at startup
extern const char* gS30DataDir;
// Save file (the 32 KiB N64 SRAM image)
extern const char* gS30SaveFile;

// Frame pacing: panel refreshes per game frame (the game's R_UPDATE_RATE)
void S30Display_SetUpdateRate(int vblanks);

// Starts the audio thread (src/s30/main.c: the game's AudioThread_Update on
// the shared library's audio thread)
void S30_StartAudio(void);

// Display-list markers for the renderer (src/s30/gfx.c): a command whose w0 is
// S30_GFX_MARK and whose w1 names the operation
#define S30_GFX_MARK 0x424C4E44u        // "BLND"
#define S30_MARK_BG_SAVE 0x50424753u    // "PBGS": copy the frame so far as the pause background
#define S30_MARK_BG_RESTORE 0x50424752u // "PBGR": draw the saved pause background
#define S30_MARK_VISMONO 0x564D4F4Eu    // "VMON": VisMono, desaturate the frame (prim/env colors)

// Graph callback depth test: whether a point at N64 screen pixel (x, y) and
// NDC depth z passes the depth test against the z-buffer. Results lag one
// frame (call i of a frame answers with call i of the last frame);
// S30Gfx_BeginDepthTests starts each frame's calls.
void S30Gfx_BeginDepthTests(void);
int S30Gfx_DepthVisible(float x, float y, float z);

#endif
