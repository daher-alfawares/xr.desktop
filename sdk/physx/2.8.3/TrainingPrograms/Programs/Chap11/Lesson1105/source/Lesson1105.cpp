// ===============================================================================
//						  NVIDIA PHYSX SDK TRAINING PROGRAMS
//			                     LESSON 1105: Soft Wheel Car
//
// ===============================================================================

#include <GL/glut.h>
#include <stdio.h>

#include "Lesson1105.h"
#include "Timing.h"

#include "objMesh.h"
#include "MediaPath.h"
#include "joints.h"
// Physics SDK globals
NxPhysicsSDK*     gPhysicsSDK = NULL;
NxScene*          gScene = NULL;
NxVec3            gDefaultGravity(0,-9.8,0);

// User report globals
DebugRenderer     gDebugRenderer;
UserAllocator*	  gAllocator;
ErrorStream       gErrorStream;

// HUD globals
HUD hud;
char gTitleString[512] = "";

// Display globals
int gMainHandle;

// Camera globals
float gCameraAspectRatio = 1;
NxVec3 gCameraPos(0,5,-15);
NxVec3 gCameraForward(0,0,1);
NxVec3 gCameraRight(-1,0,0);
const NxReal gCameraSpeed = 20;

// Force globals
NxVec3 gForceVec(0,0,0);
NxReal gForceStrength = 100000;
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

bool bPause = false;
bool bShadows = true;
bool bDebugWireframeMode = false;

// Actor globals
NxActor* groundPlane = NULL;

// Focus actor
NxActor* gSelectedActor = NULL;

// Selected softbody and softbody vertex
NxSoftBody* gSelectedSoftBody = NULL;
int		 gSelectedSoftBodyVertex = -1;

// Array of SoftBody objects
NxArray<MySoftBody*> gSoftBodies;
NxArray<ObjMesh *> gObjMeshes;

bool shadows = false;
bool gShowTetraMesh = false;

#if defined(NX_DISABLE_HARDWARE) || defined(_XBOX) || defined(__CELLOS_LV2__)
bool gHardwareSoftBodySimulation = false;
bool bHardwareScene = false;
#else
bool gHardwareSoftBodySimulation = true;
bool bHardwareScene = true;
#endif

void PrintControls()
{
	printf("\n Flight Controls:\n ----------------\n w = forward, s = back\n a = strafe left, d = strafe right\n q = up, z = down\n");
	printf("\n Force Controls:\n ---------------\n i = +z, k = -z\n j = +x, l = -x\n u = +y, m = -y\n");
	printf("\n Miscellaneous:\n --------------\n p = Pause\n r = Select Next Actor\n f = Toggle Force Mode\n b = Toggle Debug Wireframe Mode\n Space = Shoot a sphere\n");
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

	if (bPause) deltaTime = 0.0005; else deltaTime = gDeltaTime;   

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

// ------------------------------------------------------------------------------------
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

// ------------------------------------------------------------------------------------
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

				  // Return focus actor to (0,5,0)
				  // case 't': { if (gSelectedActor->isDynamic()) gSelectedActor->setGlobalPosition(NxVec3(0,5,0)); break; }
		}
	}
}

void ProcessInputs()
{
	ProcessForceKeys();

	// Show debug wireframes
	if (bDebugWireframeMode)
	{
		if (gScene)  gDebugRenderer.renderData(*gScene->getDebugRenderable());
	}
}

