#include "LightEditor.h"
#include <qlayout.h>
#include <qgroupbox.h>
#include <qcheckbox.h>
#include <qcombobox.h>
#include <qcolordialog.h>
#include <qmessagebox.h>
#include <QCloseEvent>

#include "../tools/radiant/QE3.H"
#include "../tools/radiant/GLWidget.h"

fhLightEditor::fhLightEditor(QWidget* parent)
: QDialog(parent) {

	QVBoxLayout* mainLayout = new QVBoxLayout;
	QHBoxLayout* leftRightLayout = new QHBoxLayout;

	QWidget* left = new QWidget;
	QVBoxLayout* leftLayout = new QVBoxLayout;
	left->setLayout(leftLayout);
	leftRightLayout->addWidget(left);

	m_lighttype = new QComboBox;
	m_lighttype->addItem("Point Light");
	m_lighttype->addItem("Parallel Light");
	m_lighttype->addItem("Projected Light");

	leftLayout->addWidget(m_lighttype);
	leftLayout->addWidget(CreatePointLightParameters());
	leftLayout->addWidget(CreateParallelLightParameters());
	leftLayout->addWidget(CreateProjectedLightParameters());

	QWidget* right = new QWidget;
	QVBoxLayout* rightLayout = new QVBoxLayout;
	right->setLayout( rightLayout );
	leftRightLayout->addWidget( right );
	
	m_castShadows = new QCheckBox("Cast Shadows");	
	rightLayout->addWidget(m_castShadows);	

	m_colorButton = new QPushButton(this);
	auto colorLayout = new QHBoxLayout;
	colorLayout->addWidget(new QLabel("Color"));
	colorLayout->addWidget(m_colorButton);
	rightLayout->addLayout(colorLayout);

	mainLayout->addLayout(leftRightLayout);

	QHBoxLayout* buttonLayout = new QHBoxLayout;
	m_cancelButton = new QPushButton("Cancel", this);
	m_applyButton = new QPushButton("Save", this);
	m_okButton = new QPushButton("OK", this);	

	buttonLayout->addWidget(m_cancelButton);
	buttonLayout->addItem(new QSpacerItem(10, 10, QSizePolicy::Expanding, QSizePolicy::Minimum));
	buttonLayout->addWidget(m_applyButton);
	buttonLayout->addWidget(m_okButton);
	mainLayout->addLayout(buttonLayout);

	m_material = new QComboBox(this);
	auto materialLayout = new QHBoxLayout;
	materialLayout->addWidget( new QLabel( "Material" ) );
	materialLayout->addWidget( m_material );
	rightLayout->addLayout( materialLayout );
	LoadMaterials();	

	m_drawableMaterial = new idGLDrawableMaterial();
	fhRenderWidget* renderWidget = new fhRenderWidget(this);
	renderWidget->setDrawable(m_drawableMaterial);		
	rightLayout->addWidget(renderWidget);

	this->setLayout(mainLayout);

	QObject::connect(m_lighttype, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), [=](){
		m_currentData.type = static_cast<fhLightType>(m_lighttype->currentIndex());
		m_modified = true;
		this->UpdateGame();
		UpdateLightParameters();
	});

	QObject::connect(m_colorButton, &QPushButton::clicked, [=](){ 
		QColor color;
		color.setRgbF( m_currentData.color.x, m_currentData.color.y, m_currentData.color.z );

		QColorDialog dialog(color, this);
		dialog.setWindowModality(Qt::WindowModal);
		QObject::connect(&dialog, &QColorDialog::currentColorChanged, [&](){
			this->setLightColor(dialog.currentColor());	
			this->UpdateGame();
		});

		dialog.exec();
	});

	QObject::connect(m_pointlightParameters.radius, &fhVec3Edit::valueChanged, [=](idVec3 v){
		this->m_currentData.radius = v;
		this->m_modified = true;
		this->UpdateGame();
	} );

	QObject::connect( m_pointlightParameters.center, &fhVec3Edit::valueChanged, [=]( idVec3 v ){
		this->m_currentData.center = v;
		this->m_modified = true;
		this->UpdateGame();
	} );

	QObject::connect( m_parallellightParameters.radius, &fhVec3Edit::valueChanged, [=]( idVec3 v ){
		this->m_currentData.radius = v;
		this->m_modified = true;
		this->UpdateGame();
	} );

	QObject::connect( m_parallellightParameters.direction, &fhVec3Edit::valueChanged, [=]( idVec3 v ){
		this->m_currentData.center = v;
		this->m_modified = true;
		this->UpdateGame();
	} );

	QObject::connect( m_projectedlightParameters.target, &fhVec3Edit::valueChanged, [=]( idVec3 v ){
		this->m_currentData.target = v;
		this->m_modified = true;
		this->UpdateGame();
	} );

	QObject::connect( m_projectedlightParameters.right, &fhVec3Edit::valueChanged, [=]( idVec3 v ){
		this->m_currentData.right = v;
		this->m_modified = true;
		this->UpdateGame();
	} );

	QObject::connect( m_projectedlightParameters.up, &fhVec3Edit::valueChanged, [=]( idVec3 v ){
		this->m_currentData.up = v;
		this->m_modified = true;
		this->UpdateGame();
	} );

	QObject::connect( m_projectedlightParameters.start, &fhVec3Edit::valueChanged, [=]( idVec3 v ){
		this->m_currentData.start = v;
		this->m_modified = true;
		this->UpdateGame();
	} );

	QObject::connect( m_projectedlightParameters.end, &fhVec3Edit::valueChanged, [=]( idVec3 v ){
		this->m_currentData.end = v;
		this->m_modified = true;
		this->UpdateGame();
	} );

	QObject::connect( m_castShadows, &QCheckBox::stateChanged, [=](){
		this->m_currentData.castShadows = this->m_castShadows->isChecked();
		this->m_modified = true;
		this->UpdateGame();
	} );

	QObject::connect( m_material, &QComboBox::currentTextChanged, [=](QString text){
		QByteArray ascii = text.toLocal8Bit();
		this->m_currentData.material = ascii.data();
		this->m_modified = true;
		this->m_drawableMaterial->setMedia(ascii.data());
		renderWidget->updateDrawable();
		this->UpdateGame();
	} );

	QObject::connect( m_applyButton, &QPushButton::clicked, [=](){
		this->UpdateGame();
		this->m_originalData = this->m_currentData;
		this->m_modified = false;
	});


	QObject::connect( m_okButton, &QPushButton::clicked, [=](){
		this->UpdateGame();
		this->m_originalData = this->m_currentData;
		this->m_modified = false;
		this->close();
	} );

	QObject::connect( m_cancelButton, &QPushButton::clicked, [=](){
		this->m_currentData = this->m_originalData;
		this->m_modified = false;
		this->UpdateGame();		
		this->close();
	} );

	m_timer.setInterval(33);
	m_timer.start();

	QObject::connect(&m_timer, &QTimer::timeout, [=](){
		if(this->isVisible())
			renderWidget->updateDrawable();
	});

	UpdateLightParameters();
}

