
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "liqcell.h"
#include "liqcell_easypaint.h"
#include "vgraph.h"

// virtual graphics and drawing
// routines to manage display of a UI


// rotation: http://www.delphi3000.com/articles/article_1466.asp?SK=
// http://www.petesqbsite.com/sections/tutorials/graphics.shtml#5


#include "liqcliprect.h"
#include "liqcell.h"
	
	
	

vgraph *vgraph_new()
{
	// use this to allocate and hold onto a reference
	// should really overload this and allow variations..
	vgraph *self = (vgraph *)malloc(sizeof( vgraph ));
	memset((char *)self,0, sizeof( vgraph ));
	self->usagecount=1;
	return self;
}


vgraph *vgraph_hold(vgraph *self)
{
	// use this to hold onto an object which someone else created
	self->usagecount++;
	return self;
}



void vgraph_free(vgraph *self)
{
	// never call _free directly, outside sources should _release the object
	if(self->font)
	{
		liqfont_release(self->font);
		self->font=NULL;
	}
	vgraph_setwindow(self,NULL);
	vgraph_settarget(self,NULL);
	free(self);
}




void vgraph_release(vgraph *self)
{
	// use this when you are finished with an object
	self->usagecount--;
	if(!self->usagecount) vgraph_free(self);
	
	
	
}



void    vgraph_setcliprect(      vgraph *self, liqcliprect *cliprect )
{
	if(self->cliprect)
	{
		liqcliprect_release(self->cliprect);
	}
	self->cliprect=cliprect;
}

liqcliprect *vgraph_getcliprect(      vgraph *self)
{
	return self->cliprect;
}






void    vgraph_convert_target2window(vgraph *self, int tx,int ty,  int *wx, int *wy)
{
	if(self->target && self->window)
	{
		int x = ((tx - self->scalex) * self->window->w / self->scalew);
		int y = ((ty - self->scaley) * self->window->h / self->scaleh);
		*wx=x;
		*wy=y;
	}
	else
	{
		*wx=0;
		*wy=0;
	}
}

void    vgraph_convert_window2target(vgraph *self, int wx,int wy,  int *tx, int *ty)
{
	if(self->target && self->window)
	{
		int x = self->scalex + (wx * self->scalew / self->window->w);
		int y = self->scaley + (wy * self->scaleh / self->window->h);
		*tx=x;
		*ty=y;
	}
	else
	{
		*tx=0;
		*ty=0;
	}
}






static float calcaspect(int captionw,int captionh,int availw,int availh)
{
	// return a scalar which when applied to the caption(w/h) will result in a rectangle of maximum dimensions to fit inside avail(w/h)
	// int rw = captionw*aspect;
	// int rh = captionh*aspect;    
	if(captionw==0)return 0;
	if(captionh==0)return 0;
	float ax = (float)availw / (float)captionw;
	float ay = (float)availh / (float)captionh;
	float ar = (ax<=ay ? ax : ay);	
	return ar;
	
}


static void vgraph_recalc(vgraph *self)
{
	// recalculate required settings to sit window neatly within target
	if(self->target && self->window)
	{
		float ar = calcaspect(self->window->w,self->window->h,self->target->width,self->target->height);
		
		
		if(self->scaleaspectlock)
		{
		
			self->scalew = self->window->w * ar;
			self->scaleh = self->window->h * ar;
			self->scalex = (self->target->width - self->scalew) / 2;
			self->scaley = (self->target->height - self->scaleh) / 2;
		}
		else
		{
			self->scalew = self->target->width;
			self->scaleh = self->target->height;
			self->scalex = 0;
			self->scaley = 0;
			
		}
		
		
		
		liqapp_log("scale.win %i,%i",self->window->w,self->window->h);
		liqapp_log("scale.tar %i,%i",self->target->width,self->target->height);
		liqapp_log("scale.sxy %i,%i",self->scalex,self->scaley);
		liqapp_log("scale.swh %i,%i",self->scalew,self->scaleh);


		int mx,my;
		int dx,dy;
		
		mx=0;
		my=0;
		vgraph_convert_target2window(self,mx,my,&dx,&dy);
		liqapp_log("scale.test t2w %i,%i = %i,%i",mx,my,dx,dy);
		
		mx=self->target->width;
		my=self->target->height;
		vgraph_convert_target2window(self,mx,my,&dx,&dy);
		liqapp_log("scale.test t2w %i,%i = %i,%i",mx,my,dx,dy);
		


		mx=0;
		my=0;
		vgraph_convert_window2target(self,mx,my,&dx,&dy);
		liqapp_log("scale.test w2t %i,%i = %i,%i",mx,my,dx,dy);
		
		mx=self->window->w;
		my=self->window->h;
		vgraph_convert_window2target(self,mx,my,&dx,&dy);
		liqapp_log("scale.test w2t %i,%i = %i,%i",mx,my,dx,dy);


	}
	else
	{
		
		self->scalex=0;
		self->scaley=0;
		self->scalew=0;
		self->scaleh=0;
		
	}
}



