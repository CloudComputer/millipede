/***
 * millipede: CTIPFBuilder.cpp
 * Copyright Stuart Golodetz, 2010. All rights reserved.
 ***/

#include "CTIPFBuilder.h"

#include <itkCastImageFilter.h>
#include <itkGradientAnisotropicDiffusionImageFilter.h>
#include <itkGradientMagnitudeImageFilter.h>

#include <common/adts/RootedMST.h>
#include <common/dicom/volumes/DICOMVolume.h>
#include <common/exceptions/Exception.h>
#include <common/segmentation/waterfall/NichollsWaterfallPass.h>
#include <common/segmentation/watershed/MeijsterRoerdinkWatershed.h>
#include "ForestBuildingWaterfallPassListener.h"

namespace mp {

//#################### CONSTRUCTORS ####################
CTIPFBuilder::CTIPFBuilder(const DICOMVolume_CPtr& volume, const CTSegmentationOptions& segmentationOptions, IPF_Ptr& ipf)
:	m_ipf(ipf), m_segmentationOptions(segmentationOptions), m_volume(new DICOMVolume_CPtr(volume))
{}

CTIPFBuilder::CTIPFBuilder(const boost::shared_ptr<DICOMVolume_CPtr>& volume, const CTSegmentationOptions& segmentationOptions, IPF_Ptr& ipf)
:	m_ipf(ipf), m_segmentationOptions(segmentationOptions), m_volume(volume)
{}

//#################### PUBLIC METHODS ####################
void CTIPFBuilder::execute()
{
	typedef itk::Image<int,3> GradientMagnitudeImage;
	typedef itk::Image<int,3> HounsfieldImage;
	typedef itk::Image<float,3> RealImage;
	typedef itk::Image<unsigned char,3> WindowedImage;

	HounsfieldImage::Pointer hounsfieldImage = (*m_volume)->base_image();

	//~~~~~~~
	// STEP 1
	//~~~~~~~

	set_status("Preprocessing image...");

	// Construct the windowed image.
	WindowedImage::Pointer windowedImage = (*m_volume)->windowed_image(m_segmentationOptions.windowSettings);
	if(is_aborted()) return;

	// Cast the input image (whether Hounsfield or windowed) to make its pixels real-valued.
	RealImage::Pointer realImage;
	switch(m_segmentationOptions.inputType)
	{
		case CTSegmentationOptions::INPUTTYPE_HOUNSFIELD:
		{
			typedef itk::CastImageFilter<HounsfieldImage,RealImage> CastFilter;
			CastFilter::Pointer castFilter = CastFilter::New();
			castFilter->SetInput(hounsfieldImage);
			castFilter->Update();
			realImage = castFilter->GetOutput();
			break;
		}
		case CTSegmentationOptions::INPUTTYPE_WINDOWED:
		{
			typedef itk::CastImageFilter<WindowedImage,RealImage> CastFilter;
			CastFilter::Pointer castFilter = CastFilter::New();
			castFilter->SetInput(windowedImage);
			castFilter->Update();
			realImage = castFilter->GetOutput();
			break;
		}
		default:
		{
			throw Exception("Unknown CT segmentation input type");	// this should never happen
		}
	}
	if(is_aborted()) return;

	// Smooth this real image using anisotropic diffusion filtering.
	typedef itk::GradientAnisotropicDiffusionImageFilter<RealImage,RealImage> ADFilter;
	for(int i=0; i<30; ++i)
	{
		ADFilter::Pointer adFilter = ADFilter::New();
		adFilter->SetInput(realImage);
		adFilter->SetConductanceParameter(1.0);
		adFilter->SetNumberOfIterations(1);
		adFilter->SetTimeStep(0.0625);
		adFilter->Update();
		realImage = adFilter->GetOutput();
		if(is_aborted()) return;
	}

	// Calculate the gradient magnitude of the smoothed image.
	typedef itk::GradientMagnitudeImageFilter<RealImage,GradientMagnitudeImage> GMFilter;
	GMFilter::Pointer gmFilter = GMFilter::New();
	gmFilter->SetInput(realImage);
	gmFilter->SetUseImageSpacingOff();
	gmFilter->Update();
	GradientMagnitudeImage::Pointer gradientMagnitudeImage = gmFilter->GetOutput();
	if(is_aborted()) return;

	set_progress(1);

	//~~~~~~~
	// STEP 2
	//~~~~~~~

	set_status("Running watershed...");

	typedef MeijsterRoerdinkWatershed<GradientMagnitudeImage::PixelType,3> WS;

	// Specify the necessary offsets for 6-connectivity.
	WS::NeighbourOffsets offsets(6);
	offsets[0][0] = 0;	offsets[0][1] = 0;	offsets[0][2] = -1;
	offsets[1][0] = 0;	offsets[1][1] = -1;	offsets[1][2] = 0;
	offsets[2][0] = -1;	offsets[2][1] = 0;	offsets[2][2] = 0;
	offsets[3][0] = 1;	offsets[3][1] = 0;	offsets[3][2] = 0;
	offsets[4][0] = 0;	offsets[4][1] = 1;	offsets[4][2] = 0;
	offsets[5][0] = 0;	offsets[5][1] = 0;	offsets[5][2] = 1;

	// Run the watershed algorithm on the gradient magnitude image.
	WS ws(gradientMagnitudeImage, offsets);
	if(is_aborted()) return;

	set_progress(2);

	//~~~~~~~
	// STEP 3
	//~~~~~~~

	set_status("Creating initial partition forest...");
	boost::shared_ptr<CTImageLeafLayer> leafLayer(new CTImageLeafLayer(hounsfieldImage, windowedImage, gradientMagnitudeImage));
	if(is_aborted()) return;
	boost::shared_ptr<CTImageBranchLayer> lowestBranchLayer = IPF::make_lowest_branch_layer(leafLayer, ws.calculate_groups());
	if(is_aborted()) return;
	m_ipf.reset(new IPF(leafLayer, lowestBranchLayer));
	set_progress(3);

	//~~~~~~~
	// STEP 4
	//~~~~~~~

	set_status("Creating rooted MST for lowest branch layer...");
	RootedMST<int> mst(*lowestBranchLayer);
	if(is_aborted()) return;
	set_progress(4);

	//~~~~~~~
	// STEP 5
	//~~~~~~~

	set_status("Running waterfall...");

	// Iteratively run a Nicholls waterfall pass on the MST until the forest is built.
	typedef WaterfallPass<int>::Listener WaterfallPassListener;
	NichollsWaterfallPass<int> waterfallPass;
	boost::shared_ptr<WaterfallPassListener> listener = make_forest_building_waterfall_pass_listener(m_ipf);
	waterfallPass.add_listener(listener);
	while(mst.node_count() != 1 && m_ipf->highest_layer() < m_segmentationOptions.waterfallLayerLimit)
	{
		m_ipf->clone_layer(m_ipf->highest_layer());
		if(is_aborted()) return;
		waterfallPass.run(mst);
		if(is_aborted()) return;
	}

	set_finished();
}

int CTIPFBuilder::length() const
{
	return 5;
}

}