fhLightEditor::~fhLightEditor() {
}

void fhLightEditor::closeEvent(QCloseEvent* event) {
	if(m_modified) {
		QMessageBox::StandardButton resBtn = QMessageBox::question( this, "Light Editor",
			tr( "You have unsaved changes." ),
			QMessageBox::Cancel | QMessageBox::Save | QMessageBox::Discard );

		if (resBtn == QMessageBox::Save) {
			this->UpdateGame();
			this->m_originalData = this->m_currentData;
			this->m_modified = false;			
			event->accept();
			return;
		}

		if (resBtn == QMessageBox::Discard) {
			this->m_currentData = this->m_originalData;
			this->m_modified = false;
			this->UpdateGame();
			event->accept();
			return;
		}
		
		event->ignore();
		return;		
	}

	event->accept();
}

QWidget* fhLightEditor::CreatePointLightParameters() {		

	QGroupBox* pointlightGroup = new QGroupBox( tr( "Point Light Parameters" ) );;
	QGridLayout* pointlightgrid = new QGridLayout;	

	m_pointlightParameters.radius = new fhVec3Edit(fhVec3Edit::Size, true);
	m_pointlightParameters.center = new fhVec3Edit(fhVec3Edit::Position);

	pointlightgrid->addWidget( new QLabel( "Radius" ),        0, 0, Qt::AlignBottom );
	pointlightgrid->addWidget( m_pointlightParameters.radius, 0, 1 );

	pointlightgrid->addWidget( new QLabel( "Center" ),        1, 0, Qt::AlignBottom );
	pointlightgrid->addWidget( m_pointlightParameters.center, 1, 1 );

	pointlightGroup->setLayout( pointlightgrid );

	return pointlightGroup;
}

