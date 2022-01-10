/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) <2003> David Schleef <ds@schleef.org>
 * Copyright (C) <2010> Sebastian Dröge <sebastian.droege@collabora.co.uk>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/*
 * This file was (probably) generated from gstvideobalance.c,
 * gstvideobalance.c,v 1.7 2003/11/08 02:48:59 dschleef Exp 
 */

/**
 * SECTION:element-videobalance
 *
 * Adjusts brightness, contrast, hue, saturation on a video stream.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-1.0 videotestsrc ! videobalance saturation=0.0 ! videoconvert ! ximagesink
 * ]| This pipeline converts the image to black and white by setting the
 * saturation to 0.0.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/math-compat.h>

#include "gstvideobalance.h"
#include <string.h>
#include<math.h>

#include <gst/video/colorbalance.h>

GST_DEBUG_CATEGORY_STATIC (videobalance_debug);
#define GST_CAT_DEFAULT videobalance_debug

/* GstVideoBalance properties */
#define DEFAULT_PROP_CONTRAST		1.0
#define DEFAULT_PROP_BRIGHTNESS		0.0
#define DEFAULT_PROP_HUE		0.0
#define DEFAULT_PROP_SATURATION		1.0
// #define DEFAULT_PROP_ALPHA		1.0
// #define DEFAULT_PROP_BETA		0.0
// #define DEFAULT_PROP_GAMMA		1.0

enum
{
  PROP_0,
  PROP_CONTRAST,
  PROP_BRIGHTNESS,
  PROP_HUE,
  PROP_SATURATION
  // PROP_ALPHA,
  // PROP_BETA,
  // PROP_GAMMA
};

#define PROCESSING_CAPS \
  "{ AYUV, ARGB, BGRA, ABGR, RGBA, Y444, xRGB, RGBx, " \
  "xBGR, BGRx, RGB, BGR, Y42B, YUY2, UYVY, YVYU, " \
  "I420, YV12, IYUV, Y41B, NV12, NV21 }"

static GstStaticPadTemplate gst_video_balance_src_template =
    GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE (PROCESSING_CAPS) ";"
        "video/x-raw(ANY)")
    );

static GstStaticPadTemplate gst_video_balance_sink_template =
    GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE (PROCESSING_CAPS) ";"
        "video/x-raw(ANY)")
    );

static void gst_video_balance_colorbalance_init (GstColorBalanceInterface *
    iface);

static void gst_video_balance_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_video_balance_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

#define gst_video_balance_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstVideoBalance, gst_video_balance,
    GST_TYPE_VIDEO_FILTER,
    G_IMPLEMENT_INTERFACE (GST_TYPE_COLOR_BALANCE,
        gst_video_balance_colorbalance_init));

/*
 * look-up tables (LUT).
 */
static void
gst_video_balance_update_tables (GstVideoBalance * vb)
{
  gint i, j, x;
  gdouble y, u, v, hue_cos, hue_sin, rgb;
  //g_print(">>>> gst_video_balance_update_tables is called!\n");
  //g_print("contrast: %f\n", vb->contrast);
  //g_print("brightness: %f\n", vb->brightness);
  /* Y */
  for (i = 0; i < 256; i++) {
    y = 16 + ((i - 16) * vb->contrast + vb->brightness * 255);
    // y = pow((y/255), 0.54465)*255; //对y通道进行gamma变换
    if (y < 0)
      y = 0;
    else if (y > 255)
      y = 255;
    vb->tabley[i] = rint (y);
  }
  //g_print("%d\n", vb->tabley[45]);

  hue_cos = cos (G_PI * vb->hue);
  hue_sin = sin (G_PI * vb->hue);

  /* U/V lookup tables are 2D, since we need both U/V for each table
   * separately. */
  for (i = -128; i < 128; i++) {
     
    for (j = -128; j < 128; j++) {
     
      u = 128 + ((i * hue_cos + j * hue_sin) * vb->saturation);
      // u = pow(((u-128)/255), 0.54465)*255+128;
      v = 128 + ((-i * hue_sin + j * hue_cos) * vb->saturation);
      // v = pow(((v-128)/255), 0.54465)*255+128;
      if (u < 0)
        u = 0;
      else if (u > 255)
        u = 255;
      if (v < 0)
        v = 0;
      else if (v > 255)
        v = 255;
      vb->tableu[i + 128][j + 128] = rint (u);
      vb->tablev[i + 128][j + 128] = rint (v);
    }
  }

  for (i = 0; i < 256; i++){
    rgb = i*vb->contrast+vb->brightness;
    rgb = pow((rgb/255.0), vb->saturation)*255.0;
    if (rgb < 0)
        rgb = 0;
      else if (rgb > 255)
        rgb = 255;
    vb->tablergb[i] = rint(rgb);
  }
  //   for (i = 0; i < 256; i++){
  //   rgb = i*1.20853-19.3365;
  //   rgb = pow((rgb/255.0), 0.544653)*255.0;
  //   if (rgb < 0)
  //       rgb = 0;
  //     else if (rgb > 255)
  //       rgb = 255;
  //   vb->tablergb[i] = rint(rgb);
  // }

}
static void
gst_video_balance_gamma (GstVideoBalance * vb)
{
  //g_print("***********************");
  gint x, i, j, y, u, v;
  gdouble r, g, b;
  // //g_print("%f", vb->contrast);
  gint y_array[255];
  gint u_array[255][255], v_array[255][255];

    for (x = 0; x<256; x++){
      for (i = 0; i < 256; i++) {
        for (j = 0; j < 256; j++) {
          y = vb->tabley[x];
          u = vb->tableu[i][j];
          v = vb->tablev[i][j];

          if (x==45&&i==0&&j==0){

            //g_print("%d, %d, %d\n", y, u, v);
          }
          if (x==45&&i==131&&j==124){

            //g_print("%d, %d, %d\n", y, u, v);
          }
          r = y + 1.4075*( v - 128);
          g = y - 0.3455*( u - 128) - 0.7169*( v - 128);
          b = y + 1.779*( u - 128);
          if (x==45&&i==131&&j==124)
            //g_print("%f, %f, %f\n", r, g, b);
          r = pow((r/255), 0.54465)*255;
          g = pow((g/255), 0.54465)*255;
          b = pow((b/255), 0.54465)*255;
          if (x==45&&i==131&&j==124)
            //g_print("%f, %f, %f\n", r, g, b);
          y = 0.298*r + 0.612*g + 0.117*b;
          u = -0.168*r - 0.330*g + 0.498*b + 128;
          v =  0.5*r - 0.419*g - 0.081*b + 128;
          if (x==45&&i==131&&j==124)
            //g_print("%d, %d, %d\n", y, u, v);
          if (y < 0)
            y = 0;
          else if (y > 255)
            y = 255;    
          if (u < 0)
            u = 0;
          else if (u > 255)
            u = 255;
          if (v < 0)
            v = 0;
          else if (v > 255)
            v = 255;
          if (x==45&&i==131&&j==124)
            //g_print("%d, %d, %d\n", y, u, v);
          
          y_array[x] = rint(y);
          u_array[i][j] = rint(u);
          v_array[i][j] = rint(v);

        }
      }
  }
  //g_print("%d\n", y_array[45]);

}


