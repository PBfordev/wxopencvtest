///////////////////////////////////////////////////////////////////////////////
// Name:        bmpfromocvpanel.h
// Purpose:     Displays a wxBitmap originated from OpenCV
// Author:      PB
// Created:     2020-09-16
// Copyright:   (c) 2020 PB
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef BMPFROMOCVPANEL_H
#define BMPFROMOCVPANEL_H

#include <wx/wx.h>
#include <wx/scrolwin.h>


// This class displays a wxBitmap originated from OpenCV
// and also the time it took to obtain, convert, and display the bitmap.
//
// Color or font of the overlay text can be changed by left or right
// doubleclick on the panel.

class wxBitmapFromOpenCVPanel : public wxScrolledCanvas
{
public:
    wxBitmapFromOpenCVPanel(wxWindow* parent);

    bool SetBitmap(const wxBitmap& bitmap, const long timeGet, const long timeConvert);

    const wxBitmap& GetBitmap() { return m_bitmap; }

private:
    wxBitmap m_bitmap;
    wxColour m_overlayTextColour;
    wxFont   m_overlayFont;
    long     m_timeGetCVBitmap{0};   // time to obtain bitmap from OpenCV in ms
    long     m_timeConvertBitmap{0}; // time to convert Mat to wxBitmap in ms

    wxSize DoGetBestClientSize() const override;

    void OnPaint(wxPaintEvent&);

    void OnChangeOverlayTextColour(wxMouseEvent&);
    void OnChangeOverlayFont(wxMouseEvent&);
};

#endif // #ifndef BMPFROMOCVPANEL_H