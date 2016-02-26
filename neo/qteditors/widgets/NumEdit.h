#pragma once
#include "../precompiled.h"

class QDoubleValidator;
class fhNumEdit : public QWidget {
	Q_OBJECT

public:	
	explicit fhNumEdit(QWidget* parent = nullptr);
	explicit fhNumEdit(float from, float to, QWidget* parent = nullptr);
	~fhNumEdit();

	int getInt() const;
	float getFloat() const;
	void setInt( int v );
	void setFloat( float v );

	virtual QSize sizeHint() const override;

	void setAutoRepeatInterval(int intervalMs);
	void setAdjustStepSize(float step);
	void setMinimumValue(float f);
	void setMaximumValue(float f);
	void setPrecision(int p);

signals:
	void valueChanged( int v );

private:
	QDoubleValidator* m_validator;
	QPushButton* m_up;
	QPushButton* m_down;
	QLineEdit* m_edit;
	float m_step;
	int m_precision;
};
