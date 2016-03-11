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
	void setAdjustStepSize( float f );
	void setPrecision(int n);
	void setEnabled(bool enabled);

signals:
	void valueChanged( float value );

private:
	bool isOutOfSync() const;
	void setRange(float minimum, float maximum, int steps);

	QSlider* m_slider;
	fhNumEdit* m_numEdit;;
};