// ===============================================================================
//						  NVIDIA PHYSX SDK TRAINING PROGRAMS
//			                  LESSON 1010: CLOTH METAL
//
//							Added by Xueyan Gong, 6-20-08
// ===============================================================================

#include <GL/glut.h>
#include <stdio.h>

#include "Lesson1010.h"

// Physics SDK globals
NxPhysicsSDK*     gPhysicsSDK = NULL;
NxScene*          gScene = NULL;
NxVec3            gDefaultGravity(0,-9.8,0);

// User report globals
DebugRenderer     gDebugRenderer;
UserAllocator*	  gAllocator;

// HUD globals
HUD hud;
char gTitleString[512] = "";

// Display globals
int gMainHandle;

// Camera globals
float gCameraAspectRatio = 1;
NxVec3 gCameraPos(-5,25,-35);
NxVec3 gCameraForward(0.1,-0.3,1);
NxVec3 gCameraRight(-1,0,0);
const NxReal gCameraSpeed = 20;

// Force globals
NxVec3 gForceVec(0,0,0);
NxReal gForceStrength = 10000;
bool bForceMode = true;

// Keyboard globals
#define MAX_KEYS 256
bool gKeys[MAX_KEYS];

// MouseGlobals
int mx = 0;
int my = 0;
NxDistanceJoint* gMouseJoint = NULL;
NxActor* gMouseSphere = NULL;
NxReal gMouseDepth = 0;

// Simulation globals
NxReal gDeltaTime = 1.0/60.0;
bool bHardwareScene = false;
bool gHardwareCloth = true;
bool bPause = false;
bool bShadows = true;
bool bDebugWireframeMode = false;
bool bLeftMouseButtonPressed = false;

// Actor globals
NxActor* groundPlane = NULL;

// Focus actor
NxActor* gSelectedActor = NULL;

// Hit actor
NxActor* gHitActor = NULL;

// Hit cloth and cloth vertex
NxCloth* gHitCloth = NULL;
int		 gHitClothVertex = -1;

// Array of cloth objects
NxArray<MyCloth*> gCloths;

NxArray<NxActor *> gMetalCores;


void PrintControls()
{
	printf("\n Flight Controls:\n ----------------\n w = forward, s = back\n a = strafe left, d = strafe right\n q = up, z = down\n");
    printf("\n Force Controls:\n ----------------\n i = +z, k = -z\n j = +x, l = -x\n u = +y, m = -y\n");
	printf("\n Miscellaneous:\n ----------------\n p = Pause\n r = Select Next Actor\n f = Toggle Force Mode\n b = Toggle Debug Wireframe Mode\n x = Toggle Shadows\n Space = Shoot a sphere\n Right Mouse = Drag objects\n");
}

bool IsSelectable(NxActor* actor)
{
   NxShape*const* shapes = gSelectedActor->getShapes();
   NxU32 nShapes = gSelectedActor->getNbShapes();
   while (nShapes--)
   {
       if (shapes[nShapes]->getFlag(NX_TRIGGER_ENABLE)) 
       {           
           return false;
       }
   }

   if (actor == groundPlane)
       return false;

   return true;
}

void SelectNextActor()
{
   NxU32 nbActors = gScene->getNbActors();
   NxActor** actors = gScene->getActors();
   for(NxU32 i = 0; i < nbActors; i++)
   {
       if (actors[i] == gSelectedActor)
       {
           NxU32 j = 1;
           gSelectedActor = actors[(i+j)%nbActors];
           while (!IsSelectable(gSelectedActor))
           {
               j++;
               gSelectedActor = actors[(i+j)%nbActors];
           }
           break;
       }
   }
}

