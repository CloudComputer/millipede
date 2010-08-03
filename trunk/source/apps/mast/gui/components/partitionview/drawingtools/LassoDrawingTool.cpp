/***
 * millipede: LassoDrawingTool.cpp
 * Copyright Stuart Golodetz, 2010. All rights reserved.
 ***/

#include "LassoDrawingTool.h"

#include <boost/utility.hpp>

#include <common/ogl/WrappedGL.h>

namespace mp {

//#################### PUBLIC METHODS ####################
bool LassoDrawingTool::has_started() const
{
	return !m_drawnPixels.empty();
}

void LassoDrawingTool::mouse_dragged(const Vector2i& p)
{
	m_drawnPixels.push_back(p);
}

void LassoDrawingTool::mouse_pressed(const Vector2i& p)
{
	m_drawnPixels.push_back(p);
}

void LassoDrawingTool::render() const
{
	glColor3d(1.0, 1.0, 1.0);
	glBegin(GL_LINE_STRIP);
		for(std::list<Vector2i>::const_iterator it=m_drawnPixels.begin(), iend=m_drawnPixels.end(); it!=iend; ++it)
		{
			glVertex2i(it->x, it->y);
		}
	glEnd();

	if(m_drawnPixels.size() >= 2)
	{
		const Vector2i& first = m_drawnPixels.front(), last = m_drawnPixels.back();
		glColor3d(1.0, 0.0, 1.0);
		glBegin(GL_LINES);
			glVertex2i(first.x, first.y);
			glVertex2i(last.x, last.y);
		glEnd();
	}
}

void LassoDrawingTool::reset()
{
	m_drawnPixels.clear();
}

std::vector<Vector2i> LassoDrawingTool::selected_pixels() const
{
	// NYI
	return std::vector<Vector2i>();
}

}