QWidget* fhLightEditor::CreateParallelLightParameters() {

	QGroupBox* group = new QGroupBox( tr( "Parallel Light Parameters" ) );;
	QGridLayout* grid = new QGridLayout;	

	m_parallellightParameters.radius = new fhVec3Edit(fhVec3Edit::Size, true);
	m_parallellightParameters.direction = new fhVec3Edit(fhVec3Edit::Direction);

	grid->addWidget( new QLabel( "Radius" ), 0, 0, Qt::AlignBottom );
	grid->addWidget( m_parallellightParameters.radius, 0, 1 );

	grid->addWidget( new QLabel( "Center" ), 1, 0, Qt::AlignBottom );
	grid->addWidget( m_parallellightParameters.direction, 1, 1 );	

	group->setLayout( grid );

	return group;
}

QWidget* fhLightEditor::CreateProjectedLightParameters() {

	QGroupBox* group = new QGroupBox( tr( "Projected Light Parameters" ) );;
	QGridLayout* grid = new QGridLayout;	

	m_projectedlightParameters.target = new fhVec3Edit(fhVec3Edit::Direction, true);
	m_projectedlightParameters.right = new fhVec3Edit(fhVec3Edit::Direction);
	m_projectedlightParameters.up = new fhVec3Edit(fhVec3Edit::Direction);
	m_projectedlightParameters.start = new fhVec3Edit(fhVec3Edit::Direction, true);
	m_projectedlightParameters.end = new fhVec3Edit(fhVec3Edit::Direction);

	grid->addWidget( new QLabel( "Target" ), 0, 0, Qt::AlignBottom );
	grid->addWidget( m_projectedlightParameters.target, 0, 1 );

	grid->addWidget( new QLabel( "Right" ), 1, 0, Qt::AlignBottom );
	grid->addWidget( m_projectedlightParameters.right, 1, 1 );

	grid->addWidget( new QLabel( "Up" ), 2, 0, Qt::AlignBottom );
	grid->addWidget( m_projectedlightParameters.up, 2, 1 );

	m_projectedlightParameters.explicitStartEnd = new QCheckBox;
	m_projectedlightParameters.explicitStartEnd->setText("Explicit Start/End");
	grid->addWidget(m_projectedlightParameters.explicitStartEnd, 3, 0, 1, 2);

	grid->addWidget( new QLabel( "Start" ), 4, 0, Qt::AlignBottom );
	grid->addWidget( m_projectedlightParameters.start, 4, 1 );

	grid->addWidget( new QLabel( "End" ), 5, 0, Qt::AlignBottom );
	grid->addWidget( m_projectedlightParameters.end, 5, 1 );

	group->setLayout( grid );

	QObject::connect(m_projectedlightParameters.explicitStartEnd, &QCheckBox::stateChanged, [=](){
		this->m_currentData.explicitStartEnd = this->m_projectedlightParameters.explicitStartEnd->isChecked();
		if(this->m_currentData.explicitStartEnd) {
			this->m_currentData.end = this->m_currentData.target;
			this->m_currentData.start = this->m_currentData.target.Normalized() * 8;
		}
		this->m_modified = true;
		this->UpdateLightParameters();
	} );

	return group;
}


