//
// libshapes: high-level OpenVG API
// Anthony Starks (ajstarks@gmail.com)
//
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <time.h>
#include <jpeglib.h>

#include "VG/openvg.h"
#include "VG/vgu.h"
#include "EGL/egl.h"
#include "GLES/gl.h"
#include "DejaVuSans.inc"                         // font data
#include "DejaVuSerif.inc"
#include "DejaVuSansMono.inc"
#include "eglstate.h"                             // data structures for graphics state
#include "fontinfo.h"                             // font data structure
static STATE_T _state, *state=&_state;            // global graphics state
static const int MAXFONTPATH=256;

//
// Font functions
//

// loadfont loads font path data
// derived from http://web.archive.org/web/20070808195131/http://developer.hybrid.fi/font2openvg/renderFont.cpp.txt
Fontinfo loadfont(
	const int *Points,
	const int *PointIndices,
	const unsigned char *Instructions,
	const int *InstructionIndices,
	const int *InstructionCounts,
	const int *adv,
	const short *cmap,
	int ng) {

    Fontinfo f;
    int i;

    memset(f.Glyphs, 0, MAXFONTPATH*sizeof(VGPath));
    if (ng > MAXFONTPATH) {
        return f;
    }
    for(i=0; i < ng; i++) {
        const int* p = &Points[PointIndices[i]*2];
        const unsigned char* instructions = &Instructions[InstructionIndices[i]];
        int ic = InstructionCounts[i];
        VGPath path = vgCreatePath(VG_PATH_FORMAT_STANDARD, VG_PATH_DATATYPE_S_32, 1.0f/65536.0f, 0.0f, 0, 0, VG_PATH_CAPABILITY_ALL);
        f.Glyphs[i] = path;
        if(ic) {
            vgAppendPathData(path, ic, instructions, p);
        }
    }
    f.CharacterMap = cmap;
    f.GlyphAdvances = adv;
    f.Count = ng;
    return f;
}


// unloadfont frees font path data
void unloadfont(VGPath *glyphs, int n) {
    int i;
    for(i=0; i<n; i++) {
        vgDestroyPath(glyphs[i]);
    }
}

// createImageFromJpeg decompresses a JPEG image to the standard image format
// source: https://github.com/ileben/ShivaVG/blob/master/examples/test_image.c
VGImage createImageFromJpeg(const char *filename) {
  FILE *infile;
  struct jpeg_decompress_struct jdc;
  struct jpeg_error_mgr jerr;
  JSAMPARRAY buffer;  
  unsigned int bstride;
  unsigned int bbpp;

  VGImage img;
  VGubyte *data;
  unsigned int width;
  unsigned int height;
  unsigned int dstride;
  unsigned int dbpp;
  
  VGubyte *brow;
  VGubyte *drow;
  unsigned int x;
  unsigned int lilEndianTest = 1;
  VGImageFormat rgbaFormat;

  // Check for endianness
  if (((unsigned char*)&lilEndianTest)[0] == 1)
    rgbaFormat = VG_sABGR_8888;
  else rgbaFormat = VG_sRGBA_8888;

  // Try to open image file
  infile = fopen(filename, "rb");
  if (infile == NULL) {
    printf("Failed opening '%s' for reading!\n", filename);
    return VG_INVALID_HANDLE; }
  
  // Setup default error handling
  jdc.err = jpeg_std_error(&jerr);
  jpeg_create_decompress(&jdc);
  
  // Set input file
  jpeg_stdio_src(&jdc, infile);
  
  // Read header and start
  jpeg_read_header(&jdc, TRUE);
  jpeg_start_decompress(&jdc);
  width = jdc.output_width;
  height = jdc.output_height;
  
  // Allocate buffer using jpeg allocator
  bbpp = jdc.output_components;
  bstride = width * bbpp;
  buffer = (*jdc.mem->alloc_sarray)
    ((j_common_ptr) &jdc, JPOOL_IMAGE, bstride, 1);
  
  // Allocate image data buffer
  dbpp = 4;
  dstride = width * dbpp;
  data = (VGubyte*)malloc(dstride * height);
  
  // Iterate until all scanlines processed
  while (jdc.output_scanline < height) {
    
    // Read scanline into buffer
    jpeg_read_scanlines(&jdc, buffer, 1);    
    drow = data + (height-jdc.output_scanline) * dstride;
    brow = buffer[0];
    // Expand to RGBA
    for (x=0; x<width; ++x, drow+=dbpp, brow+=bbpp) {
      switch (bbpp) {
      case 4:
        drow[0] = brow[0];
        drow[1] = brow[1];
        drow[2] = brow[2];
        drow[3] = brow[3];
        break;
      case 3:
        drow[0] = brow[0];
        drow[1] = brow[1];
        drow[2] = brow[2];
        drow[3] = 255;
        break; }
    }
  }
  
  // Create VG image
  img = vgCreateImage(rgbaFormat, width, height, VG_IMAGE_QUALITY_BETTER);
  vgImageSubData(img, data, dstride, rgbaFormat, 0, 0, width, height);
  
  // Cleanup
  jpeg_destroy_decompress(&jdc);
  fclose(infile);
  free(data);
  
  return img;
}

