/***
 * millipede: PartitionView.cpp
 * Copyright Stuart Golodetz, 2009. All rights reserved.
 ***/

#include "PartitionView.h"

#include <wx/button.h>
#include <wx/glcanvas.h>
#include <wx/numdlg.h>
#include <wx/sizer.h>
#include <wx/slider.h>
#include <wx/stattext.h>

#include <common/dicom/volumes/DICOMVolume.h>
#include <common/partitionforests/images/MosaicImageCreator.h>
#include <common/slices/SliceTextureSetFiller.h>
#include <mast/gui/dialogs/DialogUtil.h>
#include <mast/gui/overlays/IPFMultiFeatureSelectionOverlay.h>
#include <mast/gui/overlays/IPFSelectionOverlay.h>
#include <mast/gui/overlays/PartitionOverlayManager.h>
#include <mast/util/StringConversion.h>
#include "DICOMCanvas.h"
#include "PartitionCamera.h"
#include "PartitionCanvas.h"
using namespace mp;

namespace {

//#################### LOCAL CONSTANTS ####################
enum
{
	ID_BASE = wxID_HIGHEST + 1000,	// a dummy value which is never used: subsequent values are guaranteed to be higher than this
	BUTTONID_SEGMENT_VOLUME,
	BUTTONID_VIEW_XY,
	BUTTONID_VIEW_XZ,
	BUTTONID_VIEW_YZ,
	BUTTONID_VISUALIZE_IN_3D,
	SLIDERID_X,
	SLIDERID_Y,
	SLIDERID_Z,
	SLIDERID_LAYER,
	SLIDERID_ZOOM,
};

}

namespace mp {

//#################### LISTENERS ####################
struct PartitionView::CameraListener : PartitionCamera::Listener
{
	PartitionView *base;

	explicit CameraListener(PartitionView *base_)
	:	base(base_)
	{}

	void slice_location_changed(bool sliceChanged, bool layerChanged)
	{
		base->update_sliders();
		if(sliceChanged || layerChanged) base->recreate_overlays();
		base->refresh_canvases();
	}

	void slice_orientation_changed()
	{
		base->recreate_overlays();
		base->refresh_canvases();
	}

	void zoom_level_changed()
	{
		base->update_sliders();
		base->refresh_canvases();
	}
};

struct PartitionView::ModelListener : PartitionView::PartitionModelT::Listener
{
	PartitionView *base;

	explicit ModelListener(PartitionView *base_)
	:	base(base_)
	{}

	void forest_changed()
	{
		base->create_partition_textures();
		base->recreate_overlays();
		base->refresh_canvases();
		base->add_selection_listeners();
	}
};

struct PartitionView::MultiFeatureSelectionListener : PartitionModelT::VolumeIPFMultiFeatureSelectionT::Listener
{
	PartitionView *base;

	explicit MultiFeatureSelectionListener(PartitionView *base_)
	:	base(base_)
	{}

	void multi_feature_selection_changed(int commandDepth)
	{
		if(commandDepth == 0)
		{
			base->recreate_multi_feature_selection_overlay();
			base->refresh_canvases();
		}
	}
};

struct PartitionView::SelectionListener : PartitionModelT::VolumeIPFSelectionT::Listener
{
	PartitionView *base;

	explicit SelectionListener(PartitionView *base_)
	:	base(base_)
	{}

