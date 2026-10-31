/*
 * Display output for the PocketGo S30: KMS (sun4i-drm, DSI panel) + GBM + EGL
 * with GLES2 on lima.
 *
 * The panel is 320x480 portrait, mounted landscape. The game renders 3D into
 * an offscreen framebuffer the size of the landscape screen (S30_FB_W x
 * S30_FB_H; widescreen, see gfx.c) and 2D rectangles into a native-resolution
 * layer (S30_2D_W x S30_2D_H) that is composited onto it with sharp-bilinear
 * scaling whenever 3D follows 2D and at frame end. The image is drawn
 * rotated onto the panel, then page-flipped. Frames are
 * paced to the game's update rate in panel refreshes (S30Display_SetUpdateRate:
 * 3 = 20 fps in play, 2 = 30 fps in the pause menu, 1 = 60 fps on the logo).
 */
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <gbm.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include "s30_display.h"

static int sVblanksPerFrame = 3;

void S30Display_SetUpdateRate(int vblanks) {
    sVblanksPerFrame = vblanks < 1 ? 1 : vblanks > 3 ? 3 : vblanks;
}

static struct {
    int fd;
    drmModeModeInfo mode;
    uint32_t connectorId;
    uint32_t crtcId;
    struct gbm_device* gbm;
    struct gbm_surface* surface;
    struct gbm_bo* curBo;
    EGLDisplay eglDisplay;
    EGLContext eglContext;
    EGLSurface eglSurface;
    GLuint fbo;
    GLuint fbo2d;
    GLuint tex2d;
    int dirty2d;   // the 2D layer holds draws not yet composited
    int bound;     // 0 none, 1 3D, 2 2D
    GLuint compProgram;
    GLint compPos, compUv;
    GLuint fboTex;
    GLuint fboDepth;
    GLuint blitProgram;
    GLint blitPos;
    GLint blitUv;
    GLuint bgTex; // pause background: a copy of the 3D image
    GLuint bgFbo;
    GLuint monoSrcTex; // VisMono source: a copy of the 3D image
    GLuint monoSrcFbo;
    GLuint monoProgram;
    GLint monoPos, monoUv, monoPrim, monoEnv;
    GLuint probeProgram;
    GLint probePos, probeZ;
    uint32_t lastFlipSeq;
} sDisp;

static void die(const char* what) {
    fprintf(stderr, "display: %s failed (%s)\n", what, strerror(errno));
    exit(1);
}

/* The scanout device is the DRM card with a connected connector (card0 is the
 * lima render node on this board, card1 sun4i-drm). */
static void open_kms(void) {
    char path[32];
    int i;

    for (i = 0; i < 4; i++) {
        drmModeRes* res;
        int c;

        snprintf(path, sizeof(path), "/dev/dri/card%d", i);
        sDisp.fd = open(path, O_RDWR | O_CLOEXEC);
        if (sDisp.fd < 0) {
            continue;
        }
        res = drmModeGetResources(sDisp.fd);
        if (res == NULL) {
            close(sDisp.fd);
            continue;
        }
        for (c = 0; c < res->count_connectors; c++) {
            drmModeConnector* conn = drmModeGetConnector(sDisp.fd, res->connectors[c]);
            if (conn != NULL && conn->connection == DRM_MODE_CONNECTED && conn->count_modes > 0) {
                drmModeEncoder* enc = drmModeGetEncoder(sDisp.fd, conn->encoder_id ? conn->encoder_id
                                                                                    : conn->encoders[0]);
                sDisp.mode = conn->modes[0];
                sDisp.connectorId = conn->connector_id;
                sDisp.crtcId = (enc != NULL && enc->crtc_id) ? enc->crtc_id : res->crtcs[0];
                drmModeFreeEncoder(enc);
                drmModeFreeConnector(conn);
                drmModeFreeResources(res);
                printf("display: %s, %ux%u@%u\n", path, sDisp.mode.hdisplay, sDisp.mode.vdisplay,
                       sDisp.mode.vrefresh);
                return;
            }
            drmModeFreeConnector(conn);
        }
        drmModeFreeResources(res);
        close(sDisp.fd);
    }
    fprintf(stderr, "display: no connected DRM output\n");
    exit(1);
}