void ProcessCameraKeys()
{
	NxReal deltaTime;

    if (bPause) deltaTime = 0.02; else deltaTime = gDeltaTime;   

	// Process camera keys
	for (int i = 0; i < MAX_KEYS; i++)
	{	
		if (!gKeys[i])  { continue; }

		switch (i)
		{
			// Camera controls
			case 'w':{ gCameraPos += gCameraForward*gCameraSpeed*deltaTime; break; }
			case 's':{ gCameraPos -= gCameraForward*gCameraSpeed*deltaTime; break; }
			case 'a':{ gCameraPos -= gCameraRight*gCameraSpeed*deltaTime; break; }
			case 'd':{ gCameraPos += gCameraRight*gCameraSpeed*deltaTime; break; }
			case 'z':{ gCameraPos -= NxVec3(0,1,0)*gCameraSpeed*deltaTime; break; }
			case 'q':{ gCameraPos += NxVec3(0,1,0)*gCameraSpeed*deltaTime; break; }
		}
	}
}

void SetupCamera()
{
	// Setup camera
	glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0f, gCameraAspectRatio, 1.0f, 10000.0f);
	gluLookAt(gCameraPos.x,gCameraPos.y,gCameraPos.z,gCameraPos.x + gCameraForward.x, gCameraPos.y + gCameraForward.y, gCameraPos.z + gCameraForward.z, 0.0f, 1.0f, 0.0f);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

void ViewProject(NxVec3 &v, int &xi, int &yi, float &depth)
{
//We cannot do picking easily on the xbox/PS3 anyway
#if defined(_XBOX)||defined(__CELLOS_LV2__)
	xi=yi=0;
	depth=0;
#else
	GLint viewPort[4];
	GLdouble modelMatrix[16];
	GLdouble projMatrix[16];
	glGetIntegerv(GL_VIEWPORT, viewPort);
	glGetDoublev(GL_MODELVIEW_MATRIX, modelMatrix);
	glGetDoublev(GL_PROJECTION_MATRIX, projMatrix);
	GLdouble winX, winY, winZ;
	gluProject((GLdouble) v.x, (GLdouble) v.y, (GLdouble) v.z,
		modelMatrix, projMatrix, viewPort, &winX, &winY, &winZ);
	xi = (int)winX; yi = viewPort[3] - (int)winY - 1; depth = (float)winZ;
#endif
}

void ViewUnProject(int xi, int yi, float depth, NxVec3 &v)
{
//We cannot do picking easily on the xbox/PS3 anyway
#if defined(_XBOX)||defined(__CELLOS_LV2__)
	v=NxVec3(0,0,0);
#else
	GLint viewPort[4];
	GLdouble modelMatrix[16];
	GLdouble projMatrix[16];
	glGetIntegerv(GL_VIEWPORT, viewPort);
	glGetDoublev(GL_MODELVIEW_MATRIX, modelMatrix);
	glGetDoublev(GL_PROJECTION_MATRIX, projMatrix);
	yi = viewPort[3] - yi - 1;
	GLdouble wx, wy, wz;
	gluUnProject((GLdouble) xi, (GLdouble) yi, (GLdouble) depth,
	modelMatrix, projMatrix, viewPort, &wx, &wy, &wz);
	v.set((NxReal)wx, (NxReal)wy, (NxReal)wz);
#endif
}

void RenderActors(bool shadows)
{
    // Render all the actors in the scene
    NxU32 nbActors = gScene->getNbActors();
    NxActor** actors = gScene->getActors();
    while (nbActors--)
    {
        NxActor* actor = *actors++;

		bool isMetalCore = false;
		for (NxActor **core = gMetalCores.begin(); core != gMetalCores.end(); core++)
			if (actor == *core) isMetalCore = true;

		if (!isMetalCore)
			DrawActor(actor, gSelectedActor, false);

        // Handle shadows
        if (shadows)
        {
			DrawActorShadow(actor, false);
        }
    }
}

void DrawForce(NxActor* actor, NxVec3& forceVec, const NxVec3& color)
{
	// Draw only if the force is large enough
	NxReal force = forceVec.magnitude();
	if (force < 0.1)  return;

	forceVec = 3*forceVec/force;

	NxVec3 pos;
	if(actor->isDynamic())
	{
		pos= actor->getCMassGlobalPosition();
	}
	else
	{
		pos= actor->getGlobalPosition();
	}
	DrawArrow(pos, pos + forceVec, color);
}