// Image places an image at the specifed location
void Image(VGfloat x, VGfloat y, int w, int h, char * filename) {
	VGImage img = createImageFromJpeg(filename);
	vgSetPixels(x, y, img, 0, 0, w, h);
	vgDestroyImage(img);
}

// dumpscreen writes the raster to the standard output file
void dumpscreen(int w, int h) {
    void *ScreenBuffer = malloc(w*h*4);
    vgReadPixels(ScreenBuffer, (w*4), VG_sABGR_8888, 0, 0, w, h);
    fwrite(ScreenBuffer, 1, w*h*4, stdout);
    free(ScreenBuffer);
}

// init sets the system to its initial state
void init(int *w, int *h) {
    bcm_host_init();
    memset( state, 0, sizeof( *state ) );
    oglinit(state);
    SansTypeface = loadfont(DejaVuSans_glyphPoints,
        DejaVuSans_glyphPointIndices,
        DejaVuSans_glyphInstructions,
        DejaVuSans_glyphInstructionIndices,
        DejaVuSans_glyphInstructionCounts,
        DejaVuSans_glyphAdvances,
        DejaVuSans_characterMap,
        DejaVuSans_glyphCount);

    SerifTypeface = loadfont(DejaVuSerif_glyphPoints,
        DejaVuSerif_glyphPointIndices,
        DejaVuSerif_glyphInstructions,
        DejaVuSerif_glyphInstructionIndices,
        DejaVuSerif_glyphInstructionCounts,
        DejaVuSerif_glyphAdvances,
        DejaVuSerif_characterMap,
        DejaVuSerif_glyphCount);

	MonoTypeface = loadfont(DejaVuSansMono_glyphPoints,
        DejaVuSansMono_glyphPointIndices,
        DejaVuSansMono_glyphInstructions,
        DejaVuSansMono_glyphInstructionIndices,
        DejaVuSansMono_glyphInstructionCounts,
        DejaVuSansMono_glyphAdvances,
        DejaVuSansMono_characterMap,
        DejaVuSansMono_glyphCount);

    *w = state->screen_width;
    *h = state->screen_height;
}


// finish cleans up
void finish() {
    unloadfont(SansTypeface.Glyphs, SansTypeface.Count);
    unloadfont(SerifTypeface.Glyphs, SerifTypeface.Count);
    glClear( GL_COLOR_BUFFER_BIT );
    eglSwapBuffers(state->display, state->surface);
    eglMakeCurrent( state->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT );
    eglDestroySurface( state->display, state->surface );
    eglDestroyContext( state->display, state->context );
    eglTerminate( state->display );
}


//
// Transformations
//

// Translate the coordinate system to x,y
void Translate(VGfloat x, VGfloat y) {
    vgTranslate(x, y);
}


// Rotate around angle r
void Rotate(VGfloat r) {
    vgRotate(r);
}


// Shear shears the x coordinate by x degrees, the y coordinate by y degrees
void Shear(VGfloat x, VGfloat y) {
    vgShear(x,y);
}


// Scale scales by  x, y
void Scale(VGfloat x, VGfloat y) {
    vgScale(x,y);
}


//
// Style functions
//

