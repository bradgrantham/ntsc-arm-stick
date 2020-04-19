#ifndef _GRAPHICS_H_
#define _GRAPHICS_H_

int MakePalette(int whichPalette);
int SetPixel(int x, int y, int c);
int SetColorPixel(int x, int y, float r, float g, float b);
void ClearPixmap(int c);
    /* for circles, X will be scaled to aspect ratio */
void DrawFilledCircle(int cx, int cy, int r, int c, int aspX, int aspY);
void DrawLine(int x0, int y0, int x1, int y1, int c);

#endif /* _GRAPHICS_H_ */