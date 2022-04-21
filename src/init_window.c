// gcc -o test init_window.c -I. -lwayland-client -lwayland-server -lwayland-client-protocol -lwayland-egl -lEGL -lGLESv2
#include <wayland-client.h>
#include <wayland-server.h>
#include <wayland-client-protocol.h>
#include <wayland-egl.h> // Wayland EGL MUST be included before EGL headers

#include "init_window.h"
#include "log.h"

#include <string.h>
#include <stdlib.h>

#include <GLES2/gl2.h>

#define PROJECTION_FAR        30.0f
#define PROJECTION_FOVY        30.0f
#define PROJECTION_NEAR        0.1f
#define PI                    3.1415926534f
#define BUFFER_OFFSET(i) ((char *)NULL + (i))

static float angle = 0.0f;

static GLuint vertexID = 0;
static GLuint indiceID = 0;
static GLuint program_object = 0;
static GLuint position_loc = 0;
static GLuint color_loc = 0;
static GLuint mvp_matrix_loc = 0;

typedef struct
{
    GLfloat m[4][4];
}glMatrix;

static glMatrix projection;
static glMatrix modelview;
static glMatrix mvp;

static const GLushort indices[] =
{
    0, 2, 3, 0, 3, 1,    // front
    2, 4, 5, 2, 5, 3,    // left
    4, 6, 7, 4, 7, 5,    // back
    6, 0, 1, 6, 1, 7,    // right
    0, 6, 4, 0, 4, 2,    // top
    1, 3, 5, 1, 5, 7     // bottom
};

static const GLfloat vertices[] =
{
     0.5f,  0.5f, -0.5f, 1.0f, 1.0f, 1.0f,        // 0
     0.5f, -0.5f, -0.5f, 1.0f,    0,    0,        // 1
    -0.5f,  0.5f, -0.5f, 1.0f, 1.0f,    0,        // 2
    -0.5f, -0.5f, -0.5f, 1.0f,    0, 1.0f,        // 3
    -0.5f,  0.5f,  0.5f,    0, 1.0f, 1.0f,        // 4
    -0.5f, -0.5f,  0.5f,    0, 1.0f,    0,        // 5
     0.5f,  0.5f,  0.5f,    0,    0, 1.0f,        // 6
     0.5f, -0.5f,  0.5f,  0.5f, 1.0f, 0.5         // 7
};

static void InitShader(void);
static void InitializeRender(int width, int height);
static void Render(void);
static void FinalizeRender();

static void TranslateMatrix(glMatrix *result, float tx, float ty, float tz);
static void MultiplyMatrix(glMatrix *result, glMatrix *srcA, glMatrix *srcB);
static void RotationMatrix(glMatrix *result, float angle, float x, float y, float z);
static void LoadIdentityMatrix(glMatrix *result);
static void PerspectiveMatrix(glMatrix *result, float fovy, float aspect, float zNear, float zFar);

struct wl_compositor *compositor = NULL;
struct wl_surface *surface;
struct wl_egl_window *egl_window;
struct wl_region *region;
struct wl_shell *shell;
struct wl_shell_surface *shell_surface;

struct _escontext ESContext = {
  .native_display = NULL,
  .window_width = 0,
  .window_height = 0,
  .native_window  = 0,
  .display = NULL,
  .context = NULL,
  .surface = NULL
};

#define TRUE 1
#define FALSE 0

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

void CreateNativeWindow(char *title, int width, int height) {

  region = wl_compositor_create_region(compositor);

  wl_region_add(region, 0, 0, width, height);
  wl_surface_set_opaque_region(surface, region);

  struct wl_egl_window *egl_window =
    wl_egl_window_create(surface, width, height);

  if (egl_window == EGL_NO_SURFACE) {
    LOG("No window !?\n");
    exit(1);
  }
  else LOG("Window created !\n");
  ESContext.window_width = width;
  ESContext.window_height = height;
  ESContext.native_window = egl_window;

}