void RenderCallback()
{
	// Clear buffers
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	ProcessCameraKeys();
	SetupCamera();

	if (gScene && !bPause)
	{
		// Update the time step
		gDeltaTime = getElapsedTime();
		//gDeltaTime = 1.0/60.0;

		GetPhysicsResults();
		ProcessInputs();
		StartPhysics();
	//	printf("dt is %f ",gDeltaTime);

		NxReal maxTimestep = 0;
		NxU32 maxIter = 0;
		NxU32 numSubSteps = 0;
		NxTimeStepMethod tsM;
	/*	gScene->getTiming(maxTimestep,maxIter,tsM,&numSubSteps);
		printf("maxTimestep is %f\n ",maxTimestep);
		printf("maxIter is %d\n",maxIter);
		printf("numSubSteps is %d\n",numSubSteps);
	*/
	}

	// Display first scene
	RenderActors(bShadows);

	// Render all the SoftBodys in the scene
	{
		// Render all the soft bodies in the scene
		for (MySoftBody **softBody = gSoftBodies.begin(); softBody != gSoftBodies.end(); softBody++){
			glColor4f(1.0f, 0.0f, 0.0f, 1.0f);
			(*softBody)->simulateAndDraw(shadows, gShowTetraMesh, false);
		}
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
			hud.SetDisplayString(1, "Paused - Hit \"p\" to Unpause", 0.3f, 0.55f);
		else
			hud.SetDisplayString(1, "", 0.0f, 0.0f);	
		getElapsedTime(); 
		break; }
	case 'x': { bShadows = !bShadows; break; }
	case 'b': { bDebugWireframeMode = !bDebugWireframeMode; break; }		
	case 'f': { bForceMode = !bForceMode; break; }

	case 'o': 
		{     
			NxSoftBody** SoftBodys = gScene->getSoftBodies();
			for (NxU32 i = 0; i < gScene->getNbSoftBodies(); i++) 
			{
				SoftBodys[i]->setFlags(SoftBodys[i]->getFlags() ^ NX_CLF_BENDING_ORTHO);
			}
			break;
		}

	case 'g': 
		{     
			NxSoftBody** SoftBodys = gScene->getSoftBodies();
			for (NxU32 i = 0; i < gScene->getNbSoftBodies(); i++) 
			{
				SoftBodys[i]->setFlags(SoftBodys[i]->getFlags() ^ NX_CLF_GRAVITY);
			}
			break;
		}
	case 'y': 
		{
			NxSoftBody** SoftBodys = gScene->getSoftBodies();
			for (NxU32 i = 0; i < gScene->getNbSoftBodies(); i++) 
			{
				SoftBodys[i]->setFlags(SoftBodys[i]->getFlags() ^ NX_CLF_BENDING);
			}	            
			break;
		}
	case ' ': 
		{
			// you can't add actors to the scene 
			// while the simulation loop is running.
			// wait until the results are available
			gScene->checkResults(NX_RIGID_BODY_FINISHED,true);
			NxActor* sphere = CreateSphere(gCameraPos, 1, 1);
			sphere->setLinearVelocity(gCameraForward * 20);
			break; 
		}
	case 27 : { exit(0); break; }
	default : { break; }
	}
}

// ------------------------------------------------------------------------------------
void LetGoActor()
{
	if (gMouseJoint) 
		gScene->releaseJoint(*gMouseJoint);
	gMouseJoint = NULL;
	if (gMouseSphere)
		gScene->releaseActor(*gMouseSphere);
	gMouseSphere = NULL;
}

// ------------------------------------------------------------------------------------
bool PickActor(int x, int y)
{
	LetGoActor();

	NxRay ray; 
	ViewUnProject(x,y,0, ray.orig);
	ViewUnProject(x,y,1, ray.dir);
	ray.dir -= ray.orig; ray.dir.normalize();

	NxRaycastHit hit;
	NxShape* closestShape = gScene->raycastClosestShape(ray, NX_ALL_SHAPES, hit);
	if (!closestShape || &closestShape->getActor() == groundPlane) return false;
	if (!closestShape->getActor().isDynamic()) return false;
	int hitx, hity;
	ViewProject(hit.worldImpact, hitx, hity, gMouseDepth);
	gMouseSphere = CreateSphere(hit.worldImpact, 0.1, 1);
	if(gMouseSphere)
	{
		gMouseSphere->raiseBodyFlag(NX_BF_KINEMATIC);
		gMouseSphere->raiseActorFlag(NX_AF_DISABLE_COLLISION);
		NxDistanceJointDesc desc;
		gSelectedActor = &closestShape->getActor();
		desc.actor[0] = gMouseSphere;
		desc.actor[1] = gSelectedActor;
		gMouseSphere->getGlobalPose().multiplyByInverseRT(hit.worldImpact, desc.localAnchor[0]);
		gSelectedActor->getGlobalPose().multiplyByInverseRT(hit.worldImpact, desc.localAnchor[1]);
		desc.maxDistance = 0;
		desc.minDistance = 0;
		desc.spring.damper = 1;
		desc.spring.spring = 200;
		desc.flags |= NX_DJF_MAX_DISTANCE_ENABLED | NX_DJF_SPRING_ENABLED;
		NxJoint* joint = gScene->createJoint(desc);
		gMouseJoint = (NxDistanceJoint*)joint->is(NX_JOINT_DISTANCE);
		return true;
	}
	return false;
}

// ------------------------------------------------------------------------------------
void MoveActor(int x, int y)
{
	if (!gMouseSphere) return;
	NxVec3 pos;
	ViewUnProject(x,y, gMouseDepth, pos);
	gMouseSphere->setGlobalPosition(pos);
}