static gboolean
gst_video_balance_is_passthrough (GstVideoBalance * videobalance)
{
  //g_print(">>>> gst_video_balance_is_passthrough is called!\n");
  return videobalance->contrast == 1.0 &&
      videobalance->brightness == 0.0 &&
      videobalance->hue == 0.0 && videobalance->saturation == 1.0;
      //  videobalance->alpha == 1.0 && videobalance->beta == 0.0 &&
      //  videobalance->gamma == 1.0;
}

static void
gst_video_balance_update_properties (GstVideoBalance * videobalance)
{
  gboolean passthrough;
  GstBaseTransform *base = GST_BASE_TRANSFORM (videobalance);
  //g_print(">>>> gst_video_balance_update_properties is called!\n");
  GST_OBJECT_LOCK (videobalance);
  passthrough = gst_video_balance_is_passthrough (videobalance);
  if (!passthrough)
    gst_video_balance_update_tables (videobalance);

  GST_OBJECT_UNLOCK (videobalance);

  gst_base_transform_set_passthrough (base, passthrough);
}

static void
gst_video_balance_planar_yuv (GstVideoBalance * videobalance,
    GstVideoFrame * frame)
{
  gint x, y;
  guint8 *ydata;
  guint8 *udata, *vdata;
  gint ystride, ustride, vstride;
  gint width, height;
  gint width2, height2;
  guint8 *tabley = videobalance->tabley;
  guint8 **tableu = videobalance->tableu;
  guint8 **tablev = videobalance->tablev;
  //g_print(">>>> gst_video_balance_planar_yuv is called!\n");
  width = GST_VIDEO_FRAME_WIDTH (frame);
  height = GST_VIDEO_FRAME_HEIGHT (frame);

  ydata = GST_VIDEO_FRAME_PLANE_DATA (frame, 0);
  ystride = GST_VIDEO_FRAME_PLANE_STRIDE (frame, 0);

  for (y = 0; y < height; y++) {
    guint8 *yptr;

    yptr = ydata + y * ystride;
    for (x = 0; x < width; x++) {
      *yptr = tabley[*yptr];
      yptr++;
    }
  }

  width2 = GST_VIDEO_FRAME_COMP_WIDTH (frame, 1);
  height2 = GST_VIDEO_FRAME_COMP_HEIGHT (frame, 1);

  udata = GST_VIDEO_FRAME_PLANE_DATA (frame, 1);
  vdata = GST_VIDEO_FRAME_PLANE_DATA (frame, 2);
  ustride = GST_VIDEO_FRAME_PLANE_STRIDE (frame, 1);
  vstride = GST_VIDEO_FRAME_PLANE_STRIDE (frame, 2);

  for (y = 0; y < height2; y++) {
    guint8 *uptr, *vptr;
    guint8 u1, v1;

    uptr = udata + y * ustride;
    vptr = vdata + y * vstride;

    for (x = 0; x < width2; x++) {
      u1 = *uptr;
      v1 = *vptr;

      *uptr++ = tableu[u1][v1];
      *vptr++ = tablev[u1][v1];
    }
  }
}


// static void
// gst_video_balance_semiplanar_yuv (GstVideoBalance * videobalance,
//     GstVideoFrame * frame)
// {
//   gint x, y;
//   guint8 *ydata;
//   guint8 *uvdata;
//   gint ystride, uvstride;
//   gint width, height;
//   gint width2, height2;
//   guint8 *tabley = videobalance->tabley;
//   guint8 **tableu = videobalance->tableu;
//   guint8 **tablev = videobalance->tablev;
//   gint upos, vpos;

//   width = GST_VIDEO_FRAME_WIDTH (frame);
//   height = GST_VIDEO_FRAME_HEIGHT (frame);