NxVec3 ApplyForceToActor(NxActor* actor, const NxVec3& forceDir, const NxReal forceStrength, bool forceMode)
{
	NxVec3 forceVec = forceStrength*forceDir*gDeltaTime;

	if (forceMode)
		actor->addForce(forceVec);
	else 
		actor->addTorque(forceVec);

	return forceVec;
}

void ProcessForceKeys()
{
	// Process force keys
	for (int i = 0; i < MAX_KEYS; i++)
	{	
		if (!gKeys[i])  { continue; }

		switch (i)
		{
			// Force controls
			case 'i': { gForceVec = ApplyForceToActor(gSelectedActor,NxVec3(0,0,1),gForceStrength,bForceMode); break; }
			case 'k': { gForceVec = ApplyForceToActor(gSelectedActor,NxVec3(0,0,-1),gForceStrength,bForceMode); break; }
			case 'j': { gForceVec = ApplyForceToActor(gSelectedActor,NxVec3(1,0,0),gForceStrength,bForceMode); break; }
			case 'l': { gForceVec = ApplyForceToActor(gSelectedActor,NxVec3(-1,0,0),gForceStrength,bForceMode); break; }
			case 'u': { gForceVec = ApplyForceToActor(gSelectedActor,NxVec3(0,1,0),gForceStrength,bForceMode); break; }
			case 'm': { gForceVec = ApplyForceToActor(gSelectedActor,NxVec3(0,-1,0),gForceStrength,bForceMode); break; }
		}
	}
}

void ProcessInputs()
{
    ProcessForceKeys();

    // Show debug wireframes
	if (bDebugWireframeMode)
	{
		glDisable(GL_LIGHTING);
		if (gScene)  gDebugRenderer.renderData(*gScene->getDebugRenderable());
		glEnable(GL_LIGHTING);
	}
}

void RenderCallback()
{
    if (gScene && !bPause)
	{
		StartPhysics();	
		GetPhysicsResults();
	}

    // Clear buffers
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	ProcessInputs();
	ProcessCameraKeys();
	SetupCamera();

 	RenderActors(bShadows);

    // Render all the cloths in the scene
	for (MyCloth **cloth = gCloths.begin(); cloth != gCloths.end(); cloth++)
	{
		glColor4f(1.0f, 0.0f, 0.0f,1.0f);
		(*cloth)->draw(bShadows);
	}

	if (bForceMode)
		DrawForce(gSelectedActor, gForceVec, NxVec3(1,1,0));
	else
		DrawForce(gSelectedActor, gForceVec, NxVec3(0,1,1));
	gForceVec = NxVec3(0,0,0);

	// Render HUD
	hud.Render();

    glFlush();
    glutSwapBuffers();
}

void ReshapeCallback(int width, int height)
{
    glViewport(0, 0, width, height);
    gCameraAspectRatio = float(width)/float(height);
}

void IdleCallback()
{
    glutPostRedisplay();
}

void KeyboardCallback(unsigned char key, int x, int y)
{
	gKeys[key] = true;

	switch (key)
	{
		case 'r': { SelectNextActor(); break; }
		default: { break; }
	}
}