	void selection_changed(int commandDepth)
	{
		if(commandDepth == 0)
		{
			base->recreate_selection_overlay();
			base->refresh_canvases();
		}
	}
};

//#################### CONSTRUCTORS ####################
PartitionView::PartitionView(wxWindow *parent, const PartitionModel_Ptr& model, const ICommandManager_Ptr& commandManager, wxGLContext *context)
:	wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(100,100)),
	m_camera(new PartitionCamera(
		initial_slice_location(model->dicom_volume_choice()),
		ORIENT_XY,
		model->dicom_volume()->size()
	)),
	m_commandManager(commandManager),
	m_model(model),
	m_overlayManager(new PartitionOverlayManager)
{
	m_camera->add_shared_listener(boost::shared_ptr<CameraListener>(new CameraListener(this)));
	m_model->add_shared_listener(boost::shared_ptr<ModelListener>(new ModelListener(this)));
	m_model->set_command_manager(commandManager);

	calculate_canvas_size();
	setup_gui(context);

	m_dicomCanvas->setup(this);
	m_partitionCanvas->setup(this);

	fit_image_to_view();
	create_dicom_textures();
	create_overlays();
}

//#################### PUBLIC METHODS ####################
const PartitionCamera_Ptr& PartitionView::camera()
{
	return m_camera;
}

PartitionCamera_CPtr PartitionView::camera() const
{
	return m_camera;
}

void PartitionView::fit_image_to_view()
{
	m_dicomCanvas->fit_image_to_canvas();
}

wxGLContext *PartitionView::get_context() const
{
	return m_dicomCanvas->GetContext();
}

void PartitionView::goto_slice()
{
	wxSlider *slider = NULL;
	switch(m_camera->slice_orientation())
	{
		case ORIENT_XY:	slider = m_zSlider; break;
		case ORIENT_XZ:	slider = m_ySlider; break;
		case ORIENT_YZ:	slider = m_xSlider; break;
	}
	assert(slider != NULL);

	SliceLocation loc = m_camera->slice_location();
	long minValue = slider->GetMin(), maxValue = slider->GetMax();
	long curValue = minValue + loc[m_camera->slice_orientation()];
	long newValue = wxGetNumberFromUser(wxT(""), wxT("Slice Number:"), wxT("Goto Slice"), curValue, minValue, maxValue, this);
	if(newValue != -1)
	{
		loc[m_camera->slice_orientation()] = newValue - minValue;
		m_camera->set_slice_location(loc);
	}
}

SliceLocation PartitionView::initial_slice_location(const DICOMVolumeChoice& volumeChoice)
{
	return SliceLocation((volumeChoice.maxX - volumeChoice.minX)/2, (volumeChoice.maxY - volumeChoice.minY)/2, (volumeChoice.maxZ - volumeChoice.minZ)/2, 0);
}

const PartitionView::PartitionModel_Ptr& PartitionView::model()
{
	return m_model;
}

PartitionView::PartitionModel_CPtr PartitionView::model() const
{
	return m_model;
}

//#################### PRIVATE METHODS ####################
void PartitionView::add_selection_listeners()
{
	m_model->multi_feature_selection()->add_shared_listener(boost::shared_ptr<MultiFeatureSelectionListener>(new MultiFeatureSelectionListener(this)));
	m_model->selection()->add_shared_listener(boost::shared_ptr<SelectionListener>(new SelectionListener(this)));
}

void PartitionView::calculate_canvas_size()
{
	// We want our canvases to be at least 512x512, but beyond that their size should be dictated by the sizes
	// of the images. We want to be able to show the images in axial (X-Y), sagittal (Y-Z) and coronal (X-Z)
	// orientations, which dictates which dimensions we need to take into account for the canvas sizes.

	itk::Size<3> volumeSize = m_model->dicom_volume()->size();
	m_canvasWidth = std::max<int>(512, std::max(volumeSize[0], volumeSize[1]));
	m_canvasHeight = std::max<int>(512, std::max(volumeSize[1], volumeSize[2]));
}

void PartitionView::create_dicom_textures()
{
	m_dicomTextureSet.reset(new SliceTextureSet);

	DICOMVolume::WindowedImagePointer windowedImage = m_model->dicom_volume()->windowed_image(volume_choice().windowSettings);
	Job_Ptr job = fill_dicom_textures_job(m_camera->slice_orientation(), windowedImage);
	execute_with_progress_dialog(job, this, "Creating DICOM Texture Set", false);
}

