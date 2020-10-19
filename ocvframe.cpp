///////////////////////////////////////////////////////////////////////////////
// Name:        ocvframe.cpp
// Purpose:     Gets and displays images coming from various OpenCV sources
// Author:      PB
// Created:     2020-09-16
// Copyright:   (c) 2020 PB
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#include <wx/wx.h>
#include <wx/choicdlg.h>
#include <wx/filedlg.h>
#include <wx/listctrl.h>
#include <wx/slider.h>
#include <wx/textdlg.h>
#include <wx/thread.h>
#include <wx/utils.h>

#include <opencv2/opencv.hpp>

#include "convertmattowxbmp.h"
#include "bmpfromocvpanel.h"
#include "ocvframe.h"

// A frame was retrieved from WebCam or IP Camera.
wxDEFINE_EVENT(wxEVT_CAMERA_FRAME, wxThreadEvent);
// Could not retrieve a frame, consider connection to the camera lost.
wxDEFINE_EVENT(wxEVT_CAMERA_EMPTY, wxThreadEvent);

//
// Worker thread for retrieving images from WebCam or IP Camera
// and sending them to the main thread for display.
class CameraThread : public wxThread
{
public:
    struct CameraFrame
    {
        cv::Mat matBitmap;
        long    timeGet{0};
    };

    CameraThread(wxEvtHandler* eventSink, cv::VideoCapture* camera);

protected:
    wxEvtHandler*     m_eventSink{nullptr};
    cv::VideoCapture* m_camera{nullptr};

    ExitCode Entry() override;
};

CameraThread::CameraThread(wxEvtHandler* eventSink, cv::VideoCapture* camera)
    : wxThread(wxTHREAD_JOINABLE),
      m_eventSink(eventSink), m_camera(camera)
{
    wxASSERT(m_eventSink);
    wxASSERT(m_camera);
}

wxThread::ExitCode CameraThread::Entry()
{
    wxStopWatch stopWatch;

    while ( !TestDestroy() )
    {
        try
        {
            CameraFrame*  frame = new CameraFrame;

            stopWatch.Start();
            (*m_camera) >> frame->matBitmap;
            frame->timeGet = stopWatch.Time();

            if ( !frame->matBitmap.empty() )
            {
                wxThreadEvent* evt = new wxThreadEvent(wxEVT_CAMERA_FRAME);

                evt->SetPayload(frame);
                m_eventSink->QueueEvent(evt);
            }
            else // connection to camera lost
            {
                wxThreadEvent* evt = new wxThreadEvent(wxEVT_CAMERA_EMPTY);

                delete frame;
                m_eventSink->QueueEvent(evt);
                break;
            }
        }
        catch ( cv::Exception& e )
        {
            wxLogError("OpenCV exception: %s", e.msg);
        }
    }
    return static_cast<wxThread::ExitCode>(nullptr);
}