void KeyboardUpCallback(unsigned char key, int x, int y)
{
	gKeys[key] = false;

	switch (key)
	{
		case 'p': { bPause = !bPause; 
					if (bPause)
						hud.SetDisplayString(2, "Paused - Hit \"p\" to Unpause", 0.3f, 0.55f);
					else
						hud.SetDisplayString(2, "", 0.0f, 0.0f);	
					break; }
		case 'x': { bShadows = !bShadows; break; }
		case 'b': { bDebugWireframeMode = !bDebugWireframeMode; break; }		
		case 'f': { bForceMode = !bForceMode; break; }

		case 'o': 
		{     
			NxCloth** cloths = gScene->getCloths();
			for (NxU32 i = 0; i < gScene->getNbCloths(); i++) 
			{
				cloths[i]->setFlags(cloths[i]->getFlags() ^ NX_CLF_BENDING_ORTHO);
			}
			break;
		}

		case 'g': 
		{     
			NxCloth** cloths = gScene->getCloths();
			for (NxU32 i = 0; i < gScene->getNbCloths(); i++) 
			{
				cloths[i]->setFlags(cloths[i]->getFlags() ^ NX_CLF_GRAVITY);
			}
			break;
		}

		case 'y': 
		{
			NxCloth** cloths = gScene->getCloths();
			for (NxU32 i = 0; i < gScene->getNbCloths(); i++) 
			{
				cloths[i]->setFlags(cloths[i]->getFlags() ^ NX_CLF_BENDING);
			}	            
			break;
		}
		case ' ': 
		{
			NxActor* sphere = CreateSphere(gCameraPos, 1, 1);
			sphere->setLinearVelocity(gCameraForward * 20);
			break; 
		}
		case 27 : { exit(0); break; }
		default : { break; }
	}
}

void LetGo()
{
	if (gMouseJoint) 
		gScene->releaseJoint(*gMouseJoint);
	gMouseJoint = NULL;
	if (gMouseSphere)
		gScene->releaseActor(*gMouseSphere);
	gMouseSphere = NULL;
	gHitActor = NULL;

	if (gHitCloth)
		gHitCloth->freeVertex(gHitClothVertex);
	gHitClothVertex = -1;
	gHitCloth = NULL;
}

void Pick(int x, int y)
{
	LetGo();

	NxRay ray; 
	ViewUnProject(x, y, 0.0f, ray.orig);
	ViewUnProject(x, y, 1.0f, ray.dir);
	ray.dir -= ray.orig; 
	ray.dir.normalize();

	NxRaycastHit hit;
	NxShape* closestShape = gScene->raycastClosestShape(ray, NX_ALL_SHAPES, hit);

	NxVec3 origin = ray.orig;
	NxVec3 impact = hit.worldImpact;
	NxReal distRB = NX_MAX_REAL;
	if (closestShape && closestShape->getActor().isDynamic()) 
	{
		distRB = (impact.x - origin.x) * (impact.x - origin.x) + (impact.y - origin.y) * (impact.y - origin.y) + (impact.z - origin.z) * (impact.z - origin.z);
	}

	NxVec3 hitcloth, hitRecord;
	NxU32 vertexId;
	NxReal distCloth, closest= NX_MAX_REAL;
	NxU32 indexCloth, indexClothVertex;

	NxCloth **cloths = gScene->getCloths();
	for (NxU32 i = 0; i < gScene->getNbCloths(); i++) 
	{
		if (cloths[i]->raycast(ray, hitcloth, vertexId))
		{
			distCloth = (hitcloth.x - origin.x) * (hitcloth.x - origin.x) + (hitcloth.y - origin.y) * (hitcloth.y - origin.y) + (hitcloth.z - origin.z) * (hitcloth.z - origin.z);
			if(distCloth < closest)	
			{
				closest = distCloth;
				indexCloth = i;
				indexClothVertex = vertexId;
				hitRecord = hitcloth;

			}
		}
	}

	if (distRB > closest) // Pick cloth
	{
		gHitCloth = cloths[indexCloth];
		gHitClothVertex = indexClothVertex;

		int hitx, hity;
		ViewProject(hitRecord, hitx, hity, gMouseDepth);

	}
	else if (distRB < closest) // Pick actor
	{
		gHitActor = &closestShape->getActor();
		gHitActor->wakeUp();

		int hitx, hity;
		gMouseSphere = CreateSphere(hit.worldImpact, 0.1, 1);
		gMouseSphere->raiseBodyFlag(NX_BF_KINEMATIC);
		gMouseSphere->raiseActorFlag(NX_AF_DISABLE_COLLISION);
		ViewProject(hit.worldImpact, hitx, hity, gMouseDepth);

		NxDistanceJointDesc desc;
		desc.actor[0] = gMouseSphere;
		desc.actor[1] = gHitActor;
		gMouseSphere->getGlobalPose().multiplyByInverseRT(hit.worldImpact, desc.localAnchor[0]);
		gHitActor->getGlobalPose().multiplyByInverseRT(hit.worldImpact, desc.localAnchor[1]);
		desc.spring.damper = 1;
		desc.spring.spring = 200;
		desc.flags |= NX_DJF_MAX_DISTANCE_ENABLED | NX_DJF_SPRING_ENABLED;
		NxJoint* joint = gScene->createJoint(desc);
		gMouseJoint = (NxDistanceJoint*)joint->is(NX_JOINT_DISTANCE);

	}
}