void PartitionView::create_overlays()
{
	m_overlayManager->clear_overlays();
	m_overlayManager->insert_overlay_at_top("IPFMultiFeatureSelection", multi_feature_selection_overlay());
	m_overlayManager->insert_overlay_at_top("IPFSelection", selection_overlay());
}

void PartitionView::create_partition_textures()
{
	typedef VolumeIPF<DICOMImageLeafLayer,DICOMImageBranchLayer> CTVolumeIPF;
	typedef boost::shared_ptr<const CTVolumeIPF> CTVolumeIPF_CPtr;

	CTVolumeIPF_CPtr volumeIPF = m_model->volume_ipf();
	if(!volumeIPF) return;
	int highestLayer = volumeIPF->highest_layer();

	m_partitionTextureSets = std::vector<SliceTextureSet_Ptr>(highestLayer);
	for(int layer=1; layer<=highestLayer; ++layer) m_partitionTextureSets[layer-1].reset(new SliceTextureSet);

	Job_Ptr job = fill_partition_textures_job(m_camera->slice_orientation());
	execute_with_progress_dialog(job, this, "Creating Partition Texture Sets", false);

	m_layerSlider->SetRange(1, highestLayer);
	m_camera->set_highest_layer(highestLayer);
	SliceLocation loc = m_camera->slice_location();
	m_camera->set_slice_location(SliceLocation(loc.x, loc.y, loc.z, (1+highestLayer)/2));
}

SliceTextureSet_CPtr PartitionView::dicom_texture_set() const
{
	return m_dicomTextureSet;
}

Job_Ptr PartitionView::fill_dicom_textures_job(SliceOrientation ori, const itk::Image<unsigned char,3>::Pointer& windowedImage) const
{
	SliceTextureSetFiller<unsigned char> *job = new SliceTextureSetFiller<unsigned char>(ori, m_model->dicom_volume()->size(), m_dicomTextureSet);
	job->set_volume_image(windowedImage);
	return Job_Ptr(job);
}

Job_Ptr PartitionView::fill_partition_textures_job(SliceOrientation ori) const
{
	typedef VolumeIPF<DICOMImageLeafLayer,DICOMImageBranchLayer> VolumeIPFT;
	typedef boost::shared_ptr<const VolumeIPFT> VolumeIPF_CPtr;

	VolumeIPF_CPtr volumeIPF = m_model->volume_ipf();
	int highestLayer = volumeIPF->highest_layer();

	CompositeJob_Ptr job(new CompositeJob);
	for(int layer=1; layer<=highestLayer; ++layer)
	{
		typedef MosaicImageCreator<DICOMImageLeafLayer,DICOMImageBranchLayer> MIC;
		typedef SliceTextureSetFiller<unsigned char> TSF;

		MIC *mosaicImageCreator = new MIC(volumeIPF, layer, ori, true);
		TSF *textureSetFiller = new TSF(ori, volumeIPF->volume_size(), m_partitionTextureSets[layer-1]);
		textureSetFiller->set_volume_image_hook(mosaicImageCreator->get_mosaic_image_hook());

		job->add_subjob(mosaicImageCreator);
		job->add_subjob(textureSetFiller);
	}
	return job;
}

void PartitionView::fill_textures(SliceOrientation ori)
{
	CompositeJob_Ptr job(new CompositeJob);

	if(!m_dicomTextureSet->has_textures(ori))
	{
		DICOMVolume::WindowedImagePointer windowedImage = m_model->dicom_volume()->windowed_image(volume_choice().windowSettings);
		job->add_subjob(fill_dicom_textures_job(ori, windowedImage));
	}

	if(!m_partitionTextureSets.empty() && !m_partitionTextureSets[0]->has_textures(ori))
	{
		job->add_subjob(fill_partition_textures_job(ori));
	}

	if(!job->empty()) execute_with_progress_dialog(job, this, "Creating Textures", false);
}

