#include "SliderNumEdit.h"
#include "NumEdit.h"
#include <QSlider>



fhSliderNumEdit::fhSliderNumEdit( QWidget* parent )
: QWidget( parent ) {
	auto layout = new QHBoxLayout( this );
	m_slider = new QSlider( this );
	m_numEdit = new fhNumEdit( this );

	layout->addWidget( m_slider );
	layout->addWidget( m_numEdit );
	this->setLayout( layout );

	QObject::connect(m_slider, &QSlider::valueChanged, [=](int value){
		const float minimum = this->m_numEdit->minimumValue();
		const float maximum = this->m_numEdit->maximumValue();
		const int steps = this->m_slider->maximum();

		const float v = minimum + (maximum - minimum)/steps * value;

		if(qAbs(this->m_numEdit->getFloat() - v) > 0.001)
			this->m_numEdit->setFloat(v);
	});

	QObject::connect(m_numEdit, &fhNumEdit::valueChanged, [=](){
		const float minimum = this->m_numEdit->minimumValue();
		const float maximum = this->m_numEdit->maximumValue();
		const int steps = this->m_slider->maximum();

		const int value = static_cast<int>((maximum - minimum)/steps);
		if(this->m_slider->value() != value)
			this->m_slider->setValue(value);
	});
}

fhSliderNumEdit::~fhSliderNumEdit() {
}

void fhSliderNumEdit::setValue( float v ) {
	m_numEdit->setFloat(v);
}

float fhSliderNumEdit::value() const {
	return m_numEdit->getFloat();
}

void fhSliderNumEdit::setMaximum( float maximum ) {
	
}

void fhSliderNumEdit::setMinimum( float minimum ) {
	
}

void fhSliderNumEdit::setStepNum( int num ) {

}

void fhSliderNumEdit::setRange(float minimum, float maximum, int steps) {
	m_numEdit->setMaximumValue(maximum);
	m_numEdit->setMinimumValue(minimum);
	m_slider->setMinimum(0);
	m_slider->setMaximum(steps);
}