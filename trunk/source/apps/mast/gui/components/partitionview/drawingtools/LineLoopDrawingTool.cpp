/***
 * millipede: LineLoopDrawingTool.cpp
 * Copyright Stuart Golodetz, 2010. All rights reserved.
 ***/

#include "LineLoopDrawingTool.h"

#include <common/ogl/WrappedGL.h>

namespace mp {

//#################### PUBLIC METHODS ####################
void LineLoopDrawingTool::mouse_pressed(const Vector2i& p)
{
	m_drawnPixels.push_back(p);
}

DrawingTool::ToolStyle LineLoopDrawingTool::style() const
{
	return TOOLSTYLE_MULTICLICK;
}

//#################### PRIVATE METHODS ####################
void LineLoopDrawingTool::render_sub() const
{
	glPushAttrib(GL_POINT_BIT);

	glPointSize(5.0f);
	glEnable(GL_POINT_SMOOTH);

	glColor3d(0.0, 1.0, 1.0);
	glBegin(GL_POINTS);
		for(std::list<Vector2i>::const_iterator it=m_drawnPixels.begin(), iend=m_drawnPixels.end(); it!=iend; ++it)
		{
			glVertex2i(it->x, it->y);
		}
	glEnd();

	glPopAttrib();
}

}
