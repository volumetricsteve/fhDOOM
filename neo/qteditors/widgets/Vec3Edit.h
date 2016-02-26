#pragma once
#include "../precompiled.h"


class fhNumEdit;

class fhVec3Edit : public QWidget {
	Q_OBJECT

public:
	explicit fhVec3Edit(bool labels, QWidget* parent);
	explicit fhVec3Edit(QWidget* parent);
	~fhVec3Edit();

	idVec3 get() const;
	void set(idVec3 v);

	float getX() const;
	float getY() const;
	float getZ() const;

	void setX(float value);
	void setY(float value);
	void setZ(float value);

	virtual QSize sizeHint() const override;

	void setMinimumValue(idVec3 v);
	void setMaximumValue(idVec3 v);
	void setStepSize(idVec3 v);
	void setStepSize(float f) {
		setStepSize(idVec3(f,f,f));
	}

	void setPrecision(int x, int y, int z);
	void setPrecision(int p) {
		setPrecision(p,p,p);
	}	

signals:
	void valueChanged(idVec3 v);

private:
	fhNumEdit* m_xedit;
	fhNumEdit* m_yedit;
	fhNumEdit* m_zedit;	
};