void fhLightEditor::UpdateLightParameters() {
	const bool pointlight = (m_lighttype->currentIndex() == 0);
	const bool parallellight = (m_lighttype->currentIndex() == 1);
	const bool projectedlight = (m_lighttype->currentIndex() == 2);
	const bool explicitStartEnd = projectedlight && m_currentData.explicitStartEnd;

	m_pointlightParameters.center->setEnabled(pointlight);
	m_pointlightParameters.radius->setEnabled(pointlight);

	m_parallellightParameters.direction->setEnabled(parallellight);
	m_parallellightParameters.radius->setEnabled(parallellight);

	m_projectedlightParameters.target->setEnabled( projectedlight );
	m_projectedlightParameters.up->setEnabled( projectedlight );
	m_projectedlightParameters.right->setEnabled( projectedlight );
	m_projectedlightParameters.explicitStartEnd->setEnabled( projectedlight );
	m_projectedlightParameters.start->setEnabled( explicitStartEnd );
	m_projectedlightParameters.end->setEnabled( explicitStartEnd );
}


void fhLightEditor::Data::initFromSpawnArgs( const idDict* spawnArgs ) {
	if (spawnArgs->GetVector( "light_right", "-128 0 0", right )) {
		type = fhLightType::Projected;
	}
	else if (spawnArgs->GetInt( "parallel", "0" ) != 0) {
		type = fhLightType::Parallel;
	}
	else {
		type = fhLightType::Point;
	}

	explicitStartEnd = false;

	if(spawnArgs->GetVector("light_start", "0 0 0", start)) {
		explicitStartEnd = true;
	}

	if (spawnArgs->GetVector( "light_end", "0 0 0", end )) {
		explicitStartEnd = true;
	}

	target = spawnArgs->GetVector( "light_target", "0 0 -256" );
	up = spawnArgs->GetVector( "light_up", "0 -128 0" );
	radius = spawnArgs->GetVector( "light_radius", "100 100 100" );
	center = spawnArgs->GetVector( "light_center", "0 0 0" );
	color = spawnArgs->GetVector( "_color", "1 1 1" );
	castShadows = !spawnArgs->GetBool("noshadows", "0");

	name = spawnArgs->GetString("name");
	classname = spawnArgs->GetString("classname");
	material = spawnArgs->GetString("texture");
}

void fhLightEditor::Data::toSpawnArgs(idDict* spawnArgs) {	

	spawnArgs->Delete("light_right");
	spawnArgs->Delete("light_up");
	spawnArgs->Delete("light_target");
	spawnArgs->Delete("_color");
	spawnArgs->Delete("light_center");
	spawnArgs->Delete("light_radius");
	spawnArgs->Delete("light_start");
	spawnArgs->Delete("light_end");
	spawnArgs->Delete("parallel");
	spawnArgs->Delete("noshadows");
	spawnArgs->Delete("nospecular");
	spawnArgs->Delete("nodiffuse");

	switch(type) {
	case fhLightType::Projected:
		spawnArgs->SetVector("light_right", right);
		spawnArgs->SetVector("light_up", up);
		spawnArgs->SetVector("light_target", target);
		if(explicitStartEnd) {
			spawnArgs->SetVector("light_start", start);
			spawnArgs->SetVector("light_end", end);
		}
		break;
	case fhLightType::Parallel:
		spawnArgs->SetInt("parallel", 1);
		spawnArgs->SetVector( "light_radius", radius );
		spawnArgs->SetVector( "light_center", center );
		break;
	case fhLightType::Point:
		spawnArgs->SetInt("parallel", 0);
		spawnArgs->SetVector( "light_radius", radius );
		spawnArgs->SetVector( "light_center", center );
		break;
	};

	spawnArgs->SetVector("_color", color);	
	spawnArgs->SetBool("noshadows", !castShadows);

	if(!material.IsEmpty())
		spawnArgs->Set("texture", material.c_str());

	//TODO(johl): are expected to be there(?), but are not used by the game
	spawnArgs->SetBool("nospecular", false);
	spawnArgs->SetBool("nodiffuse", false);
	spawnArgs->SetFloat("falloff", 0.0f);
	
}



static void initVec3EditFromSpawnArg(fhVec3Edit* edit, const idDict* spawnArgs, const char* name, const char* defaultString) {
	idVec3 v = spawnArgs->GetVector(name, defaultString);
	edit->set(v);
}