PartitionOverlay *PartitionView::multi_feature_selection_overlay() const
{
	PartitionModelT::VolumeIPFMultiFeatureSelection_CPtr multiFeatureSelection = m_model->multi_feature_selection();
	if(multiFeatureSelection)
	{
		SliceLocation loc = m_camera->slice_location();
		SliceOrientation ori = m_camera->slice_orientation();
		Map<AbdominalFeature::Enum,RGBA32> colourMap;
		colourMap.set(AbdominalFeature::AORTA,				ITKImageUtil::make_rgba32(255,0,0,50));
		colourMap.set(AbdominalFeature::INFERIOR_VENA_CAVA,	ITKImageUtil::make_rgba32(0,0,255,50));
		colourMap.set(AbdominalFeature::KIDNEY,				ITKImageUtil::make_rgba32(255,255,0,50));
		colourMap.set(AbdominalFeature::LIVER,				ITKImageUtil::make_rgba32(128,0,128,50));
		colourMap.set(AbdominalFeature::OTHER_ARTERY,		ITKImageUtil::make_rgba32(128,0,0,50));
		colourMap.set(AbdominalFeature::OTHER_VEIN,			ITKImageUtil::make_rgba32(0,0,128,50));
		colourMap.set(AbdominalFeature::SPINAL_CORD,		ITKImageUtil::make_rgba32(128,255,255,100));
		colourMap.set(AbdominalFeature::SPLEEN,				ITKImageUtil::make_rgba32(0,255,0,50));
		colourMap.set(AbdominalFeature::VERTEBRA,			ITKImageUtil::make_rgba32(192,255,255,100));
		return new IPFMultiFeatureSelectionOverlay(multiFeatureSelection, loc, ori, colourMap);
	}
	else return NULL;
}

PartitionOverlayManager_CPtr PartitionView::overlay_manager() const
{
	return m_overlayManager;
}

SliceTextureSet_CPtr PartitionView::partition_texture_set(int layer) const
{
	int n = layer - 1;
	if(0 <= n && n < static_cast<int>(m_partitionTextureSets.size())) return m_partitionTextureSets[n];
	else return SliceTextureSet_CPtr();
}

void PartitionView::recreate_multi_feature_selection_overlay()
{
	m_overlayManager->replace_overlay("IPFMultiFeatureSelection", multi_feature_selection_overlay());
}

void PartitionView::recreate_overlays()
{
	recreate_multi_feature_selection_overlay();
	recreate_selection_overlay();
}

void PartitionView::recreate_selection_overlay()
{
	m_overlayManager->replace_overlay("IPFSelection", selection_overlay());
}

void PartitionView::refresh_canvases()
{
	m_dicomCanvas->Refresh();
	m_partitionCanvas->Refresh();
}

PartitionOverlay *PartitionView::selection_overlay() const
{
	PartitionModelT::VolumeIPFSelection_CPtr selection = m_model->selection();
	if(selection)
	{
		SliceLocation loc = m_camera->slice_location();
		SliceOrientation ori = m_camera->slice_orientation();
		return new IPFSelectionOverlay(selection, loc, ori);
	}
	else return NULL;
}