// ------------------------------------------------------------------------------------
void LetGoSoftBody()
{
	if (gSelectedSoftBody && gSelectedSoftBodyVertex >= 0)
		gSelectedSoftBody->freeVertex(gSelectedSoftBodyVertex);
	gSelectedSoftBodyVertex = -1;
}

// ------------------------------------------------------------------------------------
bool PickSoftBody(int x, int y)
{
	NxRay ray; 
	ViewUnProject(x,y,0, ray.orig);
	ViewUnProject(x,y,1, ray.dir);
	ray.dir -= ray.orig; ray.dir.normalize();

	NxVec3 hit;
	NxU32 vertexId;
	gSelectedSoftBodyVertex = -1;
	NxReal distance = NX_MAX_REAL;

	NxSoftBody** softbodies = gScene->getSoftBodies();
	for (NxU32 i = 0; i < gScene->getNbSoftBodies(); i++) 
	{
		if (softbodies[i]->raycast(ray, hit, vertexId)) 
		{
			if(hit.magnitude() < distance)
			{
				gSelectedSoftBody = softbodies[i];
				gSelectedSoftBodyVertex = vertexId;
				int hitx, hity;
				ViewProject(hit, hitx, hity, gMouseDepth);
				distance = hit.magnitude();
			}
		}
	}

	return distance < NX_MAX_REAL;
}