EGLBoolean CreateEGLContext ()
{
   EGLint numConfigs;
   EGLint majorVersion;
   EGLint minorVersion;
   EGLContext context;
   EGLSurface surface;
   EGLConfig config;
   EGLint fbAttribs[] =
   {
       EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
       EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
       EGL_RED_SIZE,        8,
       EGL_GREEN_SIZE,      8,
       EGL_BLUE_SIZE,       8,
       EGL_NONE
   };
   EGLint contextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE, EGL_NONE };
   EGLDisplay display = eglGetDisplay( ESContext.native_display );
   if ( display == EGL_NO_DISPLAY )
   {
      LOG("No EGL Display...\n");
      return EGL_FALSE;
   }

   // Initialize EGL
   if ( !eglInitialize(display, &majorVersion, &minorVersion) )
   {
      LOG("No Initialisation...\n");
      return EGL_FALSE;
   }

   // Get configs
   if ( (eglGetConfigs(display, NULL, 0, &numConfigs) != EGL_TRUE) || (numConfigs == 0))
   {
      LOG("No configuration...\n");
      return EGL_FALSE;
   }

   // Choose config
   if ( (eglChooseConfig(display, fbAttribs, &config, 1, &numConfigs) != EGL_TRUE) || (numConfigs != 1))
   {
      LOG("No configuration...\n");
      return EGL_FALSE;
   }

   // Create a surface
   surface = eglCreateWindowSurface(display, config, ESContext.native_window, NULL);
   if ( surface == EGL_NO_SURFACE )
   {
      LOG("No surface...\n");
      return EGL_FALSE;
   }

   // Create a GL context
   context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttribs );
   if ( context == EGL_NO_CONTEXT )
   {
      LOG("No context...\n");
      return EGL_FALSE;
   }

   // Make the context current
   if ( !eglMakeCurrent(display, surface, surface, context) )
   {
      LOG("Could not make the current window current !\n");
      return EGL_FALSE;
   }

   ESContext.display = display;
   ESContext.surface = surface;
   ESContext.context = context;
   return EGL_TRUE;
}

void shell_surface_ping
(void *data, struct wl_shell_surface *shell_surface, uint32_t serial) {
  wl_shell_surface_pong(shell_surface, serial);
}

void shell_surface_configure
(void *data, struct wl_shell_surface *shell_surface, uint32_t edges,
 int32_t width, int32_t height) {
  struct window *window = data;
  wl_egl_window_resize(ESContext.native_window, width, height, 0, 0);
}

void shell_surface_popup_done(void *data, struct wl_shell_surface *shell_surface) {
}

static struct wl_shell_surface_listener shell_surface_listener = {
  &shell_surface_ping,
  &shell_surface_configure,
  &shell_surface_popup_done
};

EGLBoolean CreateWindowWithEGLContext(char *title, int width, int height) {
  CreateNativeWindow(title, width, height);
  return CreateEGLContext();
}

// void draw() {
//   glClearColor(0.5, 0.3, 0.0, 1.0);
//   glClear(GL_COLOR_BUFFER_BIT);
// }

unsigned long last_click = 0;
void RefreshWindow() { eglSwapBuffers(ESContext.display, ESContext.surface); }

static void global_registry_handler
(void *data, struct wl_registry *registry, uint32_t id,
 const char *interface, uint32_t version) {
  LOG("Got a registry event for %s id %d\n", interface, id);
  if (strcmp(interface, "wl_compositor") == 0)
    compositor =
      wl_registry_bind(registry, id, &wl_compositor_interface, 1);

  else if (strcmp(interface, "wl_shell") == 0)
    shell = wl_registry_bind(registry, id, &wl_shell_interface, 1);
}

static void global_registry_remover
(void *data, struct wl_registry *registry, uint32_t id) {
  LOG("Got a registry losing event for %d\n", id);
}