static void init_egl(void) {
    static const EGLint configAttribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 0, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_NONE,
    };
    static const EGLint contextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    EGLConfig configs[64];
    EGLint numConfigs;
    EGLConfig config = NULL;
    int i;

    sDisp.gbm = gbm_create_device(sDisp.fd);
    if (sDisp.gbm == NULL) {
        die("gbm_create_device");
    }
    sDisp.surface = gbm_surface_create(sDisp.gbm, sDisp.mode.hdisplay, sDisp.mode.vdisplay,
                                       GBM_FORMAT_XRGB8888, GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
    if (sDisp.surface == NULL) {
        die("gbm_surface_create");
    }
    sDisp.eglDisplay = eglGetDisplay((EGLNativeDisplayType)sDisp.gbm);
    if (!eglInitialize(sDisp.eglDisplay, NULL, NULL)) {
        die("eglInitialize");
    }
    eglBindAPI(EGL_OPENGL_ES_API);
    /* The window surface needs the config whose visual matches the GBM format */
    eglChooseConfig(sDisp.eglDisplay, configAttribs, configs, 64, &numConfigs);
    for (i = 0; i < numConfigs; i++) {
        EGLint id;
        eglGetConfigAttrib(sDisp.eglDisplay, configs[i], EGL_NATIVE_VISUAL_ID, &id);
        if (id == GBM_FORMAT_XRGB8888) {
            config = configs[i];
            break;
        }
    }
    if (config == NULL) {
        fprintf(stderr, "display: no EGL config for XRGB8888\n");
        exit(1);
    }
    sDisp.eglContext = eglCreateContext(sDisp.eglDisplay, config, EGL_NO_CONTEXT, contextAttribs);
    sDisp.eglSurface = eglCreateWindowSurface(sDisp.eglDisplay, config, (EGLNativeWindowType)sDisp.surface, NULL);
    if (sDisp.eglContext == EGL_NO_CONTEXT || sDisp.eglSurface == EGL_NO_SURFACE) {
        die("eglCreate*");
    }
    eglMakeCurrent(sDisp.eglDisplay, sDisp.eglSurface, sDisp.eglSurface, sDisp.eglContext);
    printf("display: GL %s, %s\n", glGetString(GL_VERSION), glGetString(GL_RENDERER));
}

static GLuint compile(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    GLint ok;

    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, sizeof(log), NULL, log);
        fprintf(stderr, "display: shader: %s\n", log);
        exit(1);
    }
    return s;
}

GLuint S30Display_LinkProgram(const char* vs, const char* fs) {
    GLuint p = glCreateProgram();
    GLint ok;

    glAttachShader(p, compile(GL_VERTEX_SHADER, vs));
    glAttachShader(p, compile(GL_FRAGMENT_SHADER, fs));
    glLinkProgram(p);
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(p, sizeof(log), NULL, log);
        fprintf(stderr, "display: link: %s\n", log);
        exit(1);
    }
    return p;
}