// setfill sets the fill color
void setfill(VGfloat color[4]) {
    VGPaint fillPaint = vgCreatePaint();
    vgSetParameteri(fillPaint, VG_PAINT_TYPE, VG_PAINT_TYPE_COLOR);
    vgSetParameterfv(fillPaint, VG_PAINT_COLOR, 4, color);
    vgSetPaint(fillPaint, VG_FILL_PATH);
    vgDestroyPaint(fillPaint);
}


// setstroke sets the stroke color
void setstroke(VGfloat color[4]) {
    VGPaint strokePaint = vgCreatePaint();
    vgSetParameteri(strokePaint, VG_PAINT_TYPE, VG_PAINT_TYPE_COLOR);
    vgSetParameterfv(strokePaint, VG_PAINT_COLOR, 4, color);
    vgSetPaint(strokePaint, VG_STROKE_PATH);
    vgDestroyPaint(strokePaint);
}


// strokeWidth sets the stroke width
void strokeWidth(VGfloat width) {
    vgSetf(VG_STROKE_LINE_WIDTH, width);
    vgSeti(VG_STROKE_CAP_STYLE, VG_CAP_BUTT);
    vgSeti(VG_STROKE_JOIN_STYLE, VG_JOIN_MITER);
}


// Text renders a string of text at a specified location, size, and fill, using the specified font glyphs
// derived from http://web.archive.org/web/20070808195131/http://developer.hybrid.fi/font2openvg/renderFont.cpp.txt
void Text(VGfloat x, VGfloat y, char* s, Fontinfo f, int pointsize, VGfloat fillcolor[4]) {
    float size = (float)pointsize;
    float xx = x;
    float mm[9];
    int i;
    vgGetMatrix(mm);
    setfill(fillcolor);
    for(i=0; i < (int)strlen(s); i++) {
        unsigned int character = (unsigned int)s[i];
        int glyph = f.CharacterMap[character];
        if( glyph == -1 ) {
            continue;                             //glyph is undefined
        }
        VGfloat mat[9] = {
            size, 0.0f, 0.0f,
            0.0f, size, 0.0f,
            xx, y, 1.0f
        };
        vgLoadMatrix(mm);
        vgMultMatrix(mat);
        vgDrawPath(f.Glyphs[glyph], VG_FILL_PATH);
        xx += size * f.GlyphAdvances[glyph] / 65536.0f;
    }
    vgLoadMatrix(mm);
}


// textwidth returns the width of a text string in a font
VGfloat textwidth(char *s, Fontinfo f, VGfloat size) {
    int i;
    VGfloat tw = 0.0;
    for(i=0; i < (int)strlen(s); i++) {
        unsigned int character = (unsigned int)s[i];
        int glyph = f.CharacterMap[character];
        if( glyph == -1 ) {
            continue;                             //glyph is undefined
        }
        tw += size * f.GlyphAdvances[glyph] / 65536.0f;
    }
    return tw;
}


//
// Shape functions
//

// newpath creates path data
VGPath newpath() {
    return vgCreatePath(VG_PATH_FORMAT_STANDARD, VG_PATH_DATATYPE_F, 1.0f, 0.0f, 0, 0, VG_PATH_CAPABILITY_ALL);
}


// makecurve makes path data using specified segments and coordinates
void makecurve(VGubyte *segments, VGfloat *coords) {
    VGPath path = newpath();
    vgAppendPathData( path, 2, segments, coords );
    vgDrawPath(path, VG_FILL_PATH | VG_STROKE_PATH);
    vgDestroyPath(path);
}


// CBezier makes a quadratic bezier curve
void Cbezier( VGfloat sx, VGfloat sy, VGfloat cx, VGfloat cy, VGfloat px, VGfloat py, VGfloat ex, VGfloat ey) {
    VGubyte segments[] = { VG_MOVE_TO_ABS, VG_CUBIC_TO };
    VGfloat coords[]   = { sx, sy, cx, cy, px, py, ex, ey };
    makecurve(segments, coords);
}


// QBezier makes a quadratic bezier curve
void Qbezier(VGfloat sx, VGfloat sy, VGfloat cx, VGfloat cy, VGfloat ex, VGfloat ey) {
    VGubyte segments[] = { VG_MOVE_TO_ABS, VG_QUAD_TO };
    VGfloat coords[]   = { sx, sy, cx, cy, ex, ey };
    makecurve(segments, coords);
}