void SpecialCallback(int key, int x, int y)
{
	switch (key)
    {
		// Reset PhysX
		case GLUT_KEY_F10: ResetNx(); return; 
	}
}

void MouseCallback(int button, int state, int x, int y)
{
	mx = x;
	my = y;

	if (button == GLUT_RIGHT_BUTTON && state == GLUT_DOWN) 
	{
		Pick(x, y);

		// this prevents from only grabbing it when the mouse moves
		MotionCallback(x, y);
	}
	else if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN)
	{
		bLeftMouseButtonPressed = true;
	}
	if (state == GLUT_UP) 
	{
		LetGo();
		bLeftMouseButtonPressed = false;
	}
}

void MotionCallback(int x, int y)
{
	int dx = mx - x;
	int dy = my - y;

	if (gMouseSphere) // Move the mouse sphere
	{
		NxVec3 pos;
		ViewUnProject(x,y, gMouseDepth, pos);
		gMouseSphere->setGlobalPosition(pos);
		gHitActor->wakeUp();
	}
	else if (gHitCloth) // Attach the cloth vertex
	{
		NxVec3 pos; 
		ViewUnProject(x,y, gMouseDepth, pos);
		gHitCloth->attachVertexToGlobalPosition(gHitClothVertex, pos);
	}
	else if (bLeftMouseButtonPressed) // Set camera
	{   
		gCameraForward.normalize();
		gCameraRight.cross(gCameraForward,NxVec3(0,1,0));

		NxQuat qx(NxPiF32 * dx * 20 / 180.0f, NxVec3(0,1,0));
		qx.rotate(gCameraForward);
		NxQuat qy(NxPiF32 * dy * 20 / 180.0f, gCameraRight);
		qy.rotate(gCameraForward);
	}

    mx = x;
    my = y;
}

void ExitCallback()
{
	ReleaseNx();
}

void InitGlut(int argc, char** argv, char* lessonTitle)
{
    glutInit(&argc, argv);
    glutInitWindowSize(512, 512);
    glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);
    gMainHandle = glutCreateWindow(lessonTitle);
    glutSetWindow(gMainHandle);
    glutDisplayFunc(RenderCallback);
    glutReshapeFunc(ReshapeCallback);
    glutIdleFunc(IdleCallback);
    glutKeyboardFunc(KeyboardCallback);
    glutKeyboardUpFunc(KeyboardUpCallback);
	glutSpecialFunc(SpecialCallback);
    glutMouseFunc(MouseCallback);
    glutMotionFunc(MotionCallback);
	MotionCallback(0,0);
	atexit(ExitCallback);

    // Setup default render states
    glClearColor(0.5f, 0.6f, 0.7f, 1.0);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_COLOR_MATERIAL);
	glShadeModel(GL_SMOOTH);
	glLightModelf(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);

    // Setup lighting
    glEnable(GL_LIGHTING);
    float AmbientColor[]    = { 0.0f, 0.1f, 0.2f, 0.0f };         glLightfv(GL_LIGHT0, GL_AMBIENT, AmbientColor);
    float DiffuseColor[]    = { 0.6f, 0.6f, 0.6f, 0.0f };         glLightfv(GL_LIGHT0, GL_DIFFUSE, DiffuseColor);
    float SpecularColor[]   = { 0.5f, 0.5f, 0.5f, 0.0f };         glLightfv(GL_LIGHT0, GL_SPECULAR, SpecularColor);
    float Position[]        = { 100.0f, 100.0f, -400.0f, 1.0f };  glLightfv(GL_LIGHT0, GL_POSITION, Position);
    glEnable(GL_LIGHT0);
}