//
// OpenCVFrame
//
OpenCVFrame::OpenCVFrame()
    : wxFrame(nullptr, wxID_ANY, "")
{
    wxPanel*    mainPanel = new wxPanel(this);
    wxBoxSizer* mainPanelSizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* buttonSizer = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* bottomSizer = new wxBoxSizer(wxHORIZONTAL); // wxSlider and info button
    wxButton*   button = nullptr;

    button = new wxButton(mainPanel, wxID_ANY, "&Image...");
    button->Bind(wxEVT_BUTTON, &OpenCVFrame::OnImage, this);
    buttonSizer->Add(button, wxSizerFlags().Proportion(1).Expand().Border());

    button = new wxButton(mainPanel, wxID_ANY, "&Video...");
    button->Bind(wxEVT_BUTTON, &OpenCVFrame::OnVideo, this);
    buttonSizer->Add(button, wxSizerFlags().Proportion(1).Expand().Border());

    button = new wxButton(mainPanel, wxID_ANY, "&WebCam...");
    button->Bind(wxEVT_BUTTON, &OpenCVFrame::OnWebCam, this);
    buttonSizer->Add(button, wxSizerFlags().Proportion(1).Expand().Border());

    button = new wxButton(mainPanel, wxID_ANY, "I&P Camera...");
    button->Bind(wxEVT_BUTTON, &OpenCVFrame::OnIPCamera, this);
    buttonSizer->Add(button, wxSizerFlags().Proportion(1).Expand().Border());

    buttonSizer->AddSpacer(FromDIP(20));

    button = new wxButton(mainPanel, wxID_ANY, "&Clear");
    button->Bind(wxEVT_BUTTON, &OpenCVFrame::OnClear, this);
    buttonSizer->Add(button, wxSizerFlags().Proportion(1).Expand().Border());

    m_bitmapPanel = new wxBitmapFromOpenCVPanel(mainPanel);

    m_propertiesButton = new wxButton(mainPanel, wxID_ANY, "P&roperties...");
    m_propertiesButton->Bind(wxEVT_BUTTON, &OpenCVFrame::OnProperties, this);
    bottomSizer->Add(m_propertiesButton, wxSizerFlags().Expand().Border());

    m_videoSlider = new wxSlider(mainPanel, wxID_ANY, 0, 0, 100, wxDefaultPosition, wxDefaultSize, wxSL_LABELS);
    m_videoSlider->Bind(wxEVT_SLIDER, &OpenCVFrame::OnVideoSetFrame, this);
    bottomSizer->Add(m_videoSlider, wxSizerFlags().Proportion(1).Expand().Border().ReserveSpaceEvenIfHidden());

    mainPanelSizer->Add(buttonSizer, wxSizerFlags().Expand().Border());
    mainPanelSizer->Add(m_bitmapPanel, wxSizerFlags().Proportion(1).Expand());
    mainPanelSizer->Add(bottomSizer, wxSizerFlags().Expand().Border());

    SetMinClientSize(FromDIP(wxSize(600, 400)));
    SetSize(FromDIP(wxSize(800, 600)));

    mainPanel->SetSizerAndFit(mainPanelSizer);

    Clear();

    Bind(wxEVT_CAMERA_FRAME, &OpenCVFrame::OnCameraFrame, this);
    Bind(wxEVT_CAMERA_EMPTY, &OpenCVFrame::OnCameraEmpty, this);
}

OpenCVFrame::~OpenCVFrame()
{
    DeleteCameraThread();
}

wxBitmap OpenCVFrame::ConvertMatToBitmap(const cv::Mat& matBitmap, long& timeConvert)
{
    wxCHECK(!matBitmap.empty(), wxBitmap());

    wxBitmap    bitmap(matBitmap.cols, matBitmap.rows, 24);
    bool        converted = false;
    wxStopWatch stopWatch;
    long        time = 0;

    stopWatch.Start();
    converted = ConvertMatBitmapTowxBitmap(matBitmap, bitmap);
    time = stopWatch.Time();

    if ( !converted )
        wxLogError("Could not convert Mat to wxBitmap.");
    else
        timeConvert = time;

    return bitmap;
}

void OpenCVFrame::Clear()
{
    DeleteCameraThread();

    if ( m_videoCapture )
    {
        delete m_videoCapture;
        m_videoCapture = nullptr;
    }

    m_mode = Empty;
    m_sourceName.clear();
    m_currentVideoFrameNumber = 0;

    m_bitmapPanel->SetBitmap(wxBitmap(), 0, 0);
    m_videoSlider->SetValue(0);
    m_videoSlider->SetRange(0, 1);
    m_videoSlider->Disable();
    m_videoSlider->Hide();

    m_propertiesButton->Disable();

    UpdateFrameTitle();
}

