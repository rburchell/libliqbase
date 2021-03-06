#if 0
// liqcamera : main camera interface
// slightly cleaned up and duplicated also in liqcameraface
// these should really be instances of the same class :$


// ./v4l2-ctl --set-ctrl=indicator_intensity=1
// this turns on the red light.
// wood grain is visible.
// it is very very blurry
// perhaps an ultra low resolution merge :)

#ifdef __cplusplus
extern "C" {
#endif


// maemo5
#define VIDEO_SRC  "v4l2camsrc"
#define VIDEO_SINK "xvimagesink"


#include <stdlib.h>
#include <string.h>
#include <gst/gst.h>
#include <pthread.h>
#include <sched.h>

#include "liqapp.h"
#include "liqcanvas.h"
#include "liqcamera.h"
#include "liq_xsurface.h"			// include available workhorse functions

static pthread_mutex_t image_push_lock = PTHREAD_MUTEX_INITIALIZER;


GstElement *CAMpipeline=NULL;
int 		CAMW=0;
int 		CAMH=0;
int 		CAMFPS=0;
liqimage *	CAMdestimage=NULL;
void *      CAMtag;
void 		(*CAMUpdateCallback)(void *);

#define mute(pix) (pix)

static int image_push(char *data)
{
	if(!CAMdestimage) return -1;
	// todo: ensure n800 camera flip is handled, need to read the sensor bit and organise accordingly
	// todo: ensure mirroring option is accounted for
	pthread_mutex_lock(&image_push_lock);


		
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
		//	liqapp_sleep(50);
	
	int CAMWd2=CAMW/2;
	int CAMHd2=CAMH/2;
	
	// landscape portrait difference here
	//ROTATEPATCHPOINT
	if(canvas.rotation_angle==0)
	{
		// landscape first
		//(disable this during portrait test)
		do
		{
			// read data for 2 source pixels
			unsigned long p= *UYVY++;

			// Primary Grey channel
			*dy++ = (p & (255<<8 )) >> 8;
			*dy++ = (p & (255<<24)) >> 24;
			if(!(uy & 1))
			{
				// even lines only, 1/2 resolution
				*du++ = mute((p & (255<<16)) >> 16);
				*dv++ = mute((p & (255    )));
			}
			ux+=2;

			if(ux>=CAMW){ ux=0;uy++;   ddy=dy+(CAMW*(uy+1))-1; ddu=du+(CAMWd2*((uy>>1)+1))-1; ddv=dv+(CAMWd2*((uy>>1)+1))-1;        }
			if(uy>=CAMH) break;
		}
		while(--zl);
	}
	else
	{
		// portrait
		do
		{
			
			// read data for 2 source pixels
			unsigned long p= *UYVY++;
			
		//	liqapp_log("cam %3d,%3d, %08lx",ux,uy,p);
			
			// Primary Grey channels to 2 adjacent greys
			dy[ ( (ux  ) ) * CAMdestimage->pitches[0] + ( (CAMH-1)-(uy  ) ) ] = (p & (255<<8 )) >>  8;
			dy[ ( (ux+1) ) * CAMdestimage->pitches[0] + ( (CAMH-1)-(uy  ) ) ] = (p & (255<<24)) >> 24;
			if(!(uy & 1))
			{
				// even lines only, 1/2 resolution
		//		du[ ((CAMHd2-1)-((ux>>1)+1) ) * CAMdestimage->pitches[1] + (uy>>1) ] = mute((p & (255<<16)) >> 16);
		//		dv[ ((CAMHd2-1)-((ux>>1)+1) ) * CAMdestimage->pitches[2] + (uy>>1) ] = mute((p & (255    ))      );
	
	
				du[ ((ux>>1)) * CAMdestimage->pitches[1] + ((CAMHd2-1)-(uy>>1)) ] = mute((p & (255<<16)) >> 16);
				dv[ ((ux>>1)) * CAMdestimage->pitches[2] + ((CAMHd2-1)-(uy>>1)) ] = mute((p & (255    ))      );
			}
			ux+=2;
			// portrait mode, the camera itself was opened with (rows of CAMH) * CAMW instead of the other way round
			if(ux>=CAMH){ ux=0;uy++;     UYVY+=(CAMW-CAMH)>>1;   UYVY=(unsigned long *)&data[ (CAMW * 2) * uy ]; }
			if(uy>=CAMH) break;
		}
		while(--zl);	
	}

	// tell our host that we updated (up to him what he does with the info)
	if(CAMUpdateCallback)
		(*CAMUpdateCallback)(CAMtag);
	pthread_mutex_unlock(&image_push_lock);
	return 0;
}


static gboolean buffer_probe_callback( GstElement *image_sink, GstBuffer *buffer, GstPad *pad, int *stuff)
{
	//liqapp_log("liqcamera: callback!");
	char *data_photo = (char *) GST_BUFFER_DATA(buffer);
	image_push(data_photo);	
	return TRUE;
}



int liqcamera_start(int argCAMW,int argCAMH,int argCAMFPS,liqimage * argCAMdestimage,void (*argCAMUpdateCallback)(void*),void *argCAMtag )
{
	if(CAMpipeline)
	{
		// camera pipeline already in use..
		return -1;
	}
	liqapp_log("liqcamera: starting %i,%i %ifps",argCAMW,argCAMH,argCAMFPS);
	CAMtag=argCAMtag;
	CAMpipeline=NULL;
	CAMW=argCAMW;
	CAMH=argCAMH;
	CAMFPS=argCAMFPS;
	CAMdestimage= liqimage_hold(  argCAMdestimage );
	xsurface_drawclear_yuv(CAMdestimage,0,128,128);
	CAMUpdateCallback=argCAMUpdateCallback;
	liqapp_log("liqcamera: gst_init");
	GstElement *camera_src;
	GstElement *csp_filter;
	GstElement *image_sink;
	GstCaps *	caps;
	gst_init(NULL,NULL);
	liqapp_log("liqcamera: creating pipeline elements");
	CAMpipeline = gst_pipeline_new("liqbase-camera");
	camera_src   = gst_element_factory_make(VIDEO_SRC,          "camera_src");
	csp_filter   = gst_element_factory_make("ffmpegcolorspace", "csp_filter");
	image_sink   = gst_element_factory_make("fakesink",         "image_sink");
	if(!(CAMpipeline && camera_src && csp_filter && image_sink))
	{
		liqapp_warnandcontinue(-1,"liqcamera : Couldn't create pipeline elements");
		return -1;
	}

	
	// ############################################################################ add everything to the CAMpipeline
	liqapp_log("liqcamera: pipeline joining");
	gst_bin_add_many(GST_BIN(CAMpipeline), camera_src, csp_filter,  image_sink, NULL);
	
	// ############################################################################ prepare the camera filter
	liqapp_log("liqcamera: obtaining video caps");
    char capstr[FILENAME_MAX];
    snprintf(capstr,sizeof(capstr),"video/x-raw-yuv,format=(fourcc)UYVY,width=%i,height=%i",CAMW,CAMH);
    caps = gst_caps_from_string(capstr);
	if(!gst_element_link_filtered(camera_src, csp_filter, caps))
	{
		liqapp_warnandcontinue(-1,"liqcamera : Could not link camera_src to csp_filter");
		return -1;
	}
	gst_caps_unref(caps);
	
	
	
	// ############################################################################ prepare the image filter
	liqapp_log("liqcamera: preparing filter");
	
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
	liqapp_log("liqcamera: adding signals");
	g_object_set(     G_OBJECT(image_sink), "signal-handoffs", TRUE,                              NULL);
	g_signal_connect( G_OBJECT(image_sink), "handoff",         G_CALLBACK(buffer_probe_callback), NULL);
	liqapp_log("liqcamera: starting stream");
	gst_element_set_state(CAMpipeline, GST_STATE_PLAYING);
	liqapp_log("liqcamera: done");
	return 0;
}


liqimage * liqcamera_getimage()
{
	// return the current image of this camera, if NULL camera is switched off
	return CAMdestimage;
}

void liqcamera_stop()
{
	liqapp_log("liqcamera_stop first");
	if(!CAMpipeline)
	{
		liqapp_log("liqcamera_stop no pipe");
		// camera pipeline not in use..
		return;
	}
	liqapp_log("liqcamera_stop stopping");
	gst_element_set_state(CAMpipeline,GST_STATE_NULL);
	liqapp_log("liqcamera_stop unrefing");
	gst_object_unref(GST_OBJECT(CAMpipeline));
	liqapp_log("liqcamera_stop clearing vars");
	CAMW=0;
	CAMH=0;
	CAMFPS=0;
	CAMdestimage=NULL;
	CAMUpdateCallback=NULL;
	CAMpipeline=NULL;
	liqapp_log("liqcamera_stop releasing destimage");
	liqimage_release(CAMdestimage);
	// todo: find out if i need an anti-"get_init(..)" call here?...
	liqapp_log("liqcamera_stop done.");
}

#ifdef __cplusplus
}
#endif

#endif