void PartitionView::setup_gui(wxGLContext *context)
{
	SetBackgroundColour(wxColour(240,240,240));

	wxFlexGridSizer *sizer = new wxFlexGridSizer(3, 3, 5, 5);
	SetSizer(sizer);

	int attribList[] =
	{
		WX_GL_RGBA,
		WX_GL_DEPTH_SIZE,
		16,
		WX_GL_DOUBLEBUFFER,
		0
	};

	// Top left
	sizer->Add(new wxPanel(this));

	// Top middle
	sizer->Add(new wxPanel(this));

	// Top right
	m_segmentVolumeButton = new wxButton(this, BUTTONID_SEGMENT_VOLUME, wxT("Segment Volume..."));
	sizer->Add(m_segmentVolumeButton, 0, wxALIGN_CENTRE_HORIZONTAL);

	// Middle left
	m_dicomCanvas = new DICOMCanvas(this, context, attribList, wxID_ANY, wxDefaultPosition, wxSize(m_canvasWidth, m_canvasHeight));
	sizer->Add(m_dicomCanvas);

	// Middle
	wxPanel *middle = new wxPanel(this);
	wxBoxSizer *middleSizer = new wxBoxSizer(wxVERTICAL);
	middle->SetSizer(middleSizer);
		wxButton *viewXYButton = new wxButton(middle, BUTTONID_VIEW_XY, wxT("View X-Y (usually Axial)"));
		middleSizer->Add(viewXYButton, 0, wxALIGN_CENTRE_HORIZONTAL);

		wxButton *viewXZButton = new wxButton(middle, BUTTONID_VIEW_XZ, wxT("View X-Z (usually Coronal)"));
		middleSizer->Add(viewXZButton, 0, wxALIGN_CENTRE_HORIZONTAL);

		wxButton *viewYZButton = new wxButton(middle, BUTTONID_VIEW_YZ, wxT("View Y-Z (usually Sagittal)"));
		middleSizer->Add(viewYZButton, 0, wxALIGN_CENTRE_HORIZONTAL);

		middleSizer->AddSpacer(10);

		wxButton *visualizeIn3DButton = new wxButton(middle, BUTTONID_VISUALIZE_IN_3D, wxT("Visualize in 3D..."));
		middleSizer->Add(visualizeIn3DButton, 0, wxALIGN_CENTRE_HORIZONTAL);

		middleSizer->AddSpacer(20);

		wxPanel *locationControls = new wxPanel(middle);
		wxFlexGridSizer *locationControlsSizer = new wxFlexGridSizer(0, 2, 0, 0);
		locationControls->SetSizer(locationControlsSizer);
			wxStaticText *xText = new wxStaticText(locationControls, wxID_ANY, wxT("X: "));
			locationControlsSizer->Add(xText, 0, wxALIGN_CENTRE_VERTICAL);
			m_xSlider = new wxSlider(locationControls, SLIDERID_X, volume_choice().minX + m_camera->slice_location().x, volume_choice().minX, volume_choice().maxX, wxDefaultPosition, wxSize(100,50), wxHORIZONTAL|wxSL_AUTOTICKS|wxSL_LABELS|wxSL_TOP);
			locationControlsSizer->Add(m_xSlider, 0, wxALIGN_CENTRE);

			wxStaticText *yText = new wxStaticText(locationControls, wxID_ANY, wxT("Y: "));
			locationControlsSizer->Add(yText, 0, wxALIGN_CENTRE_VERTICAL);
			m_ySlider = new wxSlider(locationControls, SLIDERID_Y, volume_choice().minY + m_camera->slice_location().y, volume_choice().minY, volume_choice().maxY, wxDefaultPosition, wxSize(100,50), wxHORIZONTAL|wxSL_AUTOTICKS|wxSL_LABELS|wxSL_TOP);
			locationControlsSizer->Add(m_ySlider, 0, wxALIGN_CENTRE);

			wxStaticText *zText = new wxStaticText(locationControls, wxID_ANY, wxT("Z: "));
			locationControlsSizer->Add(zText, 0, wxALIGN_CENTRE_VERTICAL);
			m_zSlider = new wxSlider(locationControls, SLIDERID_Z, volume_choice().minZ+1 + m_camera->slice_location().z, volume_choice().minZ+1, volume_choice().maxZ+1, wxDefaultPosition, wxSize(100,50), wxHORIZONTAL|wxSL_AUTOTICKS|wxSL_LABELS|wxSL_TOP);
			locationControlsSizer->Add(m_zSlider, 0, wxALIGN_CENTRE);

			wxStaticText *zoomText = new wxStaticText(locationControls, wxID_ANY, wxT("Zoom: "));
			locationControlsSizer->Add(zoomText, 0, wxALIGN_CENTRE_VERTICAL);
			m_zoomSlider = new wxSlider(locationControls, SLIDERID_ZOOM, m_camera->zoom_level(), m_camera->min_zoom_level(), m_camera->max_zoom_level(), wxDefaultPosition, wxSize(100,50), wxHORIZONTAL|wxSL_AUTOTICKS|wxSL_LABELS|wxSL_TOP);
			locationControlsSizer->Add(m_zoomSlider, 0, wxALIGN_CENTRE);
		middleSizer->Add(locationControls, 0, wxALIGN_CENTRE_HORIZONTAL);
	sizer->Add(middle);

	// Middle right
	m_partitionCanvas = new PartitionCanvas(this, get_context(), attribList, wxID_ANY, wxDefaultPosition, wxSize(m_canvasWidth, m_canvasHeight));
	sizer->Add(m_partitionCanvas);

	// Bottom left
	sizer->Add(new wxPanel(this));

	// Bottom middle
	sizer->Add(new wxPanel(this));

	// Bottom right
	wxPanel *bottomRight = new wxPanel(this);
	wxFlexGridSizer *bottomRightSizer = new wxFlexGridSizer(0, 2, 0, 0);
	bottomRight->SetSizer(bottomRightSizer);
		wxStaticText *layerText = new wxStaticText(bottomRight, wxID_ANY, wxT("Layer: "));
		bottomRightSizer->Add(layerText, 0, wxALIGN_CENTRE_VERTICAL);
		m_layerSlider = new wxSlider(bottomRight, SLIDERID_LAYER, 0, 0, 1, wxDefaultPosition, wxSize(100,50), wxHORIZONTAL|wxSL_AUTOTICKS|wxSL_LABELS|wxSL_TOP);
		bottomRightSizer->Add(m_layerSlider, 0, wxALIGN_CENTRE);
	sizer->Add(bottomRight, 0, wxALIGN_CENTRE_HORIZONTAL);

	sizer->Fit(this);
}

