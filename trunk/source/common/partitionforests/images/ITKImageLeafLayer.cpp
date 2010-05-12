/***
 * millipede: ITKImageLeafLayer.cpp
 * Copyright Stuart Golodetz, 2010. All rights reserved.
 ***/

#include "ITKImageLeafLayer.h"

#include <cassert>

namespace mp {

//#################### CONSTRUCTORS ####################
ITKImageLeafLayer::ITKImageLeafLayer(int sizeX, int sizeY, int sizeZ, const std::vector<NodeProperties>& nodeProperties)
:	m_sizeX(sizeX), m_sizeY(sizeY), m_sizeZ(sizeZ)
{
	m_sizeXY = m_sizeX * m_sizeY;
	m_sizeXYZ = m_sizeXY * m_sizeZ;

	size_t size = nodeProperties.size();
	m_nodes.reserve(size);
	for(size_t i=0; i<size; ++i) m_nodes.push_back(LeafNode(nodeProperties[i]));
}

ITKImageLeafLayer::ITKImageLeafLayer(const HounsfieldImagePointer& hounsfieldImage, const WindowedImagePointer& windowedImage)
{
	assert(hounsfieldImage->GetLargestPossibleRegion().GetSize() == windowedImage->GetLargestPossibleRegion().GetSize());

	const HounsfieldImage::SizeType& size = hounsfieldImage->GetLargestPossibleRegion().GetSize();
	m_sizeX = size[0];
	m_sizeY = size[1];
	m_sizeZ = size[2];
	m_sizeXY = m_sizeX * m_sizeY;
	m_sizeXYZ = m_sizeXY * m_sizeZ;

	m_nodes.reserve(m_sizeXYZ);
	for(int z=0; z<m_sizeZ; ++z)
		for(int y=0; y<m_sizeY; ++y)
			for(int x=0; x<m_sizeX; ++x)
			{
				WindowedImage::IndexType windowedIndex;
				windowedIndex[0] = x;
				windowedIndex[1] = y;
				windowedIndex[2] = z;
				HounsfieldImage::IndexType hounsfieldIndex;
				hounsfieldIndex[0] = x;
				hounsfieldIndex[1] = y;
				hounsfieldIndex[2] = z;
				m_nodes.push_back(LeafNode(NodeProperties(windowedImage->GetPixel(windowedIndex), hounsfieldImage->GetPixel(hounsfieldIndex))));
			}
}

//#################### PUBLIC METHODS ####################
std::vector<ITKImageLeafLayer::Edge> ITKImageLeafLayer::adjacent_edges(int n) const
{
	// Note: This is a 6-connected implementation.
	std::vector<Edge> ret;

	int x = x_of(n), y = y_of(n), z = z_of(n);
	if(z != 0)				ret.push_back(Edge(n - m_sizeXY, n, edge_weight(n - m_sizeXY, n)));
	if(y != 0)				ret.push_back(Edge(n - m_sizeX, n, edge_weight(n - m_sizeX, n)));
	if(x != 0)				ret.push_back(Edge(n - 1, n, edge_weight(n - 1, n)));
	if(x != m_sizeX - 1)	ret.push_back(Edge(n, n + 1, edge_weight(n, n + 1)));
	if(y != m_sizeY - 1)	ret.push_back(Edge(n, n + m_sizeX, edge_weight(n, n + m_sizeX)));
	if(z != m_sizeZ - 1)	ret.push_back(Edge(n, n + m_sizeXY, edge_weight(n, n + m_sizeXY)));

	return ret;
}

std::vector<int> ITKImageLeafLayer::adjacent_nodes(int n) const
{
	// Note: This is a 6-connected implementation.
	std::vector<int> ret;

	int x = x_of(n), y = y_of(n), z = z_of(n);
	if(z != 0)				ret.push_back(n - m_sizeXY);
	if(y != 0)				ret.push_back(n - m_sizeX);
	if(x != 0)				ret.push_back(n - 1);
	if(x != m_sizeX - 1)	ret.push_back(n + 1);
	if(y != m_sizeY - 1)	ret.push_back(n + m_sizeX);
	if(z != m_sizeZ - 1)	ret.push_back(n + m_sizeXY);

	return ret;
}

ITKRegionProperties ITKImageLeafLayer::combine_properties(const std::set<int>& nodeIndices) const
{
	std::vector<ITKPixelProperties> properties;
	properties.reserve(nodeIndices.size());
	for(std::set<int>::const_iterator it=nodeIndices.begin(), iend=nodeIndices.end(); it!=iend; ++it)
	{
		properties.push_back(node_properties(*it));
	}
	return ITKRegionProperties::combine_leaf_properties(properties);
}

// Precondition: has_edge(u, v)
ITKImageLeafLayer::EdgeWeight ITKImageLeafLayer::edge_weight(int u, int v) const
{
	return abs(m_nodes[u].properties().grey_value() - m_nodes[v].properties().grey_value());
}

ITKImageLeafLayer::EdgeConstIterator ITKImageLeafLayer::edges_cbegin() const
{
	return EdgeConstIterator(new EdgeConstIteratorImpl(this, 0));
}

ITKImageLeafLayer::EdgeConstIterator ITKImageLeafLayer::edges_cend() const
{
	return EdgeConstIterator(new EdgeConstIteratorImpl(this, m_sizeXYZ));
}

bool ITKImageLeafLayer::has_edge(int u, int v) const
{
	// Note: This is a 6-connected implementation.
	if(!has_node(u) || !has_node(v)) return false;
	int ux = x_of(u), uy = y_of(u), uz = z_of(u);
	int vx = x_of(v), vy = y_of(v), vz = z_of(v);
	int xdiff = abs(ux - vx), ydiff = abs(uy - vy), zdiff = abs(uz - vz);
	return xdiff + ydiff + zdiff == 1;	// note that each is either 0 or 1
}

bool ITKImageLeafLayer::has_node(int n) const
{
	return 0 <= n && n < static_cast<int>(m_nodes.size());
}

ITKImageLeafLayer::LeafNodeIterator ITKImageLeafLayer::leaf_nodes_begin()
{
	return LeafNodeIterator(new LeafNodeIteratorImpl(0, m_nodes));
}

ITKImageLeafLayer::LeafNodeConstIterator ITKImageLeafLayer::leaf_nodes_cbegin() const
{
	return LeafNodeConstIterator(new LeafNodeConstIteratorImpl(0, m_nodes));
}

ITKImageLeafLayer::LeafNodeConstIterator ITKImageLeafLayer::leaf_nodes_cend() const
{
	return LeafNodeConstIterator(new LeafNodeConstIteratorImpl(static_cast<int>(m_nodes.size()), m_nodes));
}

ITKImageLeafLayer::LeafNodeIterator ITKImageLeafLayer::leaf_nodes_end()
{
	return LeafNodeIterator(new LeafNodeIteratorImpl(static_cast<int>(m_nodes.size()), m_nodes));
}

std::vector<int> ITKImageLeafLayer::node_indices() const
{
	int size = static_cast<int>(m_nodes.size());
	std::vector<int> ret(size);
	for(int i=0; i<size; ++i)
	{
		ret[i] = i;
	}
	return ret;
}

// Precondition: n is in the right range
int ITKImageLeafLayer::node_parent(int n) const
{
	return m_nodes[n].parent();
}

// Precondition: n is in the right range
const ITKPixelProperties& ITKImageLeafLayer::node_properties(int n) const
{
	return m_nodes[n].properties();
}

ITKImageLeafLayer::NodeIterator ITKImageLeafLayer::nodes_begin()
{
	return NodeIterator(new NodeIteratorImpl(0, m_nodes));
}

ITKImageLeafLayer::NodeConstIterator ITKImageLeafLayer::nodes_cbegin() const
{
	return NodeConstIterator(new NodeConstIteratorImpl(0, m_nodes));
}

ITKImageLeafLayer::NodeConstIterator ITKImageLeafLayer::nodes_cend() const
{
	return NodeConstIterator(new NodeConstIteratorImpl(static_cast<int>(m_nodes.size()), m_nodes));
}

ITKImageLeafLayer::NodeIterator ITKImageLeafLayer::nodes_end()
{
	return NodeIterator(new NodeIteratorImpl(static_cast<int>(m_nodes.size()), m_nodes));
}

// Precondition: n is in the right range
void ITKImageLeafLayer::set_node_parent(int n, int parent)
{
	m_nodes[n].set_parent(parent);
}

//#################### PRIVATE METHODS ####################
int ITKImageLeafLayer::x_of(int n) const
{
	return n % m_sizeX;
}

int ITKImageLeafLayer::y_of(int n) const
{
	return (n / m_sizeX) % m_sizeY;
}

int ITKImageLeafLayer::z_of(int n) const
{
	return n / m_sizeXY;
}

}