//   ydata = GST_VIDEO_FRAME_PLANE_DATA (frame, 0);
//   ystride = GST_VIDEO_FRAME_PLANE_STRIDE (frame, 0);
//   for (y = 0; y < height; y++) {
//     guint8 *yptr;

//     yptr = ydata + y * ystride;//取了一行的数据
//     for (x = 0; x < width; x++) {
//       *yptr = tabley[*yptr];
//       yptr++;
//     }
//   }

//   width2 = GST_VIDEO_FRAME_COMP_WIDTH (frame, 1);
//   height2 = GST_VIDEO_FRAME_COMP_HEIGHT (frame, 1);

//   uvdata = GST_VIDEO_FRAME_PLANE_DATA (frame, 1);
//   uvstride = GST_VIDEO_FRAME_PLANE_STRIDE (frame, 1);

//   upos = GST_VIDEO_INFO_FORMAT (&frame->info) == GST_VIDEO_FORMAT_NV12 ? 0 : 1;
//   vpos = GST_VIDEO_INFO_FORMAT (&frame->info) == GST_VIDEO_FORMAT_NV12 ? 1 : 0;

//   for (y = 0; y < height2; y++) {
//     guint8 *uvptr;
//     guint8 u1, v1;

//     uvptr = uvdata + y * uvstride;
//     //g_print("####%ld\n", sizeof(uvptr));
//     //g_print("%d\n", uvptr[0]);

//     for (x = 0; x < width2; x++) {
//       u1 = uvptr[upos];
//       v1 = uvptr[vpos];

//       uvptr[upos] = tableu[u1][v1];
//       uvptr[vpos] = tablev[u1][v1];
//       uvptr += 2;
//     }
//   }
// }




static void
gst_video_balance_packed_yuv (GstVideoBalance * videobalance,
    GstVideoFrame * frame)
{
  gint x, y, stride;
  guint8 *ydata, *udata, *vdata;
  gint yoff, uoff, voff;
  gint width, height;
  gint width2, height2;
  guint8 *tabley = videobalance->tabley;
  guint8 **tableu = videobalance->tableu;
  guint8 **tablev = videobalance->tablev;
  //g_print(">>>> gst_video_balance_packed_yuv is called!\n");
  width = GST_VIDEO_FRAME_WIDTH (frame);
  height = GST_VIDEO_FRAME_HEIGHT (frame);

  stride = GST_VIDEO_FRAME_PLANE_STRIDE (frame, 0);
  ydata = GST_VIDEO_FRAME_COMP_DATA (frame, 0);
  yoff = GST_VIDEO_FRAME_COMP_PSTRIDE (frame, 0);

  for (y = 0; y < height; y++) {
    guint8 *yptr;

    yptr = ydata + y * stride;
    for (x = 0; x < width; x++) {
      *yptr = tabley[*yptr];
      yptr += yoff;
    }
  }

  width2 = GST_VIDEO_FRAME_COMP_WIDTH (frame, 1);
  height2 = GST_VIDEO_FRAME_COMP_HEIGHT (frame, 1);

  udata = GST_VIDEO_FRAME_COMP_DATA (frame, 1);
  vdata = GST_VIDEO_FRAME_COMP_DATA (frame, 2);
  uoff = GST_VIDEO_FRAME_COMP_PSTRIDE (frame, 1);
  voff = GST_VIDEO_FRAME_COMP_PSTRIDE (frame, 2);

  for (y = 0; y < height2; y++) {
    guint8 *uptr, *vptr;
    guint8 u1, v1;

    uptr = udata + y * stride;
    vptr = vdata + y * stride;

    for (x = 0; x < width2; x++) {
      u1 = *uptr;
      v1 = *vptr;

      *uptr = tableu[u1][v1];
      *vptr = tablev[u1][v1];

      uptr += uoff;
      vptr += voff;
    }
  }
}

static const int cog_ycbcr_to_rgb_matrix_8bit_sdtv[] = {
  298, 0, 409, -57068,
  298, -100, -208, 34707,
  298, 516, 0, -70870,
};

static const gint cog_rgb_to_ycbcr_matrix_8bit_sdtv[] = {
  66, 129, 25, 4096,
  -38, -74, 112, 32768,
  112, -94, -18, 32768,
};

#define APPLY_MATRIX(m,o,v1,v2,v3) ((m[o*4] * v1 + m[o*4+1] * v2 + m[o*4+2] * v3 + m[o*4+3]) >> 8)

