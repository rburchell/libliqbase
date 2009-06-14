#include <stdlib.h>
#include <string.h>


#include <gst/gst.h>


#ifdef __arm__
#define VIDEO_SRC "v4l2src"
#define VIDEO_SINK "xvimagesink"
#else
#define VIDEO_SRC "v4lsrc"
#define VIDEO_SINK "ximagesink"
#endif

/*
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvlib.h>

*/

#include "liqapp.h"
#include "liqcamera.h"


GstElement *CAMpipeline=NULL;
int 		CAMW=0;
int 		CAMH=0;
int 		CAMFPS=0;
liqimage *	CAMdestimage=NULL;
void *      CAMtag;
void 		(*CAMUpdateCallback)(void *);

static inline char mute(char pix)
{
	int v=128+(   (((int)pix)-128)>>1   );
	//int v=pix;// 128+(   (((int)pix)-128)>>1   );
	return (char)v;
}

static int image_push(char *data)
{
	if(!CAMdestimage) return -1;
	
	// todo: ensure n800 camera flip is handled, need to read the sensor bit and organise accordingly
	// todo: ensure mirroring option is accounted for
	
// ok a 32bit long contains  UYVY
unsigned long * UYVY = (unsigned long *)data;
unsigned char *dy= &CAMdestimage->data[ CAMdestimage->offsets[0] ];
unsigned char *du= &CAMdestimage->data[ CAMdestimage->offsets[1] ];
unsigned char *dv= &CAMdestimage->data[ CAMdestimage->offsets[2] ];
int ux=0;
int uy=0;
int zl = (CAMW*CAMH)/2;

unsigned char *ddy= dy+(CAMW  )-1;
unsigned char *ddu= du+(CAMW/2)-1;
unsigned char *ddv= dv+(CAMW/2)-1;

int CAMWd2=CAMW/2;
	do
	{
		if(!CAMdestimage) return -1;
		
		
		//if(ux==0){ app_log("line %i",uy); }
		
		unsigned long p= *UYVY++;
		// Primary Grey channel
/*		*dy++ = (p & (255<<8 )) >> 8;
		*dy++ = (p & (255<<24)) >> 24;
		
		if(!(uy & 1))
		{
			// even lines only, 1/2 resolution
			*du++ = mute((p & (255<<16)) >> 16);
			*dv++ = mute((p & (255    )));
		}
*/		
		
		*ddy-- = (p & (255<<8 )) >> 8;
		*ddy-- = (p & (255<<24)) >> 24;
		
		if(!(uy & 1))
		{
			// even lines only, 1/2 resolution
			*ddu-- = mute((p & (255<<16)) >> 16);
			*ddv-- = mute((p & (255    )));
		}
		
		ux+=2;
		if(ux>=CAMW){ ux=0;uy++;   ddy=dy+(CAMW*(uy+1))-1; ddu=du+(CAMWd2*((uy>>1)+1))-1; ddv=dv+(CAMWd2*((uy>>1)+1))-1;        }
		if(uy>=CAMH) break;
	}
	while(--zl);
	
	// tell our host that we updated (up to him what he does with the info)
	if(CAMUpdateCallback)
		(*CAMUpdateCallback)(CAMtag);
	return 0;
}




static gboolean buffer_probe_callback( GstElement *image_sink, GstBuffer *buffer, GstPad *pad, int *stuff)
{
	char *data_photo = (char *) GST_BUFFER_DATA(buffer);
	image_push(data_photo);	
	return TRUE;
}



int liqcamera_start(int argCAMW,int argCAMH,int argCAMFPS,liqimage * argCAMdestimage,void (*argCAMUpdateCallback)(),void *argCAMtag )
{
	if(CAMpipeline)
	{
		// camera pipeline already in use..
		return -1;
	}
	
	CAMtag=argCAMtag;
	
	CAMpipeline=NULL;
	CAMW=argCAMW;
	CAMH=argCAMH;
	CAMFPS=argCAMFPS;
	CAMdestimage= liqimage_hold(  argCAMdestimage );
	CAMUpdateCallback=argCAMUpdateCallback;
	
	GstElement *camera_src;
	GstElement *csp_filter;
	GstElement *image_sink;
	GstCaps *	caps;
	//gst_init(argc, argv);
	gst_init(NULL,NULL);
	CAMpipeline = gst_pipeline_new("liqbase-camera");
	camera_src   = gst_element_factory_make(VIDEO_SRC,          "camera_src");
	csp_filter   = gst_element_factory_make("ffmpegcolorspace", "csp_filter");
	image_sink   = gst_element_factory_make("fakesink",         "image_sink");
	if(!(CAMpipeline && camera_src && csp_filter && image_sink))
	{
		g_critical("liqcamera : Couldn't create pipeline elements");
		return -1;
	}
	// ############################################################################ add everything to the CAMpipeline
	gst_bin_add_many(GST_BIN(CAMpipeline), camera_src, csp_filter,  image_sink, NULL);
	// ############################################################################ prepare the camera filter
	caps = gst_caps_new_simple("video/x-raw-yuv",
            "format",    GST_TYPE_FOURCC, GST_MAKE_FOURCC ('U', 'Y', 'V', 'Y'),
			"width",     G_TYPE_INT, CAMW,
			"height",    G_TYPE_INT, CAMH,
			"framerate", GST_TYPE_FRACTION, CAMFPS, 1,
			NULL);
		if(!gst_element_link_filtered(camera_src, csp_filter, caps))
		{
			liqapp_warnandcontinue(-1,"liqcamera : Could not link camera_src to csp_filter");
			return -1;
		}
	gst_caps_unref(caps);
	// ############################################################################ prepare the image filter
	caps = gst_caps_new_simple("video/x-raw-yuv",
			"width",  G_TYPE_INT, CAMW,
			"height", G_TYPE_INT, CAMH,
			NULL);
		if(!gst_element_link_filtered(csp_filter, image_sink, caps))
		{
			liqapp_warnandcontinue(-1,"liqcamera : Could not link csp_filter to image_sink");
			return -1;
		}
	gst_caps_unref(   caps);
	// ############################################################################ finally make sure we hear about the events
	g_object_set(     G_OBJECT(image_sink), "signal-handoffs", TRUE,                              NULL);
	g_signal_connect( G_OBJECT(image_sink), "handoff",         G_CALLBACK(buffer_probe_callback), NULL);
	
	gst_element_set_state(CAMpipeline, GST_STATE_PLAYING);
	return 0;
}
liqimage * liqcamera_getimage()
{
	// return the current image of this camera, if NULL camera is switched off
	return CAMdestimage;
}

void liqcamera_stop()
{
	if(CAMpipeline)
	{
		// camera pipeline not in use..
		return;
	}	
	liqimage_release(CAMdestimage);
	CAMW=0;
	CAMH=0;
	CAMFPS=0;
	CAMdestimage=NULL;
	CAMUpdateCallback=NULL;
	gst_element_set_state(CAMpipeline,GST_STATE_NULL);
	gst_object_unref(GST_OBJECT(CAMpipeline));
	CAMpipeline=NULL;
	// todo: find out if i need an anti-"get_init(..)" call here?...
}