void PartitionView::update_sliders()
{
	SliceLocation loc = m_camera->slice_location();
	m_xSlider->SetValue(m_xSlider->GetMin() + loc.x);
	m_ySlider->SetValue(m_ySlider->GetMin() + loc.y);
	m_zSlider->SetValue(m_zSlider->GetMin() + loc.z);
	m_layerSlider->SetValue(loc.layer);
	m_zoomSlider->SetValue(m_camera->zoom_level());
}

const DICOMVolumeChoice& PartitionView::volume_choice() const
{
	return m_model->dicom_volume_choice();
}

void PartitionView::zoom_to_fit()
{
	m_dicomCanvas->zoom_to_fit();
}

//#################### EVENT HANDLERS ####################

//~~~~~~~~~~~~~~~~~~~~ BUTTONS ~~~~~~~~~~~~~~~~~~~~
void PartitionView::OnButtonSegmentVolume(wxCommandEvent&)
{
	m_model->segment_volume(this);
}

void PartitionView::OnButtonViewXY(wxCommandEvent&)
{
	fill_textures(ORIENT_XY);
	m_camera->set_slice_orientation(ORIENT_XY);
	zoom_to_fit();
}

void PartitionView::OnButtonViewXZ(wxCommandEvent&)
{
	fill_textures(ORIENT_XZ);
	m_camera->set_slice_orientation(ORIENT_XZ);
	zoom_to_fit();
}

void PartitionView::OnButtonViewYZ(wxCommandEvent&)
{
	fill_textures(ORIENT_YZ);
	m_camera->set_slice_orientation(ORIENT_YZ);
	zoom_to_fit();
}