static void
gst_video_balance_packed_rgb (GstVideoBalance * videobalance,
    GstVideoFrame * frame)
{
  gint i, j, height;
  gint width, stride, row_wrap;
  gint pixel_stride;
  guint8 *data;
  gint offsets[3];
  gint r, g, b;
  gint y, u, v;
  gint u_tmp, v_tmp;
  guint8 *tabley = videobalance->tabley;
  guint8 **tableu = videobalance->tableu;
  guint8 **tablev = videobalance->tablev;
  guint8 *tablergb = videobalance->tablergb;
  // //g_print("***%d, %d, %d\n", videobalance->tablergb[44], videobalance->tablergb[36], videobalance->tablergb[30]);
  // //g_print("***%d, %d, %d\n", tablergb[44], tablergb[36], tablergb[30]);
  //g_print(">>>> gst_video_balance_packed_rgb is called!\n");
  width = GST_VIDEO_FRAME_WIDTH (frame);
  height = GST_VIDEO_FRAME_HEIGHT (frame);

  offsets[0] = GST_VIDEO_FRAME_COMP_OFFSET (frame, 0);
  offsets[1] = GST_VIDEO_FRAME_COMP_OFFSET (frame, 1);
  offsets[2] = GST_VIDEO_FRAME_COMP_OFFSET (frame, 2);

  data = GST_VIDEO_FRAME_PLANE_DATA (frame, 0);
  stride = GST_VIDEO_FRAME_PLANE_STRIDE (frame, 0);

  pixel_stride = GST_VIDEO_FRAME_COMP_PSTRIDE (frame, 0);
  row_wrap = stride - pixel_stride * width;

  for (i = 0; i < height; i++) {
    for (j = 0; j < width; j++) {
      r = data[offsets[0]];
      g = data[offsets[1]];
      b = data[offsets[2]];

      // y = APPLY_MATRIX (cog_rgb_to_ycbcr_matrix_8bit_sdtv, 0, r, g, b);
      // u_tmp = APPLY_MATRIX (cog_rgb_to_ycbcr_matrix_8bit_sdtv, 1, r, g, b);
      // v_tmp = APPLY_MATRIX (cog_rgb_to_ycbcr_matrix_8bit_sdtv, 2, r, g, b);

      // y = CLAMP (y, 0, 255);
      // u_tmp = CLAMP (u_tmp, 0, 255);
      // v_tmp = CLAMP (v_tmp, 0, 255);

      // y = tabley[y];
      // u = tableu[u_tmp][v_tmp];
      // v = tablev[u_tmp][v_tmp];

      // r = APPLY_MATRIX (cog_ycbcr_to_rgb_matrix_8bit_sdtv, 0, y, u, v);
      // g = APPLY_MATRIX (cog_ycbcr_to_rgb_matrix_8bit_sdtv, 1, y, u, v);
      // b = APPLY_MATRIX (cog_ycbcr_to_rgb_matrix_8bit_sdtv, 2, y, u, v);
      r = CLAMP (r, 0, 255);
      g = CLAMP (g, 0, 255);
      b = CLAMP (b, 0, 255);
      // //g_print("%d, %d, %d\n", r, g, b);
      // //g_print("+++++%d, %d, %d\n", tablergb[44], tablergb[36], tablergb[30]);

      r = videobalance->tablergb[r];
      g = videobalance->tablergb[g];
      b = videobalance->tablergb[b];

      // //g_print("****%d, %d, %d\n", r, g, b);


      data[offsets[0]] = CLAMP (r, 0, 255);
      data[offsets[1]] = CLAMP (g, 0, 255);
      data[offsets[2]] = CLAMP (b, 0, 255);
      data += pixel_stride;
    }
    data += row_wrap;
  }
}
static void
gst_video_balance_semiplanar_yuv (GstVideoBalance * videobalance,
    GstVideoFrame * frame)
{
  gint i, j,y, x;
  guint8 *ydata;
  guint8 *uvdata;
  gint ystride, uvstride;
  gint width, height;
  gint width2, height2;

  guint8 *tabley = videobalance->tabley;
  guint8 **tableu = videobalance->tableu;
  guint8 **tablev = videobalance->tablev;
  guint8 *tablergb = videobalance->tablergb;
  gint upos, vpos;
  //g_print(">>>> gst_video_balance_semiplanar_yuv is called!\n");
  width = GST_VIDEO_FRAME_WIDTH (frame);
  height = GST_VIDEO_FRAME_HEIGHT (frame);
  
  ydata = GST_VIDEO_FRAME_PLANE_DATA (frame, 0);
  ystride = GST_VIDEO_FRAME_PLANE_STRIDE (frame, 0);
  width2 = GST_VIDEO_FRAME_COMP_WIDTH (frame, 1);
  height2 = GST_VIDEO_FRAME_COMP_HEIGHT (frame, 1);
  
  uvdata = GST_VIDEO_FRAME_PLANE_DATA (frame, 1);
  uvstride = GST_VIDEO_FRAME_PLANE_STRIDE (frame, 1);
  upos = GST_VIDEO_INFO_FORMAT (&frame->info) == GST_VIDEO_FORMAT_NV12 ? 0 : 1;
  vpos = GST_VIDEO_INFO_FORMAT (&frame->info) == GST_VIDEO_FORMAT_NV12 ? 1 : 0;

guint8 uoushu[width];
guint8 voushu[width];
for(i = 0; i <  height ; i++){
    guint8 *yptr;
    yptr = ydata + i * ystride;//取了一行的y
    // //g_print("%d\n", yptr[1]);
    guint8 *uvptr;
    guint8 u1, v1;
    guint16 y, u, v;
    guint16 r, g, b;
    guint8 ujishu, vjishu;
    uvptr = uvdata +(int)(i/2)* uvstride;//取了一行的uv,两行取一次
		for(j = 0; j < width; j ++){

      y = yptr[j];//当前像素y的值
      u = uvptr[upos];
      v = uvptr[vpos];

      // r = APPLY_MATRIX (cog_ycbcr_to_rgb_matrix_8bit_sdtv, 0, y, u, v);
      // g = APPLY_MATRIX (cog_ycbcr_to_rgb_matrix_8bit_sdtv, 1, y, u, v);
      // b = APPLY_MATRIX (cog_ycbcr_to_rgb_matrix_8bit_sdtv, 2, y, u, v);
      r = 1.164*(y - 16) + 1.793*(v - 128);
      g = 1.164*(y - 16) - 0.534*(v - 128) - 0.213*(u - 128);
      b = 1.164*(y - 16) + 2.115*(u - 128);

      r = CLAMP (r, 0, 255);
      g = CLAMP (g, 0, 255);
      b = CLAMP (b, 0, 255);



      r = videobalance->tablergb[r];
      g = videobalance->tablergb[g];
      b = videobalance->tablergb[b];
      r = CLAMP (r, 0, 255);
      g = CLAMP (g, 0, 255);
      b = CLAMP (b, 0, 255);


      y = 0.257f*r + 0.504f*g + 0.098f*b + 16.0f;
      u = -0.148f*r - 0.291f*g + 0.439f*b + 128.0f;
      v = 0.439f*r - 0.368f*g - 0.071f*b + 128.0f;

      // y = APPLY_MATRIX (cog_rgb_to_ycbcr_matrix_8bit_sdtv, 0, r, g, b);
      // u = APPLY_MATRIX (cog_rgb_to_ycbcr_matrix_8bit_sdtv, 1, r, g, b);
      // v = APPLY_MATRIX (cog_rgb_to_ycbcr_matrix_8bit_sdtv, 2, r, g, b);
      y = CLAMP (y, 0, 255);
      u = CLAMP (u, 0, 255);
      v = CLAMP (v, 0, 255);


      yptr[j] = y;//当前像素y的值
      if (i%2==0){
        uoushu[j] = u;
        voushu[j] = v;    
      }

      if (j%2==0&&i%2!=0){
        ujishu = u;
        vjishu = v;      
      }

      if (j%2!=0&&i%2!=0){
        guint8 u_avg, v_avg;
        u_avg = (uoushu[j]+uoushu[j-1]+ujishu+u)*0.25f;
        v_avg = (voushu[j]+voushu[j-1]+vjishu+v)*0.25f;
        uvptr[upos] = u_avg;
        uvptr[vpos] = v_avg;


      }
      

      if (j%2!=0){
        uvptr += 2;
      }
      // yptr++;//循环这一行
      

    }
  }
  
}
/* get notified of caps and plug in the correct process function */
static gboolean
gst_video_balance_set_info (GstVideoFilter * vfilter, GstCaps * incaps,
    GstVideoInfo * in_info, GstCaps * outcaps, GstVideoInfo * out_info)
{
  GstVideoBalance *videobalance = GST_VIDEO_BALANCE (vfilter);
  //g_print(">>>> gst_video_balance_set_info is called!\n");
  GST_DEBUG_OBJECT (videobalance,
      "in %" GST_PTR_FORMAT " out %" GST_PTR_FORMAT, incaps, outcaps);

  videobalance->process = NULL;

  switch (GST_VIDEO_INFO_FORMAT (in_info)) {
    case GST_VIDEO_FORMAT_I420:
    case GST_VIDEO_FORMAT_YV12:
    case GST_VIDEO_FORMAT_Y41B:
    case GST_VIDEO_FORMAT_Y42B:
    case GST_VIDEO_FORMAT_Y444:
      videobalance->process = gst_video_balance_planar_yuv;
      break;
    case GST_VIDEO_FORMAT_YUY2:
    case GST_VIDEO_FORMAT_UYVY:
    case GST_VIDEO_FORMAT_AYUV:
    case GST_VIDEO_FORMAT_YVYU:
      videobalance->process = gst_video_balance_packed_yuv;
      break;
    case GST_VIDEO_FORMAT_NV12:
    case GST_VIDEO_FORMAT_NV21:
      videobalance->process = gst_video_balance_semiplanar_yuv;
      break;
    case GST_VIDEO_FORMAT_ARGB:
    case GST_VIDEO_FORMAT_ABGR:
    case GST_VIDEO_FORMAT_RGBA:
    case GST_VIDEO_FORMAT_BGRA:
    case GST_VIDEO_FORMAT_xRGB:
    case GST_VIDEO_FORMAT_xBGR:
    case GST_VIDEO_FORMAT_RGBx:
    case GST_VIDEO_FORMAT_BGRx:
    case GST_VIDEO_FORMAT_RGB:
    case GST_VIDEO_FORMAT_BGR:
      videobalance->process = gst_video_balance_packed_rgb;
      break;
    default:
      if (!gst_video_balance_is_passthrough (videobalance))
        goto unknown_format;
      break;
  }

  return TRUE;

  /* ERRORS */
unknown_format:
  {
    GST_ERROR_OBJECT (videobalance, "unknown format %" GST_PTR_FORMAT, incaps);
    return FALSE;
  }
}

