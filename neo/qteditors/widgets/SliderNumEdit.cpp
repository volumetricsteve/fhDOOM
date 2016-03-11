#include "SliderNumEdit.h"
#include "NumEdit.h"
#include <QSlider>



fhSliderNumEdit::fhSliderNumEdit( QWidget* parent )
: QWidget( parent ) {
	auto layout = new QHBoxLayout( this );
	m_slider = new QSlider(Qt::Horizontal, this );
	m_numEdit = new fhNumEdit( this );	
	m_numEdit->setMaximumWidth(50);

	layout->addWidget( m_slider );
	layout->addWidget( m_numEdit );
	this->setLayout( layout );

	m_slider->setMaximum(1000);

	QObject::connect(m_slider, &QSlider::valueChanged, [=](int value){
		if(this->isOutOfSync()) {
			const float rel = static_cast<float>(m_slider->value()) / static_cast<float>(m_slider->maximum());
			m_numEdit->setFloat( rel * (m_numEdit->maximumValue() - m_numEdit->minimumValue()) + m_numEdit->minimumValue() );		

			emit valueChanged( m_numEdit->getFloat() );
		}
	});

	QObject::connect(m_numEdit, &fhNumEdit::valueChanged, [=](){
		if (this->isOutOfSync()) {
			const float rel = (m_numEdit->getFloat() - m_numEdit->minimumValue()) / (m_numEdit->maximumValue() - m_numEdit->minimumValue());
			m_slider->setValue( static_cast<int>(rel * m_slider->maximum()) );

			emit valueChanged( m_numEdit->getFloat() );
		}
	});

	setSizePolicy( QSizePolicy::MinimumExpanding, QSizePolicy::Fixed );
	setContentsMargins( 0, 0, 0, 0 );
	layout->setMargin( 0 );
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
	m_numEdit->setMaximumValue(maximum);
}

void fhSliderNumEdit::setMinimum( float minimum ) {
	m_numEdit->setMinimumValue(minimum);
}

void fhSliderNumEdit::setAdjustStepSize( float f ) {
	m_numEdit->setAdjustStepSize(f);
}

void fhSliderNumEdit::setPrecision(int n) {
	m_numEdit->setPrecision(n);
}

void fhSliderNumEdit::setEnabled(bool enabled) {
	QWidget::setEnabled(enabled);
	m_slider->setEnabled(enabled);
	m_numEdit->setEnabled(enabled);
}

void fhSliderNumEdit::setRange(float minimum, float maximum, int steps) {
	m_numEdit->setMaximumValue(maximum);
	m_numEdit->setMinimumValue(minimum);
	m_slider->setMinimum(0);
	m_slider->setMaximum(steps);
}

bool fhSliderNumEdit::isOutOfSync() const {
	const float currentNumEditValue = m_numEdit->getFloat();
	const float currentSliderValue = ((float)m_slider->value() / (float)m_slider->maximum()) * m_numEdit->maximumValue();

	if (qAbs( currentNumEditValue - currentSliderValue ) > 0.00001)
		return true;

	return false;
}