int vgraph_setscaleaspectlock(vgraph *self,int newscaleaspectlock)
{
	self->scaleaspectlock = newscaleaspectlock;
	vgraph_recalc(self);
	return 0;	
}
int vgraph_getscaleaspectlock(vgraph *self)
{
	return self->scaleaspectlock;
}

	
int		vgraph_settarget(vgraph *self, liqimage *target )
{
	if(self->target)
	{
		liqimage_release(self->target);
		self->target=NULL;
	}
	if(target) self->target=liqimage_hold(target);
	vgraph_recalc(self);
	return 0;
}

int		vgraph_setwindow(   vgraph *self, liqcell *window )// int xs,int ys,    int xe,int ye )
{
	if(self->window)
	{
		liqcell_release(self->window);
		self->window=NULL;
	}	
	self->window = liqcell_hold(window);
	//if(window) self->window = (window);
	vgraph_recalc(self);
	return 0;
}










int		vgraph_setbackcolor(   vgraph *self, vcolor backcolor )
{
	self->backcolor=backcolor;
	return 0;
}

int		vgraph_setpencolor(    vgraph *self, vcolor pencolor )
{
	self->pencolor=pencolor;
	return 0;
}

int		vgraph_setpenthick(    vgraph *self, int penthick )
{
	self->penthick=penthick;
	return 0;
}



int		vgraph_setfont(        vgraph *self, liqfont *font)			//  char *fontname, int fontsize, int fontattributes
{
	if(self->font)
	{
		liqfont_release(self->font);
		self->font=NULL;
	}
	
	if(font)  self->font = liqfont_hold(font);
	return 0;
}



int		vgraph_drawclear(      vgraph *self                                  )
{
	unsigned char *yuva = (unsigned char *)&self->backcolor;
	liqcliprect_drawclear( vgraph_getcliprect(self), yuva[0],yuva[1],yuva[2] );
	return 0;
}


int		vgraph_drawpoint(      vgraph *self, int x, int y                    )
{
	
	//liqapp_log("draw.point.in  %i,%i",x,y);
	x = self->scalex + (x * self->scalew / self->window->w);
	y = self->scaley + (y * self->scaleh / self->window->h);
	
	unsigned char *yuva = (unsigned char *)&self->pencolor;
	liqcliprect_drawpsetcolor( vgraph_getcliprect(self),   x,y,       yuva[0],yuva[1],yuva[2] );
	return 0;
}


inline int		vgraph_drawline(       vgraph *self, int x, int y, int ex,int ey       )
{
	//liqapp_log("draw.line.in  %i,%i,%i,%i",x,y,ex,ey);
	x =  self->scalex + (x  * self->scalew / self->window->w);
	y =  self->scaley + (y  * self->scaleh / self->window->h);
	ex = self->scalex + (ex * self->scalew / self->window->w);
	ey = self->scaley + (ey * self->scaleh / self->window->h);

	//liqapp_log("draw.line.use %i,%i,%i,%i",x,y,ex,ey);

	unsigned char *yuva = (unsigned char *)&self->pencolor;
	liqcliprect_drawlinecolor( vgraph_getcliprect(self),   x,y,   ex,ey,    yuva[0],yuva[1],yuva[2] );
	return 0;
}


int		vgraph_drawbox(        vgraph *self, int x, int y, int w,int h       )
{
	
	//liqapp_log("draw.box.in  %i,%i,%i,%i",x,y,w,h);
	
	x = self->scalex + (x * self->scalew / self->window->w);
	y = self->scaley + (y * self->scaleh / self->window->h);
	w = (w * self->scalew / self->window->w);
	h = (h * self->scaleh / self->window->h);

	unsigned char *yuva = (unsigned char *)&self->pencolor;
	liqcliprect_drawboxlinecolor( vgraph_getcliprect(self),   x,y,   w,h,    yuva[0],yuva[1],yuva[2] );

	return 0;
}