// ------------------------------------------------------------------------------------
void MoveSoftBody(int x, int y)
{
	if (gSelectedSoftBody && gSelectedSoftBodyVertex >= 0) 
	{
		NxVec3 pos; 
		ViewUnProject(x,y, gMouseDepth, pos);
		gSelectedSoftBody->attachVertexToGlobalPosition(gSelectedSoftBodyVertex, pos);
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

	if (button == GLUT_RIGHT_BUTTON && state == GLUT_DOWN) {
		if (!PickActor(x,y))
			PickSoftBody(x,y);
	}
	if (state == GLUT_UP) {
		LetGoActor();
		LetGoSoftBody();
	}
}

void MotionCallback(int x, int y)
{
	int dx = mx - x;
	int dy = my - y;

	if (gMouseJoint)
	{
		MoveActor(x,y);
	}
	else if (gSelectedSoftBodyVertex >= 0) 
	{
		MoveSoftBody(x,y);
	}
	else 
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
	glClearColor(0.0f, 0.0f, 0.0f, 1.0);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_COLOR_MATERIAL);
	//glEnable(GL_CULL_FACE);
	glShadeModel(GL_SMOOTH);
	glLightModelf(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);

	// Setup lighting
	glEnable(GL_LIGHTING);
	float AmbientColor[]    = { 0.0f, 0.1f, 0.2f, 0.0f };         glLightfv(GL_LIGHT0, GL_AMBIENT, AmbientColor);
	float DiffuseColor[]    = { 0.2f, 0.2f, 0.2f, 0.0f };         glLightfv(GL_LIGHT0, GL_DIFFUSE, DiffuseColor);
	float SpecularColor[]   = { 0.5f, 0.5f, 0.5f, 0.0f };         glLightfv(GL_LIGHT0, GL_SPECULAR, SpecularColor);
	float Position[]        = { 100.0f, 100.0f, 10, 1.0f };  glLightfv(GL_LIGHT0, GL_POSITION, Position);
	glEnable(GL_LIGHT0);
}
NxActor* CreateOrientedCapsule(const NxMat34& globalPose, const NxReal height, const NxReal radius, const NxReal density)
{
	// Add a single-shape actor to the scene
	NxActorDesc actorDesc;
	NxBodyDesc bodyDesc;

	// The actor has one shape, a capsule
	NxCapsuleShapeDesc capsuleDesc;
	capsuleDesc.height = height;
	capsuleDesc.radius = radius;
	capsuleDesc.localPose.t = NxVec3(0,radius+(NxReal)0.5*height,0);
	actorDesc.shapes.pushBack(&capsuleDesc);

	if (density)
	{
		actorDesc.body = &bodyDesc;
		actorDesc.density = density;
	}
	else
	{
		actorDesc.body = NULL;
	}
	actorDesc.globalPose = globalPose;
	return gScene->createActor(actorDesc);	
}
// ------------------------------------------------------------------------------------
void SetupSoftWheelCarScene()
{
	sprintf(gTitleString, "SoftWheelCar Example");

	// Create the objects in the scene
	//gGroundPlane = CreateGroundPlane();

	NxSoftBodyDesc softBodyDesc;
	softBodyDesc.particleRadius = 0.2f;
	softBodyDesc.volumeStiffness = 1.0f;
	softBodyDesc.stretchingStiffness = 1.0f;
	softBodyDesc.friction = 1.0f;
	softBodyDesc.attachmentResponseCoefficient = 0.8f;


	if (gHardwareSoftBodySimulation)
		softBodyDesc.flags |= NX_SBF_HARDWARE;

	char *fileName = "wheel";	

	char tetFileName[256], objFileName[256], s[256];
	sprintf(tetFileName, "%s.tet", fileName);
	sprintf(objFileName, "%s.obj", fileName);

	MySoftBody *softBody;

	NxReal carHeight = 7.5f;
	NxReal stiffness = 0.9f;
	NxMat34 capsulePose = NxMat34(NxMat33(NX_IDENTITY_MATRIX), NxVec3(-4,carHeight,-5.0f));
	capsulePose.M.rotX(NxHalfPiF32);
	NxActor *caps1 = CreateOrientedCapsule(capsulePose, 7.1f, 1.3f, 1.0f);
	capsulePose.t = NxVec3(4,carHeight,-5.0f);
	NxActor *caps2 = CreateOrientedCapsule(capsulePose, 7.1f, 1.3f, 1.0f);

	ObjMesh *objMesh  = new ObjMesh();
	objMesh->loadFromObjFile(FindMediaFile(objFileName, s));

	NxMat33 rot;
	rot.rotX(NxPiF32);
	softBodyDesc.globalPose.t = NxVec3(4.0f, carHeight, 3.4f);
	softBodyDesc.globalPose.M = rot;
	softBodyDesc.stretchingStiffness = stiffness;
	softBody = new MySoftBody(gScene, softBodyDesc, FindMediaFile(tetFileName,s), objMesh);
	if (!softBody->getNxSoftBody())
	{
		printf("Error: Unable to create the SoftBody for the current scene.\n");
		delete softBody;
	} else
	{
		gObjMeshes.push_back(objMesh);

		softBody->getNxSoftBody()->attachToCollidingShapes(NX_SOFTBODY_ATTACHMENT_TWOWAY);
		gSoftBodies.push_back(softBody);

		softBodyDesc.globalPose.t = NxVec3(-4.0f ,carHeight, 3.4f);
		softBodyDesc.globalPose.M = rot;
		softBodyDesc.stretchingStiffness = stiffness;
		softBody = new MySoftBody(gScene, softBodyDesc, FindMediaFile(tetFileName,s), objMesh);
		softBody->getNxSoftBody()->attachToCollidingShapes(NX_SOFTBODY_ATTACHMENT_TWOWAY);
		gSoftBodies.push_back(softBody);

		softBodyDesc.globalPose.t = NxVec3(4.0f, carHeight, -3.4f);
		softBodyDesc.globalPose.M.id();
		softBodyDesc.stretchingStiffness = stiffness;
		softBody = new MySoftBody(gScene, softBodyDesc, FindMediaFile(tetFileName,s), objMesh);
		softBody->getNxSoftBody()->attachToCollidingShapes(NX_SOFTBODY_ATTACHMENT_TWOWAY);
		gSoftBodies.push_back(softBody);

		softBodyDesc.globalPose.t = NxVec3(-4.0f, carHeight, -3.4f);
		softBodyDesc.globalPose.M.id();
		softBodyDesc.stretchingStiffness = stiffness;
		softBody = new MySoftBody(gScene, softBodyDesc, FindMediaFile(tetFileName,s), objMesh);
		softBody->getNxSoftBody()->attachToCollidingShapes(NX_SOFTBODY_ATTACHMENT_TWOWAY);
		gSoftBodies.push_back(softBody);
		NxActor *box = CreateBox(NxVec3(0,carHeight,0), NxVec3(4.6f, 0.5f, 1.0f), 1.0f);
		CreateRevoluteJoint(box, caps1, NxVec3(-4,carHeight,-3.5f), NxVec3(0,0,1));
		CreateRevoluteJoint(box, caps2, NxVec3( 4,carHeight,-3.5f), NxVec3(0,0,1));
		NxActor *ground = CreateBox(NxVec3(0,1,0), NxVec3(10.0f, 1.0f, 8.0f), 0.0f);
		rot.rotZ(-0.1f);
		ground->setGlobalOrientation(rot);
	}

	// set camera position and direction
	gCameraPos.set(-17.0f, 20.0f, 20.0f);
	gCameraForward.set(1.0f, -0.8f, -0.90f);
	gCameraForward.normalize();
	gCameraForward.normalize();
}

