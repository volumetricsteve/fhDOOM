#pragma once

#include "../precompiled.h"
#include "../widgets/Vec3Edit.h"
#include <qdialog.h>

class QCheckBox;
class QComboBox;

class fhEditing {
public:
	virtual ~fhEditing() {}
		
};

class fhLightEditor : public QDialog {
	Q_OBJECT

public:
	fhLightEditor(QWidget* parent);
	~fhLightEditor();

	void initFromSpawnArgs(const idDict* spawnArgs);

	virtual void closeEvent(QCloseEvent* event) override;

private:
	enum class fhLightType
	{
		Point = 0,
		Parallel = 1,
		Projected = 2
	};

	struct Data {
		idStr name;
		idStr classname;
		idStr material;
		fhLightType type;
		idVec3 radius;
		idVec3 center;
		idVec3 target;
		idVec3 up;
		idVec3 right;
		idVec3 start;
		idVec3 end;
		idVec3 color;
		bool castShadows;
		bool explicitStartEnd;

		void initFromSpawnArgs( const idDict* spawnArgs );
		void toSpawnArgs(idDict* spawnArgs);
	};
	

	Data m_originalData;
	Data m_currentData;
	bool m_modified;
	QComboBox* m_lighttype;
	QComboBox* m_material;
	QPushButton* m_colorButton;
	QPushButton* m_cancelButton;
	QPushButton* m_applyButton;
	QPushButton* m_okButton;
	QCheckBox* m_castShadows;	

	struct PointLightParameters {
		fhVec3Edit* radius;
		fhVec3Edit* center;
	} m_pointlightParameters;

	struct ParallelLightParameters {
		fhVec3Edit* radius;
		fhVec3Edit* direction;
	} m_parallellightParameters;

	struct ProjectedLightParameters {
		fhVec3Edit* target;
		fhVec3Edit* right;
		fhVec3Edit* up;
		fhVec3Edit* start;
		fhVec3Edit* end;
		QCheckBox* explicitStartEnd;
	} m_projectedlightParameters;

	void UpdateLightParameters();
	void setLightColor(idVec3 color);
	void setLightColor(QColor color);
	void UpdateGame();

	QWidget* CreatePointLightParameters();
	QWidget* CreateParallelLightParameters();
	QWidget* CreateProjectedLightParameters();

	void LoadMaterials();
};