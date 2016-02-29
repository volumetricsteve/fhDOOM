#pragma once
#include "../precompiled.h"

class fhVec3Edit;
class QPushButton;

class fhColorEdit : public QWidget
{
	Q_OBJECT
public:
	explicit fhColorEdit(QWidget* parent);
	virtual ~fhColorEdit();

	void set(idVec3 color);
	idVec3 get() const;

signals:
	void valueChanged(idVec3 value);

private:
	QPushButton* m_button;
	fhVec3Edit* m_vec3edit;
};