void InitializeHUD()
{
	bHardwareScene = (gScene->getSimType() == NX_SIMULATION_HW);

	hud.Clear();

	// Add hardware/software to HUD
	/*if (bHardwareScene)
		hud.AddDisplayString("Master Scene: Hardware", 0.64f, 0.92f);
	else
		hud.AddDisplayString("Master Scene: Software", 0.64f, 0.92f);*/

	if (gHardwareSoftBodySimulation)
		hud.AddDisplayString("SoftBody Sim: Hardware", 0.64f, 0.89f);
	else
		hud.AddDisplayString("SoftBody Sim: Software", 0.64f, 0.89f);
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
	NxHWVersion hwCheck = gPhysicsSDK->getHWVersion();
	if (hwCheck == NX_HW_VERSION_NONE) 
	{
		printf("\nWarning: Unable to find a PhysX card, or PhysX card used by other application.");
		printf("\nThe soft bodies will be simulated in software.\n\n");
		gHardwareSoftBodySimulation = false;
		bHardwareScene = false;
	}


	// The settings for the VRD host and port are found in SampleCommonCode/SamplesVRDSettings.h
	if (gPhysicsSDK->getFoundationSDK().getRemoteDebugger() && !gPhysicsSDK->getFoundationSDK().getRemoteDebugger()->isConnected())
		gPhysicsSDK->getFoundationSDK().getRemoteDebugger()->connect("localhost", NX_DBG_DEFAULT_PORT, NX_DBG_EVENTMASK_EVERYTHING);

	NxInitCooking();

	// Set the physics parameters
	gPhysicsSDK->setParameter(NX_SKIN_WIDTH, 0.01);

	// Set the debug visualization parameters
	gPhysicsSDK->setParameter(NX_VISUALIZATION_SCALE, 1);
	gPhysicsSDK->setParameter(NX_VISUALIZE_COLLISION_SHAPES, 1);
	gPhysicsSDK->setParameter(NX_VISUALIZE_ACTOR_AXES, 1);
	gPhysicsSDK->setParameter(NX_VISUALIZE_JOINT_LIMITS, 1);
	gPhysicsSDK->setParameter(NX_VISUALIZE_JOINT_LOCAL_AXES, 1);

	// Create the scenes
	NxSceneDesc sceneDesc;
	sceneDesc.gravity = gDefaultGravity;
	sceneDesc.groundPlane = true;
	if(bHardwareScene)
		sceneDesc.simType = NX_SIMULATION_HW;
	else
		sceneDesc.simType = NX_SIMULATION_SW;

	gScene = gPhysicsSDK->createScene(sceneDesc);

	if(!gScene)
	{ 
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

	SetupSoftWheelCarScene();

	if (gSoftBodies.size() > 0)
		gSelectedSoftBody = gSoftBodies[0]->getNxSoftBody();
	else
		gSelectedSoftBody = NULL;

	if (gScene->getNbActors() > 0)
		gSelectedActor = *gScene->getActors();
	else
		gSelectedActor = NULL;

	gScene->setTiming(1.0/60.0,1);

	// Initialize HUD
	InitializeHUD();
	InitializeSpecialHUD();

	// Get the current time
	getElapsedTime();

	// Start the first frame of the simulation
	if (gScene)  StartPhysics();
}
int main(int argc, char** argv)
{
	PrintControls();
	InitGlut(argc, argv, "Lesson 1105: SoftWheelCar");
//	SetDataDirectory();
	InitNx();
	glutMainLoop();
	ReleaseNx();
	return 0;
}

void ReleaseNx()
{
	if (gScene)
	{
		GetPhysicsResults();  // Make sure to fetchResults() before shutting down
		for (MySoftBody** SoftBody = gSoftBodies.begin(); SoftBody != gSoftBodies.end(); SoftBody++)
			delete *SoftBody;
		gSoftBodies.clear();
		gPhysicsSDK->releaseScene(*gScene);
	}
	NxCloseCooking();
	if (gPhysicsSDK)  gPhysicsSDK->release();
	NX_DELETE_SINGLE(gAllocator);
}

void ResetNx()
{
	LetGoActor();
	LetGoSoftBody();
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
	while (!gScene->fetchResults(NX_RIGID_BODY_FINISHED, false));
}