void OpenCVFrame::UpdateFrameTitle()
{
    wxString modeStr;

    switch ( m_mode )
    {
        case Empty:
            modeStr = "Empty";
            break;
        case Image:
            modeStr = "Image";
            break;
        case Video:
            modeStr = "Video";
            break;
        case WebCam:
            modeStr = "WebCam";
            break;
        case IPCamera:
            modeStr = "IP Camera";
            break;
    }

    SetTitle(wxString::Format("wxOpenCVTest: %s", modeStr));
}

void OpenCVFrame::ShowVideoFrame(int frameNumber)
{
    int         captureFrameNumber = (int)(m_videoCapture->get(cv::CAP_PROP_POS_FRAMES));
    cv::Mat     matBitmap;
    wxStopWatch stopWatch;
    long        timeGet = 0;

    stopWatch.Start();
    if ( frameNumber != captureFrameNumber)
        m_videoCapture->set(cv::VideoCaptureProperties::CAP_PROP_POS_FRAMES, frameNumber);
    (*m_videoCapture) >> matBitmap;
    timeGet = stopWatch.Time();

    if ( matBitmap.empty() )
    {
        m_bitmapPanel->SetBitmap(wxBitmap(), 0, 0);
        wxLogError("Could not retrieve frame %d.", frameNumber);
        return;
    }

    wxBitmap bitmap;
    long     timeConvert = 0;

    bitmap = ConvertMatToBitmap(matBitmap, timeConvert);

    if ( !bitmap.IsOk() )
    {
        m_bitmapPanel->SetBitmap(wxBitmap(), 0, 0);
        wxLogError("Could not convert frame %d to wxBitmap.", frameNumber);
        return;
    }

    m_bitmapPanel->SetBitmap(bitmap, timeGet, timeConvert);
}

bool OpenCVFrame::StartCameraCapture(const wxString& address, const wxSize& resolution,
                                     bool useMJPEG)
{
    const bool        isDefaultWebCam = address.empty();
    cv::VideoCapture* cap = nullptr;

    Clear();

    {
        wxWindowDisabler disabler(this);
        wxBusyCursor     busyCursor;

        if ( isDefaultWebCam )
            cap = new cv::VideoCapture(0);
        else
            cap = new cv::VideoCapture(address.ToStdString());
    }

    if ( !cap->isOpened() )
    {
        delete cap;
        wxLogError("Could not connect to the camera.");
        return false;
    }

    m_videoCapture = cap;

    if ( isDefaultWebCam )
    {
        m_videoCapture->set(cv::CAP_PROP_FRAME_WIDTH, resolution.GetWidth());
        m_videoCapture->set(cv::CAP_PROP_FRAME_HEIGHT, resolution.GetHeight());

        if ( useMJPEG )
            m_videoCapture->set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
    }

    if ( !StartCameraThread() )
    {
        Clear();
        return false;
    }

    return true;
}

bool OpenCVFrame::StartCameraThread()
{
    DeleteCameraThread();

    m_cameraThread = new CameraThread(this, m_videoCapture);
    if ( m_cameraThread->Run() != wxTHREAD_NO_ERROR )
    {
        delete m_cameraThread;
        m_cameraThread = nullptr;
        wxLogError(_("Could not create the thread needed to load the data."));
        return false;
    }

    return true;
}

void OpenCVFrame::DeleteCameraThread()
{
    if ( m_cameraThread )
    {
        m_cameraThread->Delete();
        delete m_cameraThread;
        m_cameraThread = nullptr;
    }
}

