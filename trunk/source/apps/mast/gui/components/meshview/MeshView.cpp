/***
 * millipede: MeshView.cpp
 * Copyright Stuart Golodetz, 2010. All rights reserved.
 ***/

#include "MeshView.h"

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/sizer.h>
#include <wx/slider.h>
#include <wx/stattext.h>

namespace mp {

//#################### CONSTRUCTORS ####################
MeshView::MeshView(wxWindow *parent, wxGLContext *context)
:	wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(100,100))
{
	setup_gui(context);

	// TODO: Setup mesh canvas.
}

//#################### PRIVATE METHODS ####################
void MeshView::setup_gui(wxGLContext *context)
{
	const int BORDER_SIZE = 15;

	SetBackgroundColour(wxColour(240,240,240));

	wxFlexGridSizer *sizer = new wxFlexGridSizer(3, 3, 5, 5);
	SetSizer(sizer);

	// Top left
	sizer->Add(new wxPanel(this));

	// Top middle
	wxButton *updateButton = new wxButton(this, wxID_ANY, wxT("Update Mesh..."));
	sizer->Add(updateButton, 0, wxALIGN_CENTRE_HORIZONTAL|wxTOP, BORDER_SIZE);

	// Top right
	sizer->Add(new wxPanel(this));

	// Middle left
	// TODO: Add checkboxes for all the features (via enumeration, not hard-coding!).
	wxPanel *featureControls = new wxPanel(this);
	wxBoxSizer *featureControlsSizer = new wxBoxSizer(wxVERTICAL);
	featureControls->SetSizer(featureControlsSizer);
		featureControlsSizer->Add(new wxCheckBox(featureControls, wxID_ANY, wxT("Kidney")));
	sizer->Add(featureControls, 0, wxALIGN_CENTRE|wxLEFT, BORDER_SIZE);

	// Middle
	// TODO: Replace this with the mesh canvas.
	wxPanel *meshCanvas = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(512,512));
	meshCanvas->SetBackgroundColour(wxColour(0,0,0));
	sizer->Add(meshCanvas, 0, wxALL, BORDER_SIZE);

	// Middle right
	// TODO: Get the slider ranges right.
	wxPanel *clippingControls = new wxPanel(this);
	wxFlexGridSizer *clippingControlsSizer = new wxFlexGridSizer(0, 2, 0, 0);
	clippingControls->SetSizer(clippingControlsSizer);
		clippingControlsSizer->Add(new wxStaticText(clippingControls, wxID_ANY, wxT("X Clip Lower:")), 0, wxALIGN_CENTRE_VERTICAL);
		clippingControlsSizer->Add(new wxSlider(clippingControls, wxID_ANY, 0, 0, 100, wxDefaultPosition, wxSize(100,50), wxHORIZONTAL|wxSL_AUTOTICKS|wxSL_LABELS|wxSL_TOP));

		clippingControlsSizer->Add(new wxStaticText(clippingControls, wxID_ANY, wxT("X Clip Upper:")), 0, wxALIGN_CENTRE_VERTICAL);
		clippingControlsSizer->Add(new wxSlider(clippingControls, wxID_ANY, 0, 0, 100, wxDefaultPosition, wxSize(100,50), wxHORIZONTAL|wxSL_AUTOTICKS|wxSL_LABELS|wxSL_TOP));

		clippingControlsSizer->Add(new wxStaticText(clippingControls, wxID_ANY, wxT("Y Clip Lower:")), 0, wxALIGN_CENTRE_VERTICAL);
		clippingControlsSizer->Add(new wxSlider(clippingControls, wxID_ANY, 0, 0, 100, wxDefaultPosition, wxSize(100,50), wxHORIZONTAL|wxSL_AUTOTICKS|wxSL_LABELS|wxSL_TOP));

		clippingControlsSizer->Add(new wxStaticText(clippingControls, wxID_ANY, wxT("Y Clip Upper:")), 0, wxALIGN_CENTRE_VERTICAL);
		clippingControlsSizer->Add(new wxSlider(clippingControls, wxID_ANY, 0, 0, 100, wxDefaultPosition, wxSize(100,50), wxHORIZONTAL|wxSL_AUTOTICKS|wxSL_LABELS|wxSL_TOP));

		clippingControlsSizer->Add(new wxStaticText(clippingControls, wxID_ANY, wxT("Z Clip Lower:")), 0, wxALIGN_CENTRE_VERTICAL);
		clippingControlsSizer->Add(new wxSlider(clippingControls, wxID_ANY, 0, 0, 100, wxDefaultPosition, wxSize(100,50), wxHORIZONTAL|wxSL_AUTOTICKS|wxSL_LABELS|wxSL_TOP));

		clippingControlsSizer->Add(new wxStaticText(clippingControls, wxID_ANY, wxT("Z Clip Upper:")), 0, wxALIGN_CENTRE_VERTICAL);
		clippingControlsSizer->Add(new wxSlider(clippingControls, wxID_ANY, 0, 0, 100, wxDefaultPosition, wxSize(100,50), wxHORIZONTAL|wxSL_AUTOTICKS|wxSL_LABELS|wxSL_TOP));
	sizer->Add(clippingControls, 0, wxALIGN_CENTRE|wxRIGHT, BORDER_SIZE);

	// Bottom left
	sizer->Add(new wxPanel(this));

	// Bottom middle
	// TODO: Get the slider ranges right.
	wxPanel *cameraControls = new wxPanel(this);
	wxBoxSizer *cameraControlsSizer = new wxBoxSizer(wxHORIZONTAL);
	cameraControls->SetSizer(cameraControlsSizer);
		wxPanel *centreControls = new wxPanel(cameraControls);
		wxFlexGridSizer *centreControlsSizer = new wxFlexGridSizer(0, 2, 0, 0);
		centreControls->SetSizer(centreControlsSizer);
			centreControlsSizer->Add(new wxStaticText(centreControls, wxID_ANY, wxT("Centre X:")), 0, wxALIGN_CENTRE_VERTICAL);
			centreControlsSizer->Add(new wxSlider(centreControls, wxID_ANY, 0, 0, 100, wxDefaultPosition, wxSize(100,50), wxHORIZONTAL|wxSL_AUTOTICKS|wxSL_LABELS|wxSL_TOP));

			centreControlsSizer->Add(new wxStaticText(centreControls, wxID_ANY, wxT("Centre Y:")), 0, wxALIGN_CENTRE_VERTICAL);
			centreControlsSizer->Add(new wxSlider(centreControls, wxID_ANY, 0, 0, 100, wxDefaultPosition, wxSize(100,50), wxHORIZONTAL|wxSL_AUTOTICKS|wxSL_LABELS|wxSL_TOP));

			centreControlsSizer->Add(new wxStaticText(centreControls, wxID_ANY, wxT("Centre Z:")), 0, wxALIGN_CENTRE_VERTICAL);
			centreControlsSizer->Add(new wxSlider(centreControls, wxID_ANY, 0, 0, 100, wxDefaultPosition, wxSize(100,50), wxHORIZONTAL|wxSL_AUTOTICKS|wxSL_LABELS|wxSL_TOP));
		cameraControlsSizer->Add(centreControls);

		cameraControlsSizer->AddSpacer(30);

		wxPanel *eyeControls = new wxPanel(cameraControls);
		wxFlexGridSizer *eyeControlsSizer = new wxFlexGridSizer(0, 2, 0, 0);
		eyeControls->SetSizer(eyeControlsSizer);
			eyeControlsSizer->Add(new wxStaticText(eyeControls, wxID_ANY, wxT("Distance:")), 0, wxALIGN_CENTRE_VERTICAL);
			eyeControlsSizer->Add(new wxSlider(eyeControls, wxID_ANY, 0, 0, 100, wxDefaultPosition, wxSize(100,50), wxHORIZONTAL|wxSL_AUTOTICKS|wxSL_LABELS|wxSL_TOP));

			eyeControlsSizer->Add(new wxStaticText(eyeControls, wxID_ANY, wxT("Azimuth:")), 0, wxALIGN_CENTRE_VERTICAL);
			eyeControlsSizer->Add(new wxSlider(eyeControls, wxID_ANY, 0, 0, 359, wxDefaultPosition, wxSize(100,50), wxHORIZONTAL|wxSL_AUTOTICKS|wxSL_LABELS|wxSL_TOP));

			eyeControlsSizer->Add(new wxStaticText(eyeControls, wxID_ANY, wxT("Inclination:")), 0, wxALIGN_CENTRE_VERTICAL);
			eyeControlsSizer->Add(new wxSlider(eyeControls, wxID_ANY, 0, 0, 100, wxDefaultPosition, wxSize(100,50), wxHORIZONTAL|wxSL_AUTOTICKS|wxSL_LABELS|wxSL_TOP));
		cameraControlsSizer->Add(eyeControls);
	sizer->Add(cameraControls, 0, wxALIGN_CENTRE_HORIZONTAL|wxBOTTOM, BORDER_SIZE);

	sizer->Fit(this);
}

}