const struct wl_registry_listener listener = {
  global_registry_handler,
  global_registry_remover
};

static void
get_server_references() {

  struct wl_display * display = wl_display_connect(NULL);
  if (display == NULL) {
    LOG("Can't connect to wayland display !?\n");
    exit(1);
  }
  LOG("Got a display !");

  struct wl_registry *wl_registry =
    wl_display_get_registry(display);
  wl_registry_add_listener(wl_registry, &listener, NULL);

  // This call the attached listener global_registry_handler
  wl_display_dispatch(display);
  wl_display_roundtrip(display);

  // If at this point, global_registry_handler didn't set the
  // compositor, nor the shell, bailout !
  if (compositor == NULL || shell == NULL) {
    LOG("No compositor !? No Shell !! There's NOTHING in here !\n");
    exit(1);
  }
  else {
    LOG("Okay, we got a compositor and a shell... That's something !\n");
    ESContext.native_display = display;
  }
}

void destroy_window() {
  eglDestroySurface(ESContext.display, ESContext.surface);
  wl_egl_window_destroy(ESContext.native_window);
  wl_shell_surface_destroy(shell_surface);
  wl_surface_destroy(surface);
  eglDestroyContext(ESContext.display, ESContext.context);
}

static void
InitShader(void)
{
    /* The shaders */
    const char vShaderStr[] =
        "uniform mat4   u_mvpMatrix;               \n"
        "attribute vec4 a_position;                \n"
        "attribute vec4 a_color;                   \n"
        "varying vec4   v_color;                   \n"
        "                                          \n"
        "void main()                               \n"
        "{                                         \n"
        "  gl_Position = u_mvpMatrix * a_position; \n"
        "  v_color = a_color;                      \n"
        "}                                         \n";

    const char fShaderStr[] =
        "precision mediump float;                  \n"
        "varying vec4 v_color;                     \n"
        "                                          \n"
        "void main()                               \n"
        "{                                         \n"
        "  gl_FragColor = v_color;                 \n"
        "}                                         \n";

    GLuint v = glCreateShader(GL_VERTEX_SHADER);
    GLuint f = glCreateShader(GL_FRAGMENT_SHADER);

    const char* vv = vShaderStr;
    const char* ff = fShaderStr;

    glShaderSource(v, 1, &vv, NULL);
    glShaderSource(f, 1, &ff, NULL);

    /* Compile the shaders */
    glCompileShader(v);
    glCompileShader(f);

    program_object = glCreateProgram();

    glAttachShader(program_object, v);
    glAttachShader(program_object, f);

    /* Link the program */
    glLinkProgram(program_object);
}

static void
InitializeRender(int width, int height)
{
    /* The shaders */
    InitShader();

    /* Get the attribute locations */
    position_loc    = glGetAttribLocation(program_object, "a_position");
    color_loc        = glGetAttribLocation(program_object, "a_color");

    /* Get the uniform locations */
    mvp_matrix_loc = glGetUniformLocation(program_object, "u_mvpMatrix");

    glGenBuffers(1, &vertexID);
    glBindBuffer(GL_ARRAY_BUFFER, vertexID);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glGenBuffers(1, &indiceID);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indiceID);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    /* Init GL Status */
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClearDepthf(1.0f);

    /* Make Matrix */
    float aspect_ratio = (float)(width)/height;
    LoadIdentityMatrix(&projection);
    LoadIdentityMatrix(&modelview);
    LoadIdentityMatrix(&mvp);

    PerspectiveMatrix(&projection, PROJECTION_FOVY, aspect_ratio, PROJECTION_NEAR, PROJECTION_FAR);

    /* Viewport */
    glViewport(0,0,width, height);
}