void OpenCVFrame::OnImage(wxCommandEvent&)
{
    static wxString fileName;

    fileName = wxFileSelector("Select Bitmap Image", "", fileName, "",
        "Image files (*.jpg;*.png;*.tga;*.bmp)| *.jpg;*.png;*.tga;*.bmp",
        wxFD_OPEN | wxFD_FILE_MUST_EXIST, this);

    if ( fileName.empty() )
        return;

    cv::Mat matBitmap;
    wxStopWatch stopWatch;
    long timeGet = 0, timeConvert = 0;

    stopWatch.Start();
    matBitmap = cv::imread(fileName.ToStdString(), cv::IMREAD_COLOR);
    timeGet = stopWatch.Time();

    if ( matBitmap.empty() )
    {
        wxLogError("Could not read image '%s'.", fileName);
        return;
    }

    Clear();

    wxBitmap bitmap = ConvertMatToBitmap(matBitmap, timeConvert);

    if ( !bitmap.IsOk())
    {
        wxLogError("Could not convert Mat to wxBitmap.", fileName);
        Clear();
        return;
    }

    m_bitmapPanel->SetBitmap(bitmap, timeGet, timeConvert);
    m_propertiesButton->Enable();
    m_mode = Image;
    m_sourceName = fileName;
    UpdateFrameTitle();
}

void OpenCVFrame::OnVideo(wxCommandEvent&)
{
    static wxString fileName;

    fileName = wxFileSelector("Select Video", "", fileName, "",
        "Video files (*.avi;*.mp4;*.mkv)| *.avi;*.mp4;*.mkv",
        wxFD_OPEN | wxFD_FILE_MUST_EXIST, this);

    if ( fileName.empty() )
        return;

    cv::VideoCapture* cap = new cv::VideoCapture(fileName.ToStdString());
    int               frameCount = 0;

    if ( !cap->isOpened() )
    {
        wxLogError("Could not read video '%s'.", fileName);
        delete cap;
        Clear();
        return;
    }

    m_videoCapture = cap;
    m_mode = Video;
    m_sourceName = fileName;
    m_currentVideoFrameNumber = 0;

    UpdateFrameTitle();
    ShowVideoFrame(m_currentVideoFrameNumber);

    frameCount = m_videoCapture->get(cv::VideoCaptureProperties::CAP_PROP_FRAME_COUNT);
    m_videoSlider->SetValue(0);
    m_videoSlider->SetRange(0, frameCount - 1);
    m_videoSlider->Enable();
    m_videoSlider->Show();
    m_videoSlider->SetFocus();

    m_propertiesButton->Enable();
}


void OpenCVFrame::OnWebCam(wxCommandEvent&)
{
    static const wxSize resolutions[] = { { 320,  240},
                                          { 640,  480},
                                          { 800,  600},
                                          {1024,  576},
                                          {1280,  720},
                                          {1920, 1080} };
    static int    resolutionIndex = 1;
    wxArrayString resolutionStrings;
    bool          useMJPEG = false;

    for ( const auto& r : resolutions )
        resolutionStrings.push_back(wxString::Format("%d x %d", r.GetWidth(), r.GetHeight()));

    resolutionIndex = wxGetSingleChoiceIndex("Select resolution", "WebCam",
        resolutionStrings, resolutionIndex, this);
    if ( resolutionIndex == -1 )
        return;

    useMJPEG = wxMessageBox("Press Yes to use MJPEG or No to use the default FourCC.\nMJPEG may be much faster, particularly at higher resolutions.",
        "WebCamera", wxYES_NO, this) == wxYES;

    if ( StartCameraCapture(wxEmptyString, resolutions[resolutionIndex], useMJPEG ) )    {
        m_mode = WebCam;
        m_sourceName = "Default WebCam";
        UpdateFrameTitle();
        m_propertiesButton->Enable();
    }

}

void OpenCVFrame::OnIPCamera(wxCommandEvent&)
{
    static wxString address = "rtsp://freja.hiof.no:1935/rtplive/_definst_/hessdalen03.stream";

    address = wxGetTextFromUser("Enter the protocol, address, port etc.",
                                "IP camera", address, this);

    if ( address.empty() )
        return;

    if ( StartCameraCapture(address) )
    {
        m_mode = IPCamera;
        m_sourceName = address;
        UpdateFrameTitle();
        m_propertiesButton->Enable();
    }
}