static void init_fbo(void) {
    glGenTextures(1, &sDisp.fboTex);
    glBindTexture(GL_TEXTURE_2D, sDisp.fboTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, S30_FB_W, S30_FB_H, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenRenderbuffers(1, &sDisp.fboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, sDisp.fboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, S30_FB_W, S30_FB_H);

    glGenFramebuffers(1, &sDisp.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, sDisp.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sDisp.fboTex, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, sDisp.fboDepth);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "display: offscreen framebuffer incomplete\n");
        exit(1);
    }

    sDisp.blitProgram = S30Display_LinkProgram(
        "attribute vec2 aPos; attribute vec2 aUv; varying vec2 vUv;\n"
        "void main() { vUv = aUv; gl_Position = vec4(aPos, 0.0, 1.0); }\n",
        "precision mediump float; varying vec2 vUv; uniform sampler2D uTex;\n"
        "void main() { gl_FragColor = texture2D(uTex, vUv); }\n");
    sDisp.blitPos = glGetAttribLocation(sDisp.blitProgram, "aPos");
    sDisp.blitUv = glGetAttribLocation(sDisp.blitProgram, "aUv");

    // 2D layer: RGBA, no depth
    glGenTextures(1, &sDisp.tex2d);
    glBindTexture(GL_TEXTURE_2D, sDisp.tex2d);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, S30_2D_W, S30_2D_H, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenFramebuffers(1, &sDisp.fbo2d);
    glBindFramebuffer(GL_FRAMEBUFFER, sDisp.fbo2d);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sDisp.tex2d, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "display: 2D layer framebuffer incomplete\n");
        exit(1);
    }

    /* Composite: sharp bilinear (texels stay crisp, only the pixel straddling
       a texel boundary is blended), premultiplied alpha */
    sDisp.compProgram = S30Display_LinkProgram(
        "attribute vec2 aPos; attribute vec2 aUv; varying vec2 vUv;\n"
        "void main() { vUv = aUv; gl_Position = vec4(aPos, 0.0, 1.0); }\n",
        "precision mediump float; varying vec2 vUv; uniform sampler2D uTex;\n"
        "const vec2 size = vec2(360.0, 240.0);\n" // S30_2D_W x S30_2D_H
        "const vec2 scale = vec2(480.0 / 360.0, 320.0 / 240.0);\n"
        "void main() {\n"
        "  vec2 texel = vUv * size;\n"
        "  vec2 d = fract(texel) - 0.5;\n"
        "  vec2 r = 0.5 - 0.5 / scale;\n"
        "  vec2 f = (d - clamp(d, -r, r)) * scale + 0.5;\n"
        "  gl_FragColor = texture2D(uTex, (floor(texel) + f) / size);\n"
        "}\n");
    sDisp.compPos = glGetAttribLocation(sDisp.compProgram, "aPos");
    sDisp.compUv = glGetAttribLocation(sDisp.compProgram, "aUv");
}

void S30Display_Init(void) {
    open_kms();
    init_egl();
    init_fbo();
}

static void composite_2d(void) {
    static const GLfloat quad[] = { -1, -1, 0, 0, 1, -1, 1, 0, -1, 1, 0, 1, 1, 1, 1, 1 };

    glBindFramebuffer(GL_FRAMEBUFFER, sDisp.fbo);
    glViewport(0, 0, S30_FB_W, S30_FB_H);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(sDisp.compProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sDisp.tex2d);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glVertexAttribPointer(sDisp.compPos, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), quad);
    glVertexAttribPointer(sDisp.compUv, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), quad + 2);
    glEnableVertexAttribArray(sDisp.compPos);
    glEnableVertexAttribArray(sDisp.compUv);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // Clear the layer for the 2D drawn after the next 3D
    glBindFramebuffer(GL_FRAMEBUFFER, sDisp.fbo2d);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    sDisp.dirty2d = 0;
    sDisp.bound = 0;
}

/* Draws tex over the whole bound framebuffer (S30_FB_W x S30_FB_H), opaque */
static void blit(GLuint tex) {
    static const GLfloat quad[] = { -1, -1, 0, 0, 1, -1, 1, 0, -1, 1, 0, 1, 1, 1, 1, 1 };

    glViewport(0, 0, S30_FB_W, S30_FB_H);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDisable(GL_BLEND);
    glUseProgram(sDisp.blitProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glVertexAttribPointer(sDisp.blitPos, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), quad);
    glVertexAttribPointer(sDisp.blitUv, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), quad + 2);
    glEnableVertexAttribArray(sDisp.blitPos);
    glEnableVertexAttribArray(sDisp.blitUv);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray(sDisp.blitPos);
    glDisableVertexAttribArray(sDisp.blitUv);
}