void PartitionView::OnButtonVisualizeIn3D(wxCommandEvent&)
{
	m_model->visualize_in_3d(this, get_context());
}

//~~~~~~~~~~~~~~~~~~~~ SLIDERS ~~~~~~~~~~~~~~~~~~~~
void PartitionView::OnSliderX(wxScrollEvent&)
{
	SliceLocation loc = m_camera->slice_location();
	m_camera->set_slice_location(SliceLocation(m_xSlider->GetValue() - m_xSlider->GetMin(), loc.y, loc.z, loc.layer));
}

void PartitionView::OnSliderY(wxScrollEvent&)
{
	SliceLocation loc = m_camera->slice_location();
	m_camera->set_slice_location(SliceLocation(loc.x, m_ySlider->GetValue() - m_ySlider->GetMin(), loc.z, loc.layer));
}

void PartitionView::OnSliderZ(wxScrollEvent&)
{
	SliceLocation loc = m_camera->slice_location();
	m_camera->set_slice_location(SliceLocation(loc.x, loc.y, m_zSlider->GetValue() - m_zSlider->GetMin(), loc.layer));
}

void PartitionView::OnSliderLayer(wxScrollEvent&)
{
	SliceLocation loc = m_camera->slice_location();
	m_camera->set_slice_location(SliceLocation(loc.x, loc.y, loc.z, m_layerSlider->GetValue()));
}

void PartitionView::OnSliderZoom(wxScrollEvent&)
{
	m_camera->set_zoom_level(m_zoomSlider->GetValue());
}

//~~~~~~~~~~~~~~~~~~~~ UI UPDATES ~~~~~~~~~~~~~~~~~~~~
void PartitionView::OnUpdateForestNeeder(wxUpdateUIEvent& e)
{
	e.Enable(m_model->volume_ipf());
}

void PartitionView::OnUpdateSliderLayer(wxUpdateUIEvent& e)
{
	e.Enable(partition_texture_set(1).get() != NULL);
}

//#################### EVENT TABLE ####################
BEGIN_EVENT_TABLE(PartitionView, wxPanel)
	//~~~~~~~~~~~~~~~~~~~~ BUTTONS ~~~~~~~~~~~~~~~~~~~~
	EVT_BUTTON(BUTTONID_SEGMENT_VOLUME, PartitionView::OnButtonSegmentVolume)
	EVT_BUTTON(BUTTONID_VIEW_XY, PartitionView::OnButtonViewXY)
	EVT_BUTTON(BUTTONID_VIEW_XZ, PartitionView::OnButtonViewXZ)
	EVT_BUTTON(BUTTONID_VIEW_YZ, PartitionView::OnButtonViewYZ)
	EVT_BUTTON(BUTTONID_VISUALIZE_IN_3D, PartitionView::OnButtonVisualizeIn3D)

	//~~~~~~~~~~~~~~~~~~~~ SLIDERS ~~~~~~~~~~~~~~~~~~~~
	EVT_COMMAND_SCROLL(SLIDERID_X, PartitionView::OnSliderX)
	EVT_COMMAND_SCROLL(SLIDERID_Y, PartitionView::OnSliderY)
	EVT_COMMAND_SCROLL(SLIDERID_Z, PartitionView::OnSliderZ)
	EVT_COMMAND_SCROLL(SLIDERID_LAYER, PartitionView::OnSliderLayer)
	EVT_COMMAND_SCROLL(SLIDERID_ZOOM, PartitionView::OnSliderZoom)

	//~~~~~~~~~~~~~~~~~~~~ UI UPDATES ~~~~~~~~~~~~~~~~~~~~
	EVT_UPDATE_UI(BUTTONID_VISUALIZE_IN_3D, PartitionView::OnUpdateForestNeeder)
	EVT_UPDATE_UI(SLIDERID_LAYER, PartitionView::OnUpdateSliderLayer)
END_EVENT_TABLE()

}
