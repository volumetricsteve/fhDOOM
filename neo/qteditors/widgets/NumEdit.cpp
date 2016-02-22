#include "NumEdit.h"
#include <qvalidator.h>

fhNumEdit::fhNumEdit(int from, int to, QWidget* parent) : QWidget(parent) {
	m_edit = new QLineEdit(this);
	m_down = new QPushButton("-", this);
	m_up = new QPushButton("+", this);

	const QSize updownSize(8,10);
	const int repeatInterval = 20; //repeat every n milliseconds (lower means faster)
	m_down->setMaximumSize(updownSize);
	m_up->setMaximumSize(updownSize);
	m_up->setAutoRepeat(true);
	m_down->setAutoRepeat(true);
	m_up->setAutoRepeatInterval(repeatInterval);
	m_down->setAutoRepeatInterval(repeatInterval);

	m_edit->setValidator(new QIntValidator(from, to, this));	

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

	QObject::connect(m_up, &QPushButton::clicked, [=](){ int i = this->getInt(); this->setInt(i+1); });
	QObject::connect(m_down, &QPushButton::clicked, [=](){ int i = this->getInt(); this->setInt(i-1); });
	QObject::connect(m_edit, &QLineEdit::textChanged, [=](){this->valueChanged(this->getInt());} );
}

fhNumEdit::~fhNumEdit() {
}

float fhNumEdit::getFloat() const {
	return m_edit->text().toFloat();
}

int fhNumEdit::getInt() const {
	return m_edit->text().toInt();
}

void fhNumEdit::setInt( int v ) {
	m_edit->setText(QString::number(v));
}


void fhNumEdit::setFloat( float v ) {
	m_edit->setText( QString::number( v ) );
}

QSize fhNumEdit::sizeHint() const {	
	return QSize(25, m_edit->sizeHint().height());
}