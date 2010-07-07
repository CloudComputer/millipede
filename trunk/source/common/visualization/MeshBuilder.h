/***
 * millipede: MeshBuilder.h
 * Copyright Stuart Golodetz, 2010. All rights reserved.
 ***/

#ifndef H_MILLIPEDE_MESHBUILDER
#define H_MILLIPEDE_MESHBUILDER

#include <functional>

#include <itkImage.h>

#include <common/jobs/CompositeJob.h>
#include "CubeFaceGenerator.h"

namespace mp {

/**
@brief	A MeshBuilder builds a 3D mesh from a 3D label image using the multiple material marching cubes (M3C) algorithm.

@tparam	Label			The type of label to be used
@tparam	PriorityPred	A predicate type defining an ordering over the labels for resolving conflicts that arise during the algorithm
*/
template <typename Label, typename PriorityPred = std::less<Label> >
class MeshBuilder : public CompositeJob
{
	//#################### TYPEDEFS ####################
public:
	typedef itk::Image<Label,3> LabelImage;
	typedef typename LabelImage::Pointer LabelImagePointer;
private:
	typedef MeshBuildingData<Label> MeshBuildingDataT;
	typedef boost::shared_ptr<MeshBuildingDataT> MeshBuildingData_Ptr;

	//#################### PRIVATE VARIABLES ####################
private:
	MeshBuildingData_Ptr m_data;

	//#################### CONSTRUCTORS ####################
public:
	/**
	@brief	Constructs the MeshBuilder.

	@note	The volume size must be passed in explicitly here because although the label image may not exist yet,
			its size must be known in order to generate the appropriate mesh-building subjobs.
	@note	If a labelling is not passed in here, it must later be set explicitly using set_labelling().

	@param[in]	volumeSize	The size of the labelled volume
	@param[in]	labelling	An optional itk::SmartPointer to a label image
	@pre
		-	If a labelling is passed in here, it must be non-null
		-	If a labelling is passed in here, its size must equal volumeSize
	*/
	explicit MeshBuilder(const itk::Size<3>& volumeSize, const boost::optional<LabelImagePointer>& labelling = boost::none)
	:	m_data(new MeshBuildingDataT)
	{
		if(labelling) set_labelling(*labelling);

		// Add the CubeFaceGenerator subjobs.
		int xSize = volumeSize[0] - 1, ySize = volumeSize[1] - 1, zSize = volumeSize[2] - 1;
		int xDim[3], yDim[3], zDim[3];
		xDim[CubeFaceDesignator::FACE_XY] = xSize;		xDim[CubeFaceDesignator::FACE_XZ] = xSize;		xDim[CubeFaceDesignator::FACE_YZ] = xSize+1;
		yDim[CubeFaceDesignator::FACE_XY] = ySize;		yDim[CubeFaceDesignator::FACE_XZ] = ySize+1;	yDim[CubeFaceDesignator::FACE_YZ] = ySize;
		zDim[CubeFaceDesignator::FACE_XY] = zSize+1;	zDim[CubeFaceDesignator::FACE_XZ] = zSize;		zDim[CubeFaceDesignator::FACE_YZ] = zSize;

		for(CubeFaceDesignator::Enum f=enum_begin<CubeFaceDesignator::Enum>(), end=enum_end<CubeFaceDesignator::Enum>(); f!=end; ++f)
			for(int x=0; x<xDim[f]; ++x)
				for(int y=0; y<yDim[f]; ++y)
					for(int z=0; z<zDim[f]; ++z)
					{
						add_subjob(new CubeFaceGenerator<Label,PriorityPred>(m_data, x, y, z, f));
					}

		// TODO
	}

	//#################### PUBLIC METHODS ####################
public:
	void set_labelling(const LabelImagePointer& labelling)
	{
		m_data->set_labelling(labelling);
	}

	void set_labelling_hook(const DataHook<LabelImagePointer>& labellingHook)
	{
		m_data->set_labelling_hook(labellingHook);
	}
};

}

#endif