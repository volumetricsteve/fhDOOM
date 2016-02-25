#pragma once

class idGLDrawable;
class fhRenderWindow;

class fhRenderWidget : public QWidget {
	Q_OBJECT
public:
	fhRenderWidget(QWidget* parent);
	~fhRenderWidget();

	void setDrawable(idGLDrawable* d) {
		m_drawable = d;
	}

	void updateDrawable();

private:
	fhRenderWindow* m_window;
	idGLDrawable* m_drawable;
};
