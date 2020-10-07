///////////////////////////////////////////////////////////////////////////////
// Name:        ocvapp.cpp
// Purpose:     Application skeleton
// Author:      PB
// Created:     2020-09-16
// Copyright:   (c) 2020 PB
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#include <wx/wx.h>

#include "ocvframe.h"

class OpenCVApp : public wxApp
{
public:
	bool OnInit() override
	{
		SetVendorName("PB");
		SetAppName("wxOpenCVTest");

        (new OpenCVFrame)->Show();
        return true;
	}
}; wxIMPLEMENT_APP(OpenCVApp);