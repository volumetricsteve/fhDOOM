
static QApplication * app = nullptr;
static QLabel * currentEntity = nullptr;

void runQtEditors()
{	
	QApplication::processEvents();

	if(!app)
	{
		static char* name = "foo";
		char** argv = &name;
		int argc = 1;
		app = new QApplication(argc, argv);
		currentEntity = new QLabel("?");
		currentEntity->show();		
	}

	idEntity* entityList[128];
	int num = gameEdit->GetSelectedEntities(entityList, 128);
	if(num <= 0) {
		currentEntity->setText("<none>");
	} else {
		auto spawnArgs = gameEdit->EntityGetSpawnArgs( entityList[0] );
		if(spawnArgs)
		{
			auto value = spawnArgs->FindKey("name");
			if(value)
			{
				currentEntity->setText(value->GetValue().c_str());
			}
		}

	}	
}