static void
Render(void)
{
    LoadIdentityMatrix(&modelview);
    TranslateMatrix(&modelview, 0.0f, 0.0f, -4.0f);
    RotationMatrix(&modelview, angle, 1.0f, 1.0f, 0.0);

    angle+=0.3f;
    if(angle > 360.0f) {
        angle-=360.0f;
    }

    /* Compute the final MVP by multiplying the model-view and perspective matrices together */
    MultiplyMatrix(&mvp, &modelview, &projection);

    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

    glUseProgram(program_object);

    /* Enable cube array */
    glBindBuffer(GL_ARRAY_BUFFER, vertexID);

    glVertexAttribPointer(position_loc, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), BUFFER_OFFSET(0));
    glVertexAttribPointer(color_loc,    3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), BUFFER_OFFSET(3 * sizeof(GLfloat)));

    glEnableVertexAttribArray(position_loc);
    glEnableVertexAttribArray(color_loc);

    /* Load the MVP matrix */
    glUniformMatrix4fv(mvp_matrix_loc, 1, GL_FALSE, (GLfloat*)&mvp.m[0][0]);

    /* Finally draw the elements */
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indiceID);
    glDrawElements(GL_TRIANGLES, sizeof(indices) / sizeof(GLushort), GL_UNSIGNED_SHORT, BUFFER_OFFSET(0));
}

static void
FinalizeRender()
{
    glDeleteProgram(program_object);
    glDeleteBuffers(1, &vertexID);
    glDeleteBuffers(1, &indiceID);

    /* screen clear by black for another application using opengles */
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    /* clear buffer */
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
}

static void
TranslateMatrix(glMatrix *result, float tx, float ty, float tz)
{
    result->m[3][0] += (result->m[0][0] * tx + result->m[1][0] * ty + result->m[2][0] * tz);
    result->m[3][1] += (result->m[0][1] * tx + result->m[1][1] * ty + result->m[2][1] * tz);
    result->m[3][2] += (result->m[0][2] * tx + result->m[1][2] * ty + result->m[2][2] * tz);
    result->m[3][3] += (result->m[0][3] * tx + result->m[1][3] * ty + result->m[2][3] * tz);
}

static void
MultiplyMatrix(glMatrix *result, glMatrix *srcA, glMatrix *srcB)
{
    glMatrix tmp;
    int i;

    for (i=0; i<4; i++)    {
        tmp.m[i][0] =    (srcA->m[i][0] * srcB->m[0][0]) +
                        (srcA->m[i][1] * srcB->m[1][0]) +
                        (srcA->m[i][2] * srcB->m[2][0]) +
                        (srcA->m[i][3] * srcB->m[3][0]) ;

        tmp.m[i][1] =    (srcA->m[i][0] * srcB->m[0][1]) +
                        (srcA->m[i][1] * srcB->m[1][1]) +
                        (srcA->m[i][2] * srcB->m[2][1]) +
                        (srcA->m[i][3] * srcB->m[3][1]) ;

        tmp.m[i][2] =    (srcA->m[i][0] * srcB->m[0][2]) +
                        (srcA->m[i][1] * srcB->m[1][2]) +
                        (srcA->m[i][2] * srcB->m[2][2]) +
                        (srcA->m[i][3] * srcB->m[3][2]) ;

        tmp.m[i][3] =    (srcA->m[i][0] * srcB->m[0][3]) +
                        (srcA->m[i][1] * srcB->m[1][3]) +
                        (srcA->m[i][2] * srcB->m[2][3]) +
                        (srcA->m[i][3] * srcB->m[3][3]) ;
    }

    memcpy(result, &tmp, sizeof(glMatrix));
}

