#include "Vec3Edit.h"
#include "NumEdit.h"

fhVec3Edit::fhVec3Edit(Type type, bool labels) 
: QWidget() {

	int from = INT_MIN;
	int to = INT_MAX;
	
	if(type == fhVec3Edit::Size)
		from = 0;

	m_xedit = new fhNumEdit(from, to, this);
	m_yedit = new fhNumEdit(from, to, this);
	m_zedit = new fhNumEdit(from, to, this);
	
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

	QObject::connect(m_xedit, &fhNumEdit::valueChanged, [&]() {this->valueChanged(this->get()); });
	QObject::connect(m_yedit, &fhNumEdit::valueChanged, [&]() {this->valueChanged(this->get()); });
	QObject::connect(m_zedit, &fhNumEdit::valueChanged, [&]() {this->valueChanged(this->get()); });

	this->setLayout(layout);	
	this->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
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
	return m_xedit->getFloat();
}

float fhVec3Edit::getY() const {
	return m_yedit->getFloat();
}

float fhVec3Edit::getZ() const {
	return m_zedit->getFloat();
}

void fhVec3Edit::setX( float value ) {
	m_xedit->setFloat(value);
}

void fhVec3Edit::setY( float value ) {
	m_yedit->setFloat(value);
}

void fhVec3Edit::setZ( float value ) {
	m_zedit->setFloat(value);
}

QSize fhVec3Edit::sizeHint() const {
	QSize size = QSize(45, m_xedit->sizeHint().height());
	return size;
}