void CreateMetalCloth(const NxVec3 &position, int mode, NxClothDesc &clothDesc, char *meshName)
{
	// Global metal data
	NxReal impulseThreshold  = 50.0f;
	NxReal penetrationDepth  = 0.5f;
	NxReal maxDeformationDistance = 0.5f;

	NxBodyDesc  coreBodyDesc;
	coreBodyDesc.linearDamping = 0.2f;
	coreBodyDesc.angularDamping = 0.2f;

	NxActorDesc coreActorDesc;
	coreActorDesc.density = 0.1f;
	coreActorDesc.body = &coreBodyDesc;

	// Create the shape, no need for size info because it is automatically generated by the cloth
	if (mode == 0) { // Sphere as core
		coreActorDesc.shapes.pushBack(new NxSphereShapeDesc());
	}
	else if (mode == 1) { // Capsule as core
		coreActorDesc.shapes.pushBack(new NxCapsuleShapeDesc());
	}
	else if (mode == 2) { // Box as core
		coreActorDesc.shapes.pushBack(new NxBoxShapeDesc());
	}
	else if (mode == 3) { // Compound of spheres as core
		const NxU32 numSpheres = 10;
		for (NxU32 i = 0; i < numSpheres; i++) 
			coreActorDesc.shapes.pushBack(new NxSphereShapeDesc());
	}
	else return;

	NxActor *coreActor = gScene->createActor(coreActorDesc);
	gMetalCores.push_back(coreActor);

	// Clean up allocations
	for (NxU32 i = 0; i < coreActorDesc.shapes.size(); i++)
		delete coreActorDesc.shapes[i];

	clothDesc.globalPose.t = position;

	MyCloth *objCloth = new MyCloth(gScene, clothDesc, meshName, 1.0f);
	if (!objCloth->getNxCloth())
	{
		printf("Error: Unable to create the cloth for the current scene.\n");
		delete objCloth;
	} else
	{
		gCloths.push_back(objCloth);
		objCloth->getNxCloth()->attachToCore(coreActor, impulseThreshold, penetrationDepth, maxDeformationDistance); 
	}
}

void SetupMetalScene()
{
    sprintf(gTitleString, "Cloth Metal Demo");

	// Create the objects in the scene
	groundPlane = CreateGroundPlane();
	NxActor *box1 = CreateBox(NxVec3(0,0,10), NxVec3(20,10,0.5), 0);
	NxActor *box2 = CreateBox(NxVec3(-20,0,0), NxVec3(0.5,10,10), 0); 
	NxActor *box3 = CreateBox(NxVec3( 20,0,0), NxVec3(0.5,10,10), 0); 

	NxClothDesc clothDesc;

	if (gHardwareCloth)
		clothDesc.flags |= NX_CLF_HARDWARE;

	// Create metal cloth with different core actors
	CreateMetalCloth(NxVec3(0.0f,  3.0f, 0.5f), 0, clothDesc, "barrel.obj");
	CreateMetalCloth(NxVec3(0.5f, 20.0f, 0.0f), 1, clothDesc, "barrel.obj");
	CreateMetalCloth(NxVec3(0.0f, 40.0f, 0.5f), 2, clothDesc, "barrel.obj");
	CreateMetalCloth(NxVec3(0.5f, 60.0f, 0.0f), 3, clothDesc, "barrel.obj");

}