static void
RotationMatrix(glMatrix *result, float angle, float x, float y, float z)
{
    float sinAngle, cosAngle;
    float mag = sqrtf(x * x + y * y + z * z);

    sinAngle = sinf(angle * (float)M_PI / 180.0f);
    cosAngle = cosf(angle * (float)M_PI / 180.0f);

    if (mag > 0.0f)    {
        float xx, yy, zz, xy, yz, zx, xs, ys, zs;
        float oneMinusCos;
        glMatrix rotMat;

        x /= mag;
        y /= mag;
        z /= mag;

        xx = x * x;
        yy = y * y;
        zz = z * z;
        xy = x * y;
        yz = y * z;
        zx = z * x;
        xs = x * sinAngle;
        ys = y * sinAngle;
        zs = z * sinAngle;
        oneMinusCos = 1.0f - cosAngle;

        rotMat.m[0][0] = (oneMinusCos * xx) + cosAngle;
        rotMat.m[1][0] = (oneMinusCos * xy) - zs;
        rotMat.m[2][0] = (oneMinusCos * zx) + ys;
        rotMat.m[3][0] = 0.0F;

        rotMat.m[0][1] = (oneMinusCos * xy) + zs;
        rotMat.m[1][1] = (oneMinusCos * yy) + cosAngle;
        rotMat.m[2][1] = (oneMinusCos * yz) - xs;
        rotMat.m[3][1] = 0.0F;

        rotMat.m[0][2] = (oneMinusCos * zx) - ys;
        rotMat.m[1][2] = (oneMinusCos * yz) + xs;
        rotMat.m[2][2] = (oneMinusCos * zz) + cosAngle;
        rotMat.m[3][2] = 0.0F;

        rotMat.m[0][3] = 0.0F;
        rotMat.m[1][3] = 0.0F;
        rotMat.m[2][3] = 0.0F;
        rotMat.m[3][3] = 1.0F;

        MultiplyMatrix(result, &rotMat, result);
    }
}

static void
LoadIdentityMatrix(glMatrix *result)
{
    memset(result, 0x0, sizeof(glMatrix));
    result->m[0][0] = 1.0f;
    result->m[1][1] = 1.0f;
    result->m[2][2] = 1.0f;
    result->m[3][3] = 1.0f;
}

static void
PerspectiveMatrix(glMatrix *result, float fovy, float aspect, float zNear, float zFar)
{
    glMatrix m;
    float sine, cotangent, deltaZ;
    float radians = fovy / 2.0f * (float)M_PI / 180.0f;

    deltaZ = zFar - zNear;
    sine = sinf(radians);
    if ((deltaZ == 0) || (sine == 0) || (aspect == 0)) {
        return;
    }
    cotangent = cosf(radians) / sine;

    m.m[0][0] = cotangent / aspect; m.m[0][1] =                          0; m.m[0][2] =                          0; m.m[0][3] =  0;
    m.m[1][0] =                  0; m.m[1][1] =                  cotangent; m.m[1][2] =                          0; m.m[1][3] =  0;
    m.m[2][0] =                  0; m.m[2][1] =                          0; m.m[2][2] =   -(zFar + zNear) / deltaZ; m.m[2][3] = -1;
    m.m[3][0] =                  0; m.m[3][1] =                          0; m.m[3][2] = -2 * zNear * zFar / deltaZ; m.m[3][3] =  0;

    MultiplyMatrix(result, &m, result);
}

int main() {
  get_server_references();

  surface = wl_compositor_create_surface(compositor);
  if (surface == NULL) {
    LOG("No Compositor surface ! Yay....\n");
    exit(1);
  }
  else LOG("Got a compositor surface !\n");

  shell_surface = wl_shell_get_shell_surface(shell, surface);
  wl_shell_surface_set_title(shell_surface, "test_title");
  wl_shell_surface_set_toplevel(shell_surface);

  CreateWindowWithEGLContext("Nya", 1280, 720);

  InitializeRender(WINDOW_WIDTH, WINDOW_HEIGHT);

  while (1) {
    wl_display_dispatch_pending(ESContext.native_display);
    // draw(); // simple draw for the test.
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    Render();
    RefreshWindow();
  }

  FinalizeRender();

  wl_display_disconnect(ESContext.native_display);
  LOG("Display disconnected !\n");

  exit(0);
}
