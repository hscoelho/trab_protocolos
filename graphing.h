#ifndef GRAPHING_H
#define GRAPHING_H

#include <stdio.h>
#include <SDL/SDL.h>
#include <math.h>

#define SCREEN_W 640 // tamanho da janela que sera criada
#define SCREEN_H 640

//#define BPP 8
// typedef Uint8 PixelType;
//#define BPP 16
// typedef Uint16 PixelType;
#define BPP 32
typedef Uint32 PixelType;

typedef struct canvas
{
    SDL_Surface *canvas;
    int Height;  // canvas height
    int Width;   // canvas width
    int Xoffset; // X off set, in canvas pixels
    int Yoffset; // Y off set, in canvas pixels
    int Xext;    // X extra width
    int Yext;    // Y extra height
    double Xmax;
    double Ymax;
    double Xstep; // half a distance between X pixels in 'Xmax' scale

    PixelType *zpixel;

} Tcanvas;

typedef struct dataholder
{
    Tcanvas *canvas;
    double Tcurrent;
    double Lcurrent;
    PixelType Lcolor;
    double INcurrent;
    PixelType INcolor;
    double OUTcurrent;
    PixelType OUTcolor;

} Tdataholder;

void c_pixeldraw(Tcanvas *canvas, int x, int y, PixelType color);

void c_hlinedraw(Tcanvas *canvas, int xstep, int y, PixelType color);

void c_vlinedraw(Tcanvas *canvas, int x, int ystep, PixelType color);

void c_linedraw(Tcanvas *canvas, double x0, double y0, double x1, double y1, PixelType color);

Tcanvas *c_open(int Width, int Height, double Xmax, double Ymax);

Tdataholder *datainit(int Width, int Height, double Xmax, double Ymax, double Lcurrent, double INcurrent, double OUTcurrent);

void setdatacolors(Tdataholder *data, PixelType Lcolor, PixelType INcolor, PixelType OUTcolor);

void datadraw(Tdataholder *data, double time, double level, double inangle, double outangle);

void quitevent();

#endif