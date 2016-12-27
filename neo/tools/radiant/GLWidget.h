/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company. 

This file is part of the Doom 3 GPL Source Code (?Doom 3 Source Code?).  

Doom 3 Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Doom 3 Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Doom 3 Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/
#if !defined(AFX_IDGLWIDGET_H__6399A341_2976_4A6E_87DD_9AF4DBD4C5DB__INCLUDED_)
#define AFX_IDGLWIDGET_H__6399A341_2976_4A6E_87DD_9AF4DBD4C5DB__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

/////////////////////////////////////////////////////////////////////////////
// idGLWidget window

enum class MouseButton {
	Left,
	Right,
	Middle
};

class idGLDrawable {
public:
	idGLDrawable();
	~idGLDrawable() {};
	virtual void draw(int x, int y, int w, int h);
	virtual void setMedia(const char *name){}
	virtual void buttonDown(int button, float x, float y);
	virtual void buttonUp(int button, float x, float y);
	virtual void mouseMove(float x, float y);
	virtual int getRealTime() {return realTime;};
	virtual bool ScreenCoords() { 
		return true;
	}
	void SetRealTime(int i) {
		realTime = i;
	}
	virtual void Update() {};
	float getScale() {
		return scale;
	}
	void setScale(float f) {
		scale = f;
	}
protected:
	float scale;
	float xOffset;
	float yOffset;
	float zOffset;
	float pressX;
	float pressY;
	bool  handleMove;
	int button;
	int realTime;
};

class idGLDrawableWorld : public idGLDrawable {
public:
	idGLDrawableWorld();
	~idGLDrawableWorld();
	void AddTris(srfTriangles_t *tris, const idMaterial *mat);
	virtual void draw(int x, int y, int w, int h) override;
	virtual void mouseMove( float x, float y ) override;
	virtual void Update() override { worldDirty = true; };
	virtual void buttonDown( int button, float x, float y ) override;
	void InitWorld();
protected:
	
	void InitLight(const idVec3& position);

	idRenderWorld *world;
	idRenderModel *worldModel;
	qhandle_t	worldModelDef;
	qhandle_t	lightDef;
	qhandle_t   modelDef;
	float       light;

//model
	bool worldDirty;
	idStr skinStr;
	idQuat rotation;
	idVec3 lastPress;
	float radius;
	idVec4 rect;
};

class idGLDrawableMaterial : public idGLDrawableWorld {
public:
	idGLDrawableMaterial(const idMaterial *mat) {
		material = mat;
		scale = 2.0;
		light = 1.0;
		worldDirty = true;
	}

	idGLDrawableMaterial() {
		material = NULL;
		light = 1.0;
		worldDirty = true;
		realTime = 50;
	}
	
	virtual void setMedia(const char *name) override;
	virtual void draw(int x, int y, int w, int h) override;

//protected:
	const idMaterial *material;
};

class idGLDrawableModel : public idGLDrawableWorld {
public:
	idGLDrawableModel(const char *name);
	idGLDrawableModel();

	virtual void setMedia(const char *name) override;

	virtual void draw(int x, int y, int w, int h) override;
	virtual bool ScreenCoords() override { 
		return false;
	}
	void SetSkin( const char *skin );
};

class idGLWidget : public CWnd
{
// Construction
public:
	idGLWidget();
	void setDrawable(idGLDrawable *d);

// Attributes
public:

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(idGLWidget)
	public:
	virtual BOOL Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext = NULL);
	protected:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~idGLWidget();

	// Generated message map functions
protected:
	idGLDrawable *drawable;
	bool initialized;
	//{{AFX_MSG(idGLWidget)
	afx_msg void OnPaint();
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnMButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnTimer(UINT nIDEvent);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_IDGLWIDGET_H__6399A341_2976_4A6E_87DD_9AF4DBD4C5DB__INCLUDED_)
