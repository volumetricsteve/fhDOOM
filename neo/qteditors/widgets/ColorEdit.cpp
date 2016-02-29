#include "ColorEdit.h"
#include "Vec3Edit.h"
#include <qcolordialog.h>

static idVec3 QColorToVec3(const QColor& color) {
	QRgb rgb = color.rgb();
	idVec3 ret;
	ret.x = qRed( rgb ) / 255.0f;
	ret.y = qGreen( rgb ) / 255.0f;
	ret.z = qBlue( rgb ) / 255.0f;
	return ret;
}

static QColor Vec3ToQColor(idVec3 v) {
	QColor c;
	c.setRed( static_cast<int>(v.x * 255) );
	c.setGreen( static_cast<int>(v.y * 255) );
	c.setBlue( static_cast<int>(v.z * 255) );	
	return c;
}

static void setButtonColor(QPushButton* button, const QColor& color) {
	QString s( "background: #"
		+ QString( color.red() < 16 ? "0" : "" ) + QString::number( color.red(), 16 )
		+ QString( color.green() < 16 ? "0" : "" ) + QString::number( color.green(), 16 )
		+ QString( color.blue() < 16 ? "0" : "" ) + QString::number( color.blue(), 16 ) + ";" );
	button->setStyleSheet( s );
	button->update();
}

fhColorEdit::fhColorEdit(QWidget* parent)
: QWidget(parent) {
	auto layout = new QHBoxLayout(this);
	m_button = new QPushButton(this);
	m_vec3edit = new fhVec3Edit(this);

	layout->addWidget(m_button);
	layout->addWidget(m_vec3edit);
	this->setLayout(layout);

	m_vec3edit->setMinimumValue(idVec3(0,0,0));
	m_vec3edit->setMaximumValue(idVec3(1,1,1));
	m_vec3edit->setPrecision(2);
	m_vec3edit->setStepSize(0.01f);
	m_vec3edit->setMaximumWidth(140);

	QObject::connect( m_button, &QPushButton::clicked, [=](){
		QColorDialog dialog( Vec3ToQColor(this->m_vec3edit->get()), this );

		dialog.setWindowModality( Qt::WindowModal );
		QObject::connect( &dialog, &QColorDialog::currentColorChanged, [&](const QColor& c){
			setButtonColor(this->m_button, c);
			this->m_vec3edit->set(QColorToVec3(c));
		} );

		dialog.exec();
		this->m_vec3edit->set(QColorToVec3(dialog.currentColor()));
	} );

	QObject::connect( m_vec3edit, &fhVec3Edit::valueChanged, [=](idVec3 value){
		setButtonColor(this->m_button, Vec3ToQColor(value));
		emit valueChanged(value);
	});

	m_vec3edit->set(idVec3(1,1,1));
	this->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
	this->setContentsMargins(0,0,0,0);
	layout->setMargin(0);
	//layout->setSpacing(0);
}

fhColorEdit::~fhColorEdit() {
}

void fhColorEdit::set( idVec3 color ) {
	m_vec3edit->set(color);
}

idVec3 fhColorEdit::get() const {
	return m_vec3edit->get();
}