void fhLightEditor::initFromSpawnArgs(const idDict* spawnArgs) {
	if(!spawnArgs) {
		m_currentData.name.Empty();
		m_currentData.classname.Empty();
		this->setWindowTitle(QString("Light Editor: <no light selected>"));
		return;
	}
	
	m_currentData.initFromSpawnArgs(spawnArgs);
	m_originalData = m_currentData;
	m_modified = false;

	if(m_currentData.classname != "light") {
		this->setWindowTitle(QString("Light Editor: <no light selected>"));
		return;
	}

	m_lighttype->setCurrentIndex((int)m_currentData.type);
	m_pointlightParameters.radius->set(m_currentData.radius);
	m_pointlightParameters.center->set(m_currentData.center);
	m_parallellightParameters.radius->set( m_currentData.radius );
	m_parallellightParameters.direction->set( m_currentData.center );
	m_projectedlightParameters.target->set( m_currentData.target );
	m_projectedlightParameters.right->set( m_currentData.right );
	m_projectedlightParameters.up->set( m_currentData.up );
	m_projectedlightParameters.start->set( m_currentData.start );
	m_projectedlightParameters.end->set( m_currentData.end );	
	m_projectedlightParameters.explicitStartEnd->setChecked( m_currentData.explicitStartEnd );
	m_castShadows->setChecked(m_currentData.castShadows);
	m_material->setCurrentText(m_currentData.material.c_str());


	QColor color;
	color.setRgbF(m_currentData.color.x, m_currentData.color.y, m_currentData.color.z);
	setLightColor(color);
	UpdateLightParameters();
	this->setWindowTitle(QString("Light Editor: %1").arg(m_currentData.name.c_str()));
}

void fhLightEditor::setLightColor(QColor color) {

	QString s( "background: #"
		+ QString( color.red() < 16 ? "0" : "" ) + QString::number( color.red(), 16 )
		+ QString( color.green() < 16 ? "0" : "" ) + QString::number( color.green(), 16 )
		+ QString( color.blue() < 16 ? "0" : "" ) + QString::number( color.blue(), 16 ) + ";" );
	m_colorButton->setStyleSheet( s );
	m_colorButton->update();

	QRgb rgb = color.rgb();
	m_currentData.color.x = qRed(rgb) / 255.0f;
	m_currentData.color.y = qGreen(rgb) / 255.0f;
	m_currentData.color.z = qBlue(rgb) / 255.0f;
	m_modified = true;
}

void fhLightEditor::UpdateGame() {
	if(m_currentData.name.IsEmpty() || m_currentData.classname != "light")
		return;

	if(!com_editorActive)
	{
		//use ingame
		idEntity* list[128];
		int count = gameEdit->GetSelectedEntities( list, 128 );

		idDict newSpawnArgs;
		m_currentData.toSpawnArgs( &newSpawnArgs );

		for (int i = 0; i < count; ++i) {
			gameEdit->EntityChangeSpawnArgs( list[i], &newSpawnArgs );
			gameEdit->EntityUpdateChangeableSpawnArgs( list[i], NULL );
			gameEdit->EntityUpdateVisuals( list[i] );
		}
	}
	else
	{	
		// used from Radiant
		for (brush_t *b = selected_brushes.next; b && b != &selected_brushes; b = b->next) {
			if ((b->owner->eclass->nShowFlags & ECLASS_LIGHT) && !b->entityModel) {
				m_currentData.toSpawnArgs(&b->owner->epairs);
				Brush_Build( b );
			}
		}
		Sys_UpdateWindows( W_ALL );
	}
}


void fhLightEditor::LoadMaterials() {
	m_material->clear();
	int count = declManager->GetNumDecls( DECL_MATERIAL );		
	m_material->addItem("");
	for (int i = 0; i < count; i++) {
		const idMaterial *mat = declManager->MaterialByIndex( i, false );
		idStr str = mat->GetName();
		str.ToLower();
		if (str.Icmpn( "lights/", strlen( "lights/" ) ) == 0 || str.Icmpn( "fogs/", strlen( "fogs/" ) ) == 0) {
			m_material->addItem(str.c_str());
		}
	}
}