void OpenCVFrame::OnClear(wxCommandEvent&)
{
    Clear();
}

void OpenCVFrame::OnProperties(wxCommandEvent&)
{
    wxArrayString properties;

    properties.push_back(wxString::Format("Source: %s", m_sourceName));

    if ( m_mode == Image )
    {
        const wxBitmap& bmp = m_bitmapPanel->GetBitmap();

        properties.push_back(wxString::Format("Width: %d", bmp.GetWidth()));
        properties.push_back(wxString::Format("Height: %d", bmp.GetHeight()));
    }

    if ( m_videoCapture )
    {
        const int  fourCCInt   = static_cast<int>(m_videoCapture->get(cv::CAP_PROP_FOURCC));
        const char fourCCStr[] = {(char)(fourCCInt  & 0XFF),
                                  (char)((fourCCInt & 0XFF00) >> 8),
                                  (char)((fourCCInt & 0XFF0000) >> 16),
                                  (char)((fourCCInt & 0XFF000000) >> 24), 0};

        properties.push_back(wxString::Format("Backend: %s", wxString(m_videoCapture->getBackendName())));

        properties.push_back(wxString::Format("Width: %.0f", m_videoCapture->get(cv::CAP_PROP_FRAME_WIDTH)));
        properties.push_back(wxString::Format("Height: %0.f", m_videoCapture->get(cv::CAP_PROP_FRAME_HEIGHT)));

        properties.push_back(wxString::Format("FourCC: %s", fourCCStr));
        properties.push_back(wxString::Format("FPS: %.1f", m_videoCapture->get(cv::CAP_PROP_FPS)));

        if ( m_mode == Video )
        {
           // abuse wxDateTime to display position in video as time
           wxDateTime time(static_cast<time_t>(m_videoCapture->get(cv::CAP_PROP_POS_MSEC) / 1000));

           time.MakeUTC(true);

           properties.push_back(wxString::Format("Current frame: %.0f", m_videoCapture->get(cv::CAP_PROP_POS_FRAMES) - 1.0));
           properties.push_back(wxString::Format("Current time: %s", time.FormatISOTime()));
           properties.push_back(wxString::Format("Total frame count: %.f", m_videoCapture->get(cv::CAP_PROP_FRAME_COUNT)));
           properties.push_back(wxString::Format("Bitrate: %.0f kbits/s", m_videoCapture->get(cv::CAP_PROP_BITRATE)));
        }
    }

    wxGetSingleChoice("Name: value", "Properties", properties, this);
}

void OpenCVFrame::OnVideoSetFrame(wxCommandEvent& evt)
{
    wxCHECK_RET(m_videoCapture, "OnVideoSetFrame() called without valid VideoCapture");

    const int requestedFrameNumber = evt.GetInt();

    if ( requestedFrameNumber == m_currentVideoFrameNumber )
        return;

    m_currentVideoFrameNumber = requestedFrameNumber;
    ShowVideoFrame(m_currentVideoFrameNumber);
}

void OpenCVFrame::OnCameraFrame(wxThreadEvent& evt)
{
    CameraThread::CameraFrame* frame = evt.GetPayload<CameraThread::CameraFrame*>();

    // After deleting the camera thread we may still get a stray frame
    // from yet unprocessed event, just silently drop it.
    if ( m_mode != IPCamera && m_mode != WebCam )
    {
        delete frame;
        return;
    }

    long     timeConvert = 0;
    wxBitmap bitmap = ConvertMatToBitmap(frame->matBitmap, timeConvert);

    if ( bitmap.IsOk() )
        m_bitmapPanel->SetBitmap(bitmap, frame->timeGet, timeConvert);
    else
        m_bitmapPanel->SetBitmap(wxBitmap(), 0, 0);

    delete frame;
}

void OpenCVFrame::OnCameraEmpty(wxThreadEvent&)
{
    wxLogError("Connection to the camera lost.");

    Clear();
}