/* Copies the 3D image (pending 2D composited in) into *tex, creating it and
   its framebuffer *fbo on first use */
static void copy_frame(GLuint* tex, GLuint* fbo) {
    if (sDisp.dirty2d) {
        composite_2d();
    }
    if (*fbo == 0) {
        glGenTextures(1, tex);
        glBindTexture(GL_TEXTURE_2D, *tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, S30_FB_W, S30_FB_H, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glGenFramebuffers(1, fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, *fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, *tex, 0);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, *fbo);
    sDisp.bound = 0;
    blit(sDisp.fboTex);
}

void S30Display_SaveBackground(void) {
    copy_frame(&sDisp.bgTex, &sDisp.bgFbo);
}

void S30Display_RestoreBackground(void) {
    S30Display_Bind3D();
    if (sDisp.bgFbo != 0) {
        blit(sDisp.bgTex);
    }
}

/* VisMono (z_vismono.c): the frame's intensity (R:G:B weighted 2:4:1) picks
   a color between env and prim, blended over the frame by prim alpha */
void S30Display_VisMono(const float prim[4], const float env[4]) {
    static const GLfloat quad[] = { -1, -1, 0, 0, 1, -1, 1, 0, -1, 1, 0, 1, 1, 1, 1, 1 };

    copy_frame(&sDisp.monoSrcTex, &sDisp.monoSrcFbo);
    if (sDisp.monoProgram == 0) {
        sDisp.monoProgram = S30Display_LinkProgram(
            "attribute vec2 aPos; attribute vec2 aUv; varying vec2 vUv;\n"
            "void main() { vUv = aUv; gl_Position = vec4(aPos, 0.0, 1.0); }\n",
            "precision mediump float; varying vec2 vUv; uniform sampler2D uTex;\n"
            "uniform vec4 uPrim; uniform vec3 uEnv;\n"
            "void main() {\n"
            "  vec3 c = texture2D(uTex, vUv).rgb;\n"
            "  float i = dot(c, vec3(2.0, 4.0, 1.0) / 7.0);\n"
            "  gl_FragColor = vec4(mix(c, mix(uEnv, uPrim.rgb, i), uPrim.a), 1.0);\n"
            "}\n");
        sDisp.monoPos = glGetAttribLocation(sDisp.monoProgram, "aPos");
        sDisp.monoUv = glGetAttribLocation(sDisp.monoProgram, "aUv");
        sDisp.monoPrim = glGetUniformLocation(sDisp.monoProgram, "uPrim");
        sDisp.monoEnv = glGetUniformLocation(sDisp.monoProgram, "uEnv");
    }
    S30Display_Bind3D();
    glViewport(0, 0, S30_FB_W, S30_FB_H);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDisable(GL_BLEND);
    glUseProgram(sDisp.monoProgram);
    glUniform4fv(sDisp.monoPrim, 1, prim);
    glUniform3fv(sDisp.monoEnv, 1, env);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sDisp.monoSrcTex);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glVertexAttribPointer(sDisp.monoPos, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), quad);
    glVertexAttribPointer(sDisp.monoUv, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), quad + 2);
    glEnableVertexAttribArray(sDisp.monoPos);
    glEnableVertexAttribArray(sDisp.monoUv);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray(sDisp.monoPos);
    glDisableVertexAttribArray(sDisp.monoUv);
}

/* Depth probes (the graph callback's sun and light glow tests): queued
   during a frame, drawn at its end against its depth buffer (each pixel
   cleared, then a full-screen quad at the probe's depth through a 1-pixel
   scissor), and gathered into one row of a small texture, read at the next
   frame's start when that frame is complete: no GPU stall. */
#define MAX_PROBES 32

