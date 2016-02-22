#include "Vec3Edit.h"
#include <qvalidator.h>

static QValidator* createValidator(fhVec3Edit::Type type, QObject* parent)
{
	switch (type)
	{
	case fhVec3Edit::Size:
		return new QIntValidator( 0, 65535, parent );
	case fhVec3Edit::Position:
		//fall through
	case fhVec3Edit::Direction:
		return new QIntValidator( -65535, 65535, parent );
	}

	return nullptr;
}

fhVec3Edit::fhVec3Edit(Type type, bool labels) 
: QWidget() {

	const int width = 45;

	m_xedit = new QLineEdit(this);
	m_yedit = new QLineEdit(this);
	m_zedit = new QLineEdit(this);

	m_xedit->setValidator( createValidator( type, this ) );
	m_yedit->setValidator( createValidator( type, this ) );
	m_zedit->setValidator( createValidator( type, this ) );

	m_xedit->setMaximumWidth(width);
	m_yedit->setMaximumWidth(width);
	m_zedit->setMaximumWidth(width);
	
	auto layout = new QGridLayout;
	layout->setSpacing(2);
	layout->setMargin(0);
	
	if(labels) {
		layout->addWidget(new QLabel("X"), 0, 0, Qt::AlignHCenter);
		layout->addWidget(new QLabel("Y"), 0, 1, Qt::AlignHCenter);
		layout->addWidget(new QLabel("Z"), 0, 2, Qt::AlignHCenter);
		layout->addWidget( m_xedit, 1, 0 );
		layout->addWidget( m_yedit, 1, 1 );
		layout->addWidget( m_zedit, 1, 2 );
	} else {
		layout->addWidget( m_xedit, 0, 0 );
		layout->addWidget( m_yedit, 0, 1 );
		layout->addWidget( m_zedit, 0, 2 );
	}

	QObject::connect(m_xedit, &QLineEdit::textChanged, [&]() {this->valueChanged(this->get()); });
	QObject::connect(m_yedit, &QLineEdit::textChanged, [&]() {this->valueChanged(this->get()); });
	QObject::connect(m_zedit, &QLineEdit::textChanged, [&]() {this->valueChanged(this->get()); });

	this->setLayout(layout);	
}

fhVec3Edit::~fhVec3Edit() {
}

idVec3 fhVec3Edit::get() const {
	return idVec3(getX(), getY(), getZ());
}

void fhVec3Edit::set( idVec3 v ) {
	setX(v.x);
	setY(v.y);
	setZ(v.z);
}

float fhVec3Edit::getX() const {
	return m_xedit->text().toFloat();
}

float fhVec3Edit::getY() const {
	return m_yedit->text().toFloat();
}

float fhVec3Edit::getZ() const {
	return m_zedit->text().toFloat();
}

void fhVec3Edit::setX( float value ) {
	m_xedit->setText(QString::number(value));
}

void fhVec3Edit::setY( float value ) {
	m_yedit->setText(QString::number(value));
}

void fhVec3Edit::setZ( float value ) {
	m_zedit->setText(QString::number(value));
}
