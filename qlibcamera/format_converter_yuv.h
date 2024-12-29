#pragma once
#include <QtGlobal>

void rgb24_to_yuv420(quint8* rgb24, quint8* y_plane, quint8* u_plane, quint8* v_plane, int width, int height);
void bgr24_to_yuv420(quint8* rgb24, quint8* y_plane, quint8* u_plane, quint8* v_plane, int width, int height);
void rgb565_to_yuv420(quint16* rgb565, quint8* y_plane, quint8* u_plane, quint8* v_plane, int width, int height);
