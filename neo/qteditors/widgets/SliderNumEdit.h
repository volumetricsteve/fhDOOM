#pragma once
#include "../precompiled.h"

class fhNumEdit;
class QSlider;

class fhSliderNumEdit : public QWidget
{
	Q_OBJECT
public:
	explicit fhSliderNumEdit( QWidget* parent );
	virtual ~fhSliderNumEdit();

	void setValue( float v );
	float value() const;

	void setMaximum( float maximum );
	void setMinimum( float minimum );
	void setStepNum( int num );

signals:
	void valueChanged( float value );

private:
	void setRange(float minimum, float maximum, int steps);

	QSlider* m_slider;
	fhNumEdit* m_numEdit;;
};