static struct {
    int x, y;
    float z;
    int slot;
} sProbes[MAX_PROBES];
static int sNumProbes;
static int sProbeSlots; // result slots drawn by the last frame
static GLuint sProbeTex, sProbeFbo, sProbeProgram, sGatherProgram;
static GLint sProbePos, sProbeZ, sGatherPos, sGatherUv;

void S30Display_QueueProbe(int slot, int x, int y, float z) {
    if (sNumProbes < MAX_PROBES && slot < MAX_PROBES) {
        sProbes[sNumProbes].x = x;
        sProbes[sNumProbes].y = y;
        sProbes[sNumProbes].z = z;
        sProbes[sNumProbes].slot = slot;
        sNumProbes++;
    }
}

static void draw_probes(void) {
    static const GLfloat quad[] = { -1, -1, 1, -1, -1, 1, 1, 1 };
    GLfloat pts[MAX_PROBES * 4];
    int i;

    sProbeSlots = 0;
    if (sNumProbes == 0) {
        return;
    }
    if (sProbeProgram == 0) {
        sProbeProgram = S30Display_LinkProgram("attribute vec2 aPos; uniform float uZ;\n"
                                               "void main() { gl_Position = vec4(aPos, uZ, 1.0); }\n",
                                               "precision mediump float;\n"
                                               "void main() { gl_FragColor = vec4(1.0); }\n");
        sProbePos = glGetAttribLocation(sProbeProgram, "aPos");
        sProbeZ = glGetUniformLocation(sProbeProgram, "uZ");
        sGatherProgram = S30Display_LinkProgram(
            "attribute vec2 aPos; attribute vec2 aUv; varying vec2 vUv;\n"
            "void main() { vUv = aUv; gl_PointSize = 1.0; gl_Position = vec4(aPos, 0.0, 1.0); }\n",
            "precision mediump float; varying vec2 vUv; uniform sampler2D uTex;\n"
            "void main() { gl_FragColor = texture2D(uTex, vUv); }\n");
        sGatherPos = glGetAttribLocation(sGatherProgram, "aPos");
        sGatherUv = glGetAttribLocation(sGatherProgram, "aUv");
        glGenTextures(1, &sProbeTex);
        glBindTexture(GL_TEXTURE_2D, sProbeTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, MAX_PROBES, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glGenFramebuffers(1, &sProbeFbo);
        glBindFramebuffer(GL_FRAMEBUFFER, sProbeFbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sProbeTex, 0);
    }

    // Depth tests in the (already presented) 3D image
    glBindFramebuffer(GL_FRAMEBUFFER, sDisp.fbo);
    glViewport(0, 0, S30_FB_W, S30_FB_H);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glEnable(GL_SCISSOR_TEST);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glClearColor(0, 0, 0, 1);
    glUseProgram(sProbeProgram);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glVertexAttribPointer(sProbePos, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), quad);
    glEnableVertexAttribArray(sProbePos);
    for (i = 0; i < sNumProbes; i++) {
        glScissor(sProbes[i].x, sProbes[i].y, 1, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        glUniform1f(sProbeZ, sProbes[i].z);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }
    glDisableVertexAttribArray(sProbePos);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);

    // Gather: result of probe i to texel (slot, 0); slots not probed read 0
    glBindFramebuffer(GL_FRAMEBUFFER, sProbeFbo);
    glViewport(0, 0, MAX_PROBES, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    for (i = 0; i < sNumProbes; i++) {
        pts[i * 4 + 0] = (sProbes[i].slot + 0.5f) * 2.0f / MAX_PROBES - 1.0f;
        pts[i * 4 + 1] = 0.0f;
        pts[i * 4 + 2] = (sProbes[i].x + 0.5f) / S30_FB_W;
        pts[i * 4 + 3] = (sProbes[i].y + 0.5f) / S30_FB_H;
        if (sProbes[i].slot + 1 > sProbeSlots) {
            sProbeSlots = sProbes[i].slot + 1;
        }
    }
    glUseProgram(sGatherProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sDisp.fboTex);
    glVertexAttribPointer(sGatherPos, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), pts);
    glVertexAttribPointer(sGatherUv, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), pts + 2);
    glEnableVertexAttribArray(sGatherPos);
    glEnableVertexAttribArray(sGatherUv);
    glDrawArrays(GL_POINTS, 0, sNumProbes);
    glDisableVertexAttribArray(sGatherPos);
    glDisableVertexAttribArray(sGatherUv);
    sNumProbes = 0;
}