// interleave interleaves arrays of x, y into a single array
void interleave(VGfloat *x, VGfloat *y, int n, VGfloat *points) {
    while (n--) {
        *points++ = *x++;
        *points++ = *y++;
    }
}


// poly makes either a polygon or polyline
void poly(VGfloat *x, VGfloat *y, VGint n, VGbitfield flag) {
    VGfloat points[n*2];
    VGPath path = newpath();
    interleave(x, y, n, points);
    vguPolygon(path, points, n, VG_FALSE);
    vgDrawPath(path, flag);
    vgDestroyPath(path);
}


// Polygon makes a filled polygon with vertices in x, y arrays
void Polygon(VGfloat *x, VGfloat *y, VGint n) {
    poly(x, y, n, VG_FILL_PATH);
}


// Polyline makes a polyline with vertices at x, y arrays
void Polyline(VGfloat *x, VGfloat *y, VGint n) {
    poly(x, y, n, VG_STROKE_PATH);
}


// Rect makes a rectangle at the specified location and dimensions
void Rect(VGfloat x, VGfloat y, VGfloat w, VGfloat h) {
    VGPath path = newpath();
    vguRect(path, x, y, w, h);
    vgDrawPath(path, VG_FILL_PATH | VG_STROKE_PATH);
    vgDestroyPath(path);
}


// Line makes a line from (x1,y1) to (x2,y2)
void Line(VGfloat x1, VGfloat y1, VGfloat x2, VGfloat y2) {
    VGPath path = newpath();
    vguLine(path, x1, y1, x2, y2);
    vgDrawPath(path, VG_STROKE_PATH);
    vgDestroyPath(path);
}


// Roundrect makes an rounded rectangle at the specified location and dimensions
void Roundrect(VGfloat x, VGfloat y, VGfloat w, VGfloat h, VGfloat rw, VGfloat rh) {
    VGPath path = newpath();
    vguRoundRect(path, x, y, w, h, rw, rh);
    vgDrawPath(path, VG_FILL_PATH | VG_STROKE_PATH);
    vgDestroyPath(path);
}


// Ellipse makes an ellipse at the specified location and dimensions
void Ellipse(VGfloat x, VGfloat y, VGfloat w, VGfloat h) {
    VGPath path = newpath();
    vguEllipse(path, x, y, w, h);
    vgDrawPath(path, VG_FILL_PATH | VG_STROKE_PATH);
    vgDestroyPath(path);
}


// Circle makes a circle at the specified location and dimensions
void Circle(VGfloat x, VGfloat y, VGfloat r) {
    Ellipse(x, y, r, r);
}


// Arc makes an elliptical arc at the specified location and dimensions
void Arc(VGfloat x, VGfloat y, VGfloat w, VGfloat h, VGfloat sa, VGfloat aext) {
    VGPath path = newpath();
    vguArc(path, x, y, w, h, sa, aext, VGU_ARC_OPEN);
    vgDrawPath(path, VG_FILL_PATH | VG_STROKE_PATH);
    vgDestroyPath(path);
}


// Start begins the picture, clearing a rectangular region with a specified color
void Start(int width, int height, float fill[4]) {
    vgSetfv(VG_CLEAR_COLOR, 4, fill);
    vgClear(0, 0, width, height);
    VGfloat black[4] = {0,0,0,1};
    setfill(black);
    setstroke(black);
    strokeWidth(0);
    vgLoadIdentity();
}


// End checks for errors, and renders to the display
void End() {
    assert(vgGetError() == VG_NO_ERROR);
    eglSwapBuffers(state->display, state->surface);
    assert(eglGetError() == EGL_SUCCESS);
}

// SaveEnd dumps the raster before rendering to the display 
void SaveEnd() {
    assert(vgGetError() == VG_NO_ERROR);
    dumpscreen(state->screen_width, state->screen_height);
    eglSwapBuffers(state->display, state->surface);
    assert(eglGetError() == EGL_SUCCESS);
}


// clear the screen to a background color
void Background(int w, int h, VGfloat fill[4]) {
    vgSetfv(VG_CLEAR_COLOR, 4, fill);
    vgClear(0,0,w,h);
}