static void
gst_video_balance_before_transform (GstBaseTransform * base, GstBuffer * buf)
{
  GstVideoBalance *balance = GST_VIDEO_BALANCE (base);
  GstClockTime timestamp, stream_time;
  //g_print(">>>> gst_video_balance_before_transform is called!\n");
  timestamp = GST_BUFFER_TIMESTAMP (buf);
  stream_time =
      gst_segment_to_stream_time (&base->segment, GST_FORMAT_TIME, timestamp);

  GST_DEBUG_OBJECT (balance, "sync to %" GST_TIME_FORMAT,
      GST_TIME_ARGS (timestamp));

  if (GST_CLOCK_TIME_IS_VALID (stream_time))
    gst_object_sync_values (GST_OBJECT (balance), stream_time);
}

static GstCaps *
gst_video_balance_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstVideoBalance *balance = GST_VIDEO_BALANCE (trans);
  GstCaps *ret;
  //g_print(">>>> gst_video_balance_transform_caps is called!\n");
  if (!gst_video_balance_is_passthrough (balance)) {
    static GstStaticCaps raw_caps =
        GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE (PROCESSING_CAPS));

    caps = gst_caps_intersect (caps, gst_static_caps_get (&raw_caps));

    if (filter) {
      ret = gst_caps_intersect_full (filter, caps, GST_CAPS_INTERSECT_FIRST);
      gst_caps_unref (caps);
    } else {
      ret = caps;
    }
  } else {
    if (filter) {
      ret = gst_caps_intersect_full (filter, caps, GST_CAPS_INTERSECT_FIRST);
    } else {
      ret = gst_caps_ref (caps);
    }
  }

  return ret;
}

static GstFlowReturn
gst_video_balance_transform_frame_ip (GstVideoFilter * vfilter,
    GstVideoFrame * frame)
{
  GstVideoBalance *videobalance = GST_VIDEO_BALANCE (vfilter);
  //g_print(">>>> gst_video_balance_transform_frame_ip is called!\n");
  if (!videobalance->process)
    goto not_negotiated;

  GST_OBJECT_LOCK (videobalance);
  videobalance->process (videobalance, frame);
  GST_OBJECT_UNLOCK (videobalance);

  return GST_FLOW_OK;

  /* ERRORS */
not_negotiated:
  {
    GST_ERROR_OBJECT (videobalance, "Not negotiated yet");
    return GST_FLOW_NOT_NEGOTIATED;
  }
}

static void
gst_video_balance_finalize (GObject * object)
{
  GList *channels = NULL;
  //g_print(">>>> gst_video_balance_finalize is called!\n");
  GstVideoBalance *balance = GST_VIDEO_BALANCE (object);

  g_free (balance->tableu[0]);

  channels = balance->channels;
  while (channels) {
    GstColorBalanceChannel *channel = channels->data;

    g_object_unref (channel);
    channels->data = NULL;
    channels = g_list_next (channels);
  }

  if (balance->channels)
    g_list_free (balance->channels);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_video_balance_class_init (GstVideoBalanceClass * klass)
{
  //g_print(">>>> gst_video_balance_class_init is called!\n");
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;
  GstBaseTransformClass *trans_class = (GstBaseTransformClass *) klass;
  GstVideoFilterClass *vfilter_class = (GstVideoFilterClass *) klass;

  GST_DEBUG_CATEGORY_INIT (videobalance_debug, "videobalance", 0,
      "videobalance");

  gobject_class->finalize = gst_video_balance_finalize;
  gobject_class->set_property = gst_video_balance_set_property;
  
  gobject_class->get_property = gst_video_balance_get_property;

  g_object_class_install_property (gobject_class, PROP_CONTRAST,
      g_param_spec_double ("contrast", "Contrast", "contrast", 0.0, 10.0, 
      DEFAULT_PROP_CONTRAST,
          GST_PARAM_CONTROLLABLE | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_BRIGHTNESS,
      g_param_spec_double ("brightness", "Brightness", "brightness", -40.0, 40.0,
          DEFAULT_PROP_BRIGHTNESS,
          GST_PARAM_CONTROLLABLE | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_HUE,
      g_param_spec_double ("hue", "Hue", "hue", -1.0, 1.0, DEFAULT_PROP_HUE,
          GST_PARAM_CONTROLLABLE | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_SATURATION,
      g_param_spec_double ("saturation", "Saturation", "saturation", 0.0, 10.0,
          DEFAULT_PROP_SATURATION,
          GST_PARAM_CONTROLLABLE | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  // g_object_class_install_property (gobject_class, PROP_ALPHA,
  //     g_param_spec_double ("alpha", "Alpha", "alpha", 0.0, 10.0,
  //         DEFAULT_PROP_ALPHA,
  //         GST_PARAM_CONTROLLABLE | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  // g_object_class_install_property (gobject_class, PROP_BETA,
  //     g_param_spec_double ("beta", "Beta", "beta", -30.0, 30.0,
  //         DEFAULT_PROP_BETA,
  //         GST_PARAM_CONTROLLABLE | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  // g_object_class_install_property (gobject_class, PROP_GAMMA,
  //     g_param_spec_double ("gamma", "Gamma", "gamma", 0.0, 2.0,
  //         DEFAULT_PROP_GAMMA,
  //         GST_PARAM_CONTROLLABLE | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  

  gst_element_class_set_static_metadata (gstelement_class, "Video balance",
      "Filter/Effect/Video",
      "Adjusts brightness, contrast, hue, saturation on a video stream",
      "David Schleef <ds@schleef.org>");

  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_video_balance_sink_template);
  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_video_balance_src_template);

  trans_class->before_transform =
      GST_DEBUG_FUNCPTR (gst_video_balance_before_transform);
  trans_class->transform_ip_on_passthrough = FALSE;
  trans_class->transform_caps =
      GST_DEBUG_FUNCPTR (gst_video_balance_transform_caps);

  vfilter_class->set_info = GST_DEBUG_FUNCPTR (gst_video_balance_set_info);
  vfilter_class->transform_frame_ip =
      GST_DEBUG_FUNCPTR (gst_video_balance_transform_frame_ip);
}

static void
gst_video_balance_init (GstVideoBalance * videobalance)
{
  //g_print(">>>> gst_video_balance_init is called!\n");
  const gchar *channels[4] = { "HUE", "SATURATION",
    "BRIGHTNESS", "CONTRAST"
  };
  gint i;

  /* Initialize propertiews */
  videobalance->contrast = DEFAULT_PROP_CONTRAST;
  videobalance->brightness = DEFAULT_PROP_BRIGHTNESS;
  videobalance->hue = DEFAULT_PROP_HUE;
  videobalance->saturation = DEFAULT_PROP_SATURATION;
  // videobalance->alpha = DEFAULT_PROP_ALPHA;
  // videobalance->beta = DEFAULT_PROP_BETA;
  // videobalance->gamma = DEFAULT_PROP_GAMMA;

  videobalance->tableu[0] = g_new (guint8, 256 * 256 * 2);
  for (i = 0; i < 256; i++) {
    videobalance->tableu[i] =
        videobalance->tableu[0] + i * 256 * sizeof (guint8);
    videobalance->tablev[i] =
        videobalance->tableu[0] + 256 * 256 * sizeof (guint8) +
        i * 256 * sizeof (guint8);
  }
  
  gst_video_balance_update_properties (videobalance);
  
  /* Generate the channels list */
  for (i = 0; i < G_N_ELEMENTS (channels); i++) {
    GstColorBalanceChannel *channel;

    channel = g_object_new (GST_TYPE_COLOR_BALANCE_CHANNEL, NULL);
    channel->label = g_strdup (channels[i]);
    channel->min_value = -1000;
    channel->max_value = 1000;

    videobalance->channels = g_list_append (videobalance->channels, channel);
  }
  
}

static const GList *
gst_video_balance_colorbalance_list_channels (GstColorBalance * balance)
{
  GstVideoBalance *videobalance = GST_VIDEO_BALANCE (balance);
  //g_print(">>>> gst_video_balance_colorbalance_list_channels is called!\n");
  g_return_val_if_fail (videobalance != NULL, NULL);
  g_return_val_if_fail (GST_IS_VIDEO_BALANCE (videobalance), NULL);

  return videobalance->channels;
}

static void
gst_video_balance_colorbalance_set_value (GstColorBalance * balance,
    GstColorBalanceChannel * channel, gint value)
{
  GstVideoBalance *vb = GST_VIDEO_BALANCE (balance);
  gdouble new_val;
  gboolean changed = FALSE;
  //g_print(">>>> gst_video_balance_colorbalance_set_value is called!\n");
  g_return_if_fail (vb != NULL);
  g_return_if_fail (GST_IS_VIDEO_BALANCE (vb));
  g_return_if_fail (GST_IS_VIDEO_FILTER (vb));
  g_return_if_fail (channel->label != NULL);

  GST_OBJECT_LOCK (vb);
  if (!g_ascii_strcasecmp (channel->label, "HUE")) {
    new_val = (value + 1000.0) * 2.0 / 2000.0 - 1.0;
    changed = new_val != vb->hue;
    vb->hue = new_val;
  } else if (!g_ascii_strcasecmp (channel->label, "SATURATION")) {
    new_val = (value + 1000.0) * 2.0 / 2000.0;
    changed = new_val != vb->saturation;
    vb->saturation = new_val;
  } else if (!g_ascii_strcasecmp (channel->label, "BRIGHTNESS")) {
    new_val = (value + 1000.0) * 2.0 / 2000.0 - 1.0;
    changed = new_val != vb->brightness;
    vb->brightness = new_val;
  } else if (!g_ascii_strcasecmp (channel->label, "CONTRAST")) {
    new_val = (value + 1000.0) * 2.0 / 2000.0;
    changed = new_val != vb->contrast;
    vb->contrast = new_val;
  }
  GST_OBJECT_UNLOCK (vb);

  if (changed)
    gst_video_balance_update_properties (vb);

  if (changed) {
    gst_color_balance_value_changed (balance, channel,
        gst_color_balance_get_value (balance, channel));
  }
  
}

static gint
gst_video_balance_colorbalance_get_value (GstColorBalance * balance,
    GstColorBalanceChannel * channel)
{
  GstVideoBalance *vb = GST_VIDEO_BALANCE (balance);
  gint value = 0;
  //g_print(">>>> gst_video_balance_colorbalance_get_value is called!\n");
  g_return_val_if_fail (vb != NULL, 0);
  g_return_val_if_fail (GST_IS_VIDEO_BALANCE (vb), 0);
  g_return_val_if_fail (channel->label != NULL, 0);

  if (!g_ascii_strcasecmp (channel->label, "HUE")) {
    value = (vb->hue + 1) * 2000.0 / 2.0 - 1000.0;
  } else if (!g_ascii_strcasecmp (channel->label, "SATURATION")) {
    value = vb->saturation * 2000.0 / 2.0 - 1000.0;
  } else if (!g_ascii_strcasecmp (channel->label, "BRIGHTNESS")) {
    value = (vb->brightness + 1) * 2000.0 / 2.0 - 1000.0;
  } else if (!g_ascii_strcasecmp (channel->label, "CONTRAST")) {
    value = vb->contrast * 2000.0 / 2.0 - 1000.0;
  }
  // else if (!g_ascii_strcasecmp (channel->label, "ALPHA")) {
  //   value = vb->alpha;
  // }
  // else if (!g_ascii_strcasecmp (channel->label, "BETA")) {
  //   value = vb->beta;
  // }
  // else if (!g_ascii_strcasecmp (channel->label, "GAMMA")) {
  //   value = vb->gamma;
  // }

  return value;
}

static GstColorBalanceType
gst_video_balance_colorbalance_get_balance_type (GstColorBalance * balance)
{
  //g_print(">>>> gst_video_balance_colorbalance_get_balance_type is called!\n");
  return GST_COLOR_BALANCE_SOFTWARE;
}

static void
gst_video_balance_colorbalance_init (GstColorBalanceInterface * iface)
{
  //g_print(">>>> gst_video_balance_colorbalance_init is called!\n");
  iface->list_channels = gst_video_balance_colorbalance_list_channels;
  iface->set_value = gst_video_balance_colorbalance_set_value;
  iface->get_value = gst_video_balance_colorbalance_get_value;
  iface->get_balance_type = gst_video_balance_colorbalance_get_balance_type;
}

static GstColorBalanceChannel *
gst_video_balance_find_channel (GstVideoBalance * balance, const gchar * label)
{
  GList *l;
  //g_print(">>>> gst_video_balance_find_channel is called!\n");
  for (l = balance->channels; l; l = l->next) {
    GstColorBalanceChannel *channel = l->data;

    if (g_ascii_strcasecmp (channel->label, label) == 0)
      return channel;
  }
  return NULL;
}

static void
gst_video_balance_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstVideoBalance *balance = GST_VIDEO_BALANCE (object);
  gdouble d;
  const gchar *label = NULL;
  //g_print(">>>> gst_video_balance_set_property is called!\n");
  GST_OBJECT_LOCK (balance);
  gint tag =0;
  switch (prop_id) {
    case PROP_CONTRAST:
      
      d = g_value_get_double (value);
      GST_DEBUG_OBJECT (balance, "Changing contrast from %lf to %lf",
          balance->contrast, d);
      //g_print(">>>> Changing contrast from %lf to %lf\n",
          // balance->contrast, d);
      if (d != balance->contrast)
        label = "CONTRAST";
      balance->contrast = d;
      break;
    case PROP_BRIGHTNESS:     
      d = g_value_get_double (value);
      GST_DEBUG_OBJECT (balance, "Changing brightness from %lf to %lf",
          balance->brightness, d);
      //g_print(">>>> Changing brightness from %lf to %lf\n",
          // balance->brightness, d);
      if (d != balance->brightness)
        label = "BRIGHTNESS";
      balance->brightness = d;
      break;
    case PROP_HUE:
      //g_print(">>>> 3\n");
      d = g_value_get_double (value);
      GST_DEBUG_OBJECT (balance, "Changing hue from %lf to %lf", balance->hue,
          d);
      //g_print(">>>> Changing hue from %lf to %lf\n",
          // balance->hue, d);
      if (d != balance->hue)
        label = "HUE";
      balance->hue = d;
      break;
    case PROP_SATURATION:
      //g_print(">>>> 4\n");
      tag = 1;
      d = g_value_get_double (value);
      GST_DEBUG_OBJECT (balance, "Changing saturation from %lf to %lf",
          balance->saturation, d);
      //g_print(">>>> Changing saturantion from %lf to %lf\n",
          // balance->saturation, d);
      if (d != balance->saturation)
        label = "SATURATION";
      balance->saturation = d;
      break;
    //   case PROP_ALPHA:

    //   tag = 1;
    //   d = g_value_get_double (value);
    //   GST_DEBUG_OBJECT (balance, "Changing alpha from %lf to %lf",
    //       balance->alpha, d);
  
    //   if (d != balance->alpha)
    //     label = "ALPHA";
    //   balance->alpha = d;
    //   break;
    //   case PROP_BETA:
    //   tag = 1;
    //   d = g_value_get_double (value);
    //   GST_DEBUG_OBJECT (balance, "Changing beta from %lf to %lf",
    //       balance->beta, d);

    //   if (d != balance->beta)
    //     label = "BETA";
    //   balance->beta = d;
    //   break;
    //   case PROP_GAMMA:
    //   tag = 1;
    //   d = g_value_get_double (value);
    //   GST_DEBUG_OBJECT (balance, "Changing gamma from %lf to %lf",
    //       balance->gamma, d);

    //   if (d != balance->gamma)
    //     label = "GAMMA";
    //   balance->gamma = d;
    //   break;
    // default:
    //   G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    //   break;
  }

  GST_OBJECT_UNLOCK (balance);
  gst_video_balance_update_properties (balance);
  // if (tag==1){
  //   // GstVideoBalance *vb = GST_VIDEO_BALANCE (balance);
  //   gst_video_balance_gamma(balance);
  // }
  

  if (label) {
    GstColorBalanceChannel *channel =
        gst_video_balance_find_channel (balance, label);
    gst_color_balance_value_changed (GST_COLOR_BALANCE (balance), channel,
        gst_color_balance_get_value (GST_COLOR_BALANCE (balance), channel));
  }
}

static void
gst_video_balance_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstVideoBalance *balance = GST_VIDEO_BALANCE (object);
  //g_print(">>>> gst_video_balance_get_property is called!\n");
  switch (prop_id) {
    case PROP_CONTRAST:
      g_value_set_double (value, balance->contrast);
      break;
    case PROP_BRIGHTNESS:
      g_value_set_double (value, balance->brightness);
      break;
    case PROP_HUE:
      g_value_set_double (value, balance->hue);
      break;
    case PROP_SATURATION:
      g_value_set_double (value, balance->saturation);
      break;
    // case PROP_ALPHA:
    //   g_value_set_double (value, balance->alpha);
    //   break;
    // case PROP_BETA:
    //   g_value_set_double (value, balance->beta);
    //   break;
    // case PROP_GAMMA:
    //   g_value_set_double (value, balance->gamma);
    //   break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}