int S30Display_ReadProbes(unsigned char* out, int max) {
    unsigned char px[MAX_PROBES * 4];
    int i, n = sProbeSlots < max ? sProbeSlots : max;

    if (n == 0) {
        return 0;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, sProbeFbo);
    sDisp.bound = 0;
    glReadPixels(0, 0, n, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
    for (i = 0; i < n; i++) {
        out[i] = px[i * 4] > 127;
    }
    return n;
}

void S30Display_Unbind(void) {
    sDisp.bound = 0;
}

void S30Display_BeginFrame(void) {
    glBindFramebuffer(GL_FRAMEBUFFER, sDisp.fbo2d);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    sDisp.dirty2d = 0;
    glBindFramebuffer(GL_FRAMEBUFFER, sDisp.fbo);
    glViewport(0, 0, S30_FB_W, S30_FB_H);
    sDisp.bound = 1;
}

int S30Display_Bind3D(void) {
    int composited = sDisp.dirty2d;

    if (sDisp.dirty2d) {
        composite_2d();
    }
    if (sDisp.bound != 1) {
        glBindFramebuffer(GL_FRAMEBUFFER, sDisp.fbo);
        sDisp.bound = 1;
    }
    return composited;
}

void S30Display_Bind2D(void) {
    if (sDisp.bound != 2) {
        glBindFramebuffer(GL_FRAMEBUFFER, sDisp.fbo2d);
        sDisp.bound = 2;
    }
    sDisp.dirty2d = 1;
}

static void page_flip_handler(int fd, unsigned int seq, unsigned int sec, unsigned int usec, void* data) {
    sDisp.lastFlipSeq = seq;
    *(int*)data = 0;
}

static void drm_fb_destroy(struct gbm_bo* bo, void* data) {
    uint32_t fbId = (uint32_t)(uintptr_t)data;

    drmModeRmFB(sDisp.fd, fbId);
}

static uint32_t bo_fb(struct gbm_bo* bo) {
    uint32_t fbId = (uint32_t)(uintptr_t)gbm_bo_get_user_data(bo);

    if (fbId == 0) {
        uint32_t handles[4] = { gbm_bo_get_handle(bo).u32 };
        uint32_t strides[4] = { gbm_bo_get_stride(bo) };
        uint32_t offsets[4] = { 0 };

        if (drmModeAddFB2(sDisp.fd, gbm_bo_get_width(bo), gbm_bo_get_height(bo), GBM_FORMAT_XRGB8888,
                          handles, strides, offsets, &fbId, 0) != 0) {
            die("drmModeAddFB2");
        }
        gbm_bo_set_user_data(bo, (void*)(uintptr_t)fbId, drm_fb_destroy);
    }
    return fbId;
}

/* Debug: OOT_SHOT=N saves the offscreen image every N frames to
   /tmp/oot_shot.ppm (bottom-up rows flipped to top-down); SIGUSR1 saves the
   next frame */
static volatile sig_atomic_t sShotRequested;

static void on_sigusr1(int sig) {
    (void)sig;
    sShotRequested = 1;
}

static void maybe_screenshot(void) {
    static int every = -1, frame;
    static unsigned char px[S30_FB_W * S30_FB_H * 4];
    FILE* f;
    int y;

    if (every < 0) {
        every = getenv("OOT_SHOT") ? atoi(getenv("OOT_SHOT")) : 0;
        signal(SIGUSR1, on_sigusr1);
    }
    frame++;
    if (!sShotRequested && (every <= 0 || frame % every != 0)) {
        return;
    }
    sShotRequested = 0;
    glReadPixels(0, 0, S30_FB_W, S30_FB_H, GL_RGBA, GL_UNSIGNED_BYTE, px);
    f = fopen("/tmp/oot_shot.ppm.tmp", "wb");
    if (f == NULL) {
        return;
    }
    fprintf(f, "P6\n%d %d\n255\n", S30_FB_W, S30_FB_H);
    for (y = S30_FB_H - 1; y >= 0; y--) {
        int x;
        for (x = 0; x < S30_FB_W; x++) {
            fwrite(&px[(y * S30_FB_W + x) * 4], 1, 3, f);
        }
    }
    fclose(f);
    rename("/tmp/oot_shot.ppm.tmp", "/tmp/oot_shot.ppm");
}

void S30Display_EndFrame(void) {
    /* Landscape 480x320 view of the portrait panel, filled by the image. In
     * panel (portrait) NDC the landscape x axis runs along the panel's y
     * axis. */
    const float w = 1.0f;
    /* aPos (panel NDC), aUv: rotated 90 degrees (game top-left at panel
     * x = -1, y = -w) */
    const GLfloat quad[] = {
        -1.0f, -w, 0.0f, 1.0f,
        1.0f,  -w, 0.0f, 0.0f,
        -1.0f, w,  1.0f, 1.0f,
        1.0f,  w,  1.0f, 0.0f,
    };
    struct gbm_bo* bo;
    int waiting = 1;
    drmEventContext ev = { .version = 2, .page_flip_handler = page_flip_handler };
    drmVBlank vbl;

    if (sDisp.dirty2d) {
        composite_2d();
    }
    glBindFramebuffer(GL_FRAMEBUFFER, sDisp.fbo);
    maybe_screenshot();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    sDisp.bound = 0;
    glViewport(0, 0, sDisp.mode.hdisplay, sDisp.mode.vdisplay);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_CULL_FACE);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(sDisp.blitProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sDisp.fboTex);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glVertexAttribPointer(sDisp.blitPos, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), quad);
    glVertexAttribPointer(sDisp.blitUv, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), quad + 2);
    glEnableVertexAttribArray(sDisp.blitPos);
    glEnableVertexAttribArray(sDisp.blitUv);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray(sDisp.blitPos);
    glDisableVertexAttribArray(sDisp.blitUv);
    glDisableVertexAttribArray(sDisp.compPos);
    glDisableVertexAttribArray(sDisp.compUv);
    draw_probes();

    eglSwapBuffers(sDisp.eglDisplay, sDisp.eglSurface);
    bo = gbm_surface_lock_front_buffer(sDisp.surface);

    if (sDisp.curBo == NULL) {
        if (drmModeSetCrtc(sDisp.fd, sDisp.crtcId, bo_fb(bo), 0, 0, &sDisp.connectorId, 1, &sDisp.mode) != 0) {
            die("drmModeSetCrtc");
        }
    } else {
        /* Hold the frame until sVblanksPerFrame refreshes after the last
           flip (OOT_FAST: run at panel rate, for tests) */
        static int fast = -1;
        if (fast < 0) {
            fast = getenv("OOT_FAST") != NULL;
        }
        if (!fast) {
            memset(&vbl, 0, sizeof(vbl));
            vbl.request.type = DRM_VBLANK_ABSOLUTE;
            vbl.request.sequence = sDisp.lastFlipSeq + sVblanksPerFrame - 1;
            drmWaitVBlank(sDisp.fd, &vbl);
        }

        if (drmModePageFlip(sDisp.fd, sDisp.crtcId, bo_fb(bo), DRM_MODE_PAGE_FLIP_EVENT, &waiting) != 0) {
            die("drmModePageFlip");
        }
        while (waiting) {
            drmHandleEvent(sDisp.fd, &ev);
        }
        gbm_surface_release_buffer(sDisp.surface, sDisp.curBo);
    }
    sDisp.curBo = bo;
}