void InitializeHUD()
{
	bHardwareScene = (gScene->getSimType() == NX_SIMULATION_HW);

	hud.Clear();

	// Add hardware/software to HUD
	if (bHardwareScene)
	    hud.AddDisplayString("Hardware Scene", 0.74f, 0.92f);
	else
		hud.AddDisplayString("Software Scene", 0.74f, 0.92f);

	if (gHardwareCloth)
	    hud.AddDisplayString("Hardware Cloth", 0.74f, 0.87f);
	else
		hud.AddDisplayString("Software Cloth", 0.74f, 0.87f);

	// Add pause to HUD
	if (bPause)  
		hud.AddDisplayString("Paused - Hit \"p\" to Unpause", 0.3f, 0.55f);
	else
		hud.AddDisplayString("", 0.0f, 0.0f);
}

void InitializeSpecialHUD()
{
	char ds[512];

	// Add lesson title string to HUD
	sprintf(ds, gTitleString);
	hud.AddDisplayString(ds, 0.015f, 0.92f);
}

void InitNx()
{
	// Create a memory allocator
    gAllocator = new UserAllocator;

    // Create the physics SDK
	gPhysicsSDK = NxCreatePhysicsSDK(NX_PHYSICS_SDK_VERSION, gAllocator);
	if (!gPhysicsSDK)  return;

	// Set the physics parameters
	gPhysicsSDK->setParameter(NX_SKIN_WIDTH, 0.01);

	// Set the debug visualization parameters
	gPhysicsSDK->setParameter(NX_VISUALIZATION_SCALE, 1);
	gPhysicsSDK->setParameter(NX_VISUALIZE_COLLISION_SHAPES, 1);
	gPhysicsSDK->setParameter(NX_VISUALIZE_ACTOR_AXES, 1);
	gPhysicsSDK->setParameter(NX_VISUALIZE_CLOTH_COLLISIONS, 1);
	gPhysicsSDK->setParameter(NX_VISUALIZE_CLOTH_SLEEP, 1);

	// Check available hardware
	NxHWVersion hwCheck = gPhysicsSDK->getHWVersion();
	if (hwCheck == NX_HW_VERSION_NONE) 
	{
		gHardwareCloth = false;
	}
	else 
		gHardwareCloth = true;

	// Create the scenes
    NxSceneDesc sceneDesc;
    sceneDesc.gravity = gDefaultGravity;
    sceneDesc.simType = NX_SIMULATION_HW;
    gScene = gPhysicsSDK->createScene(sceneDesc);	
	if(!gScene){ 
		sceneDesc.simType = NX_SIMULATION_SW; 
		gScene = gPhysicsSDK->createScene(sceneDesc);  
		if(!gScene) return;
	}

	// Create the default material
	NxMaterialDesc       m; 
	m.restitution        = 0.5;
	m.staticFriction     = 0.2;
	m.dynamicFriction    = 0.2;
	NxMaterial* mat = gScene->getMaterialFromIndex(0);
	mat->loadFromDesc(m); 

    SetupMetalScene();

	if (gScene->getNbActors() > 0)
		gSelectedActor = *gScene->getActors();
	else
		gSelectedActor = NULL;

	// Initialize HUD
    InitializeHUD();
	InitializeSpecialHUD();
}

int main(int argc, char** argv)
{
	PrintControls();
	InitGlut(argc, argv, "Lesson 1010: Cloth Metal");
    InitNx();
    glutMainLoop();
	ReleaseNx();
	return 0;
}

void ReleaseNx()
{
    if (gScene)
	{
		for (MyCloth** cloth = gCloths.begin(); cloth != gCloths.end(); cloth++)
			delete *cloth;
		gCloths.clear();

		gMetalCores.clear();

		gPhysicsSDK->releaseScene(*gScene);
	}
	if (gPhysicsSDK)  NxReleasePhysicsSDK(gPhysicsSDK);
    NX_DELETE_SINGLE(gAllocator);
}

void ResetNx()
{
	LetGo();
	ReleaseNx();
	InitNx();
}

void StartPhysics()
{
	// Start collision and dynamics for delta time since the last frame
    gScene->simulate(gDeltaTime);
	gScene->flushStream();
}

void GetPhysicsResults()
{
	// Get results from gScene->simulate(gDeltaTime)
	gScene->fetchResults(NX_RIGID_BODY_FINISHED, true);
}
