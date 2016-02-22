#include "dialogs/LightEditor.h"

static QApplication * app = nullptr;
static fhLightEditor * lightEditor = nullptr;

void runQtEditors()
{	
	if(!app)
	{
		static char* name = "foo";
		char** argv = &name;
		int argc = 1;
		app = new QApplication(argc, argv);
	}

	QApplication::processEvents();

#if 0

#endif
}


void QtLightEditorInit( const idDict* spawnArgs ) {
	if(!lightEditor) {
		lightEditor = new fhLightEditor(nullptr);		
	}

	lightEditor->initFromSpawnArgs(spawnArgs);
	lightEditor->show();
}