int		vgraph_drawrect(       vgraph *self, int x, int y, int w,int h       )
{
	
	//liqapp_log("draw.rect.in  %i,%i,%i,%i",x,y,w,h);
	
	x = self->scalex + (x * self->scalew / self->window->w);
	y = self->scaley + (y * self->scaleh / self->window->h);
	w = (w * self->scalew / self->window->w);
	h = (h * self->scaleh / self->window->h);
	
	
	
	//liqapp_log("draw.rect.use %i,%i,%i,%i",x,y,w,h);
	
	unsigned char *yuva = (unsigned char *)&self->backcolor;
	liqcliprect_drawboxfillcolor( vgraph_getcliprect(self),   x,y,   w,h,    yuva[0],yuva[1],yuva[2] );
	return 0;
}



int		vgraph_drawcircle(     vgraph *self, int x, int y, int radius        )
{




	return vgraph_drawellipse(self,x,y,radius,radius);
}

















int		vgraph_drawellipse(    vgraph *self, int x, int y, int rx,int ry     )
{
	//liqapp_log("draw.ellipse.in  %i,%i,%i,%i",x,y,rx,ry);
	
	x = self->scalex + (x * self->scalew / self->window->w);
	y = self->scaley + (y * self->scaleh / self->window->h);
	rx = (rx * self->scalew / self->window->w);
	ry = (ry * self->scaleh / self->window->h);
	
	
	
	//liqapp_log("draw.ellipse.use %i,%i,%i,%i",x,y,rx,ry);
	
	unsigned char *yuva = (unsigned char *)&self->pencolor;
	
int idx;
for(idx=0;idx<5;idx++)
{
	liqcliprect_drawboxlinecolor( vgraph_getcliprect(self),   x-rx,y-ry,   rx*2,ry*2,    yuva[0],yuva[1],yuva[2] );
	rx/=2;
	ry/=2;
}
	return 0;}


int		vgraph_drawtext(       vgraph *self, int x, int y, char *text        )
{
	if(!self->font)return -1;
	
	return 0;
}

























int		vgraph_drawsketch_old(       vgraph *self, int x, int y, int w,int h  ,liqsketch *sketch     )
{
	if(!sketch) return -1;
/*	
	liqapp_log("draw.sketch.in  %i,%i,%i,%i",x,y,w,h);
	
	x = self->scalex + (x * self->scalew / self->window->w);
	y = self->scaley + (y * self->scaleh / self->window->h);
	w = (w * self->scalew / self->window->w);
	h = (h * self->scaleh / self->window->h);
	
	
	
	liqapp_log("draw.sketch.use %i,%i,%i,%i",x,y,w,h);
 */	
	
	
	unsigned char *yuva = (unsigned char *)&self->backcolor;
	liqcliprect_drawboxfillcolor( vgraph_getcliprect(self),   x,y,   w,h,    yuva[0],yuva[1],yuva[2] );
	//vsketch_rendertograph(sketch,self,x,y,w,h);
	return 0;
}




int		vgraph_drawimage(      vgraph *self, int x, int y, int w,int h , liqimage *image      )
{
	
	liqcliprect_drawboxfillcolor( vgraph_getcliprect(self),   x,y,   w,h,   128,128,128 );
	return 0;
}







int		vgraph_drawsketch(       vgraph *self, int x, int y, int w,int h  ,liqsketch *sketch     )
{
	if(!sketch) return -1;
	
	//liqapp_log("draw.sketch.in  %i,%i,%i,%i",x,y,w,h);
	
	x = self->scalex + (x * self->scalew / self->window->w);
	y = self->scaley + (y * self->scaleh / self->window->h);
	w = (w * self->scalew / self->window->w);
	h = (h * self->scaleh / self->window->h);
	
	
	
	//liqapp_log("draw.sketch.use %i,%i,%i,%i",x,y,w,h);
	
	liqcliprect_drawboxfillcolor( vgraph_getcliprect(self),   x,y,   w,h,   128,128,128 );
	return 0;
}


//#########################################################################
//#########################################################################
//#########################################################################
//#########################################################################
//#########################################################################




int		vgraph_drawcell(      vgraph *self, int x, int y, int w,int h , liqcell *cell      )
{
	//liqapp_log("draw.cell.in  %i,%i,%i,%i   %s",x,y,w,h,cell->name);
	
	x = self->scalex + (x * self->scalew / self->window->w);
	y = self->scaley + (y * self->scaleh / self->window->h);
	w = (w * self->scalew / self->window->w);
	h = (h * self->scaleh / self->window->h);
	
		
	//liqapp_log("draw.cell.use %i,%i,%i,%i",x,y,w,h);
	
	
	
	liqcell_easypaint(cell,vgraph_getcliprect(self),x,y,w,h);
	
	
	return 0;
}

