#include "NumEdit.h"
#include <qvalidator.h>

static const int maxDecimals = 3;
static const int defaultRepeatInterval = 20; //repeat every n milliseconds (lower means faster)

fhNumEdit::fhNumEdit(float from, float to, QWidget* parent) : QWidget(parent), m_step(1), m_precision(maxDecimals) {
	m_edit = new QLineEdit(this);
	m_down = new QPushButton("-", this);
	m_up = new QPushButton("+", this);
	m_validator = new QDoubleValidator(from, to, m_precision, this);
	m_validator->setNotation(QDoubleValidator::StandardNotation);	

	const QSize updownSize(12,12);	
	m_down->setMaximumSize(updownSize);
	m_up->setMaximumSize(updownSize);
	m_up->setAutoRepeat(true);
	m_down->setAutoRepeat(true);
	m_up->setAutoRepeatInterval(defaultRepeatInterval);
	m_down->setAutoRepeatInterval(defaultRepeatInterval);
	
	m_edit->setValidator(m_validator);	

	auto vlayout = new QVBoxLayout;
	vlayout->setSpacing(0);
	vlayout->setMargin(0);
	vlayout->addWidget(m_up);
	vlayout->addWidget(m_down);	

	auto hlayout = new QHBoxLayout;
	hlayout->setSpacing( 0 );
	hlayout->setMargin( 0 );
	hlayout->addWidget(m_edit);
	hlayout->addLayout(vlayout);

	this->setLayout(hlayout);
	this->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);

	QObject::connect(m_up, &QPushButton::clicked, [=](){ float f = this->getFloat(); this->setFloat(f+m_step); });
	QObject::connect(m_down, &QPushButton::clicked, [=](){ float f = this->getFloat(); this->setFloat(f-m_step); });
	QObject::connect(m_edit, &QLineEdit::textChanged, [=](){this->valueChanged(this->getFloat());} );
}

fhNumEdit::fhNumEdit(QWidget* parent) 
: fhNumEdit(-100000, 100000, parent) {
}

fhNumEdit::~fhNumEdit() {
}

void fhNumEdit::setAutoRepeatInterval( int intervalMs ) {
	m_up->setAutoRepeatInterval( intervalMs );
	m_down->setAutoRepeatInterval( intervalMs );
}

void fhNumEdit::setAdjustStepSize( float step ) {
	m_step = step;
}

float fhNumEdit::getFloat() const {
	return m_edit->text().toFloat();
}

int fhNumEdit::getInt() const {
	return m_edit->text().toInt();
}

void fhNumEdit::setInt( int v ) {
	setFloat(v);
}

void fhNumEdit::setFloat( float v ) {
	if ( v > m_validator->top() )
		v = m_validator->top();

	if (v < m_validator->bottom())
		v = m_validator->bottom();

	m_edit->setText( QString::number( v, 'f', m_precision ) );
}

QSize fhNumEdit::sizeHint() const {	
	return QSize(60, m_edit->sizeHint().height());
}

void fhNumEdit::setMaximumValue(float f) {
	m_validator->setRange(m_validator->bottom(), f, m_precision);
	m_edit->setValidator(m_validator);	
}

void fhNumEdit::setMinimumValue( float f ) {
	m_validator->setRange( f, m_validator->top(), m_precision );
	m_edit->setValidator(m_validator);	
}

void fhNumEdit::setPrecision(int p) {
	m_precision = p;
	m_validator->setRange( m_validator->bottom(), m_validator->top(), m_precision );
	m_edit->setValidator( m_validator );
}

float fhNumEdit::maximumValue() const {
	return static_cast<float>(m_validator->top());
}

float fhNumEdit::minimumValue() const {
	return static_cast<float>(m_validator->bottom());
}