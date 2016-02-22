#pragma once
#include "../precompiled.h"

class fhNumEdit : public QWidget {
	Q_OBJECT

public:	
	explicit fhNumEdit(int from, int to, QWidget* parent = nullptr);
	~fhNumEdit();

	int getInt() const;
	float getFloat() const;
	void setInt( int v );
	void setFloat( float v );

	virtual QSize sizeHint() const override;

signals:
	void valueChanged( int v );

private:
	QPushButton* m_up;
	QPushButton* m_down;
	QLineEdit* m_edit;
};