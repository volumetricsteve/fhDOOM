#pragma once
#include "../precompiled.h"


class fhNumEdit;

class fhVec3Edit : public QWidget {
	Q_OBJECT

public:
	enum Type
	{
		Size,
		Position,
		Direction
	};

	explicit fhVec3Edit(Type type, bool labels = false);
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

signals:
	void valueChanged(idVec3 v);

private:
	fhNumEdit* m_xedit;
	fhNumEdit* m_yedit;
	fhNumEdit* m_zedit;	
};