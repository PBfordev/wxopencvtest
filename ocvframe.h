///////////////////////////////////////////////////////////////////////////////
// Name:        ocvframe.h
// Purpose:     Gets and displays images coming from various OpenCV sources
// Author:      PB
// Created:     2020-09-16
// Copyright:   (c) 2020 PB
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////


#ifndef OCVFRAME_H
#define OCVFRAME_H

#include <wx/wx.h>

// forward declarations
class WXDLLIMPEXP_FWD_CORE wxSlider;
class wxBitmapFromOpenCVPanel;

namespace cv
{
    class Mat;
    class VideoCapture;
}

class CameraThread;


// This class can open an OpenCV source of images (image file, video file,
// default WebCam, and IP camera) and display the images using wxBitmapFromOpenCVPanel.
class OpenCVFrame : public wxFrame
{
public:
    OpenCVFrame();
    ~OpenCVFrame();
private:
    enum Mode
    {
        Empty,
        Image,
        Video,
        WebCam,
        IPCamera,
    };

    Mode                     m_mode{Empty};
    wxString                 m_sourceName;
    int                      m_currentVideoFrameNumber{0};

    cv::VideoCapture*        m_videoCapture{nullptr};
    CameraThread*            m_cameraThread{nullptr};

    wxBitmapFromOpenCVPanel* m_bitmapPanel;
    wxSlider*                m_videoSlider;
    wxButton*                m_propertiesButton;

    static wxBitmap ConvertMatToBitmap(const cv::Mat& matBitmap, long& timeConvert);

    void Clear();
    void UpdateFrameTitle();

    void ShowVideoFrame(int frameNumber);

    // If address is empty, the default webcam is used.
    // resolution and useMJPEG are used only for webcam.
    bool StartCameraCapture(const wxString& address,
                            const wxSize& resolution = wxSize(),
                            bool useMJPEG = false);
    bool StartCameraThread();
    void DeleteCameraThread();

    void OnImage(wxCommandEvent&);
    void OnVideo(wxCommandEvent&);
    void OnWebCam(wxCommandEvent&);
    void OnIPCamera(wxCommandEvent&);
    void OnClear(wxCommandEvent&);

    void OnProperties(wxCommandEvent&);

    void OnVideoSetFrame(wxCommandEvent& evt);

    void OnCameraFrame(wxThreadEvent& evt);
    void OnCameraEmpty(wxThreadEvent&);
    void OnCameraException(wxThreadEvent& evt);
};

#endif // #ifndef OCVFRAME_H