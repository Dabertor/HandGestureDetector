/*******************************************************************************
*                                                                              *
*   PrimeSense NiTE 2.0 - Hand Viewer Sample                                   *
*   Copyright (C) 2012 PrimeSense Ltd.                                         *
*                                                                              *
*******************************************************************************/

#include <map>
#include "Viewer.h"
#include <Windows.h>

#if (ONI_PLATFORM == ONI_PLATFORM_MACOSX)
        #include <GLUT/glut.h>
#else
        #include <GL/glut.h>
#endif

#include "HistoryBuffer.h"
#include <NiteSampleUtilities.h>

#define GL_WIN_SIZE_X	700
#define GL_WIN_SIZE_Y	700
#define TEXTURE_SIZE	512

#define DEFAULT_DISPLAY_MODE	DISPLAY_MODE_DEPTH

#define MIN_NUM_CHUNKS(data_size, chunk_size)	((((data_size)-1) / (chunk_size) + 1))
#define MIN_CHUNKS_SIZE(data_size, chunk_size)	(MIN_NUM_CHUNKS(data_size, chunk_size) * (chunk_size))

SampleViewer* SampleViewer::ms_self = NULL;

std::map<int, HistoryBuffer<20> *> g_histories;

bool g_drawDepth = true;
bool g_smoothing = false;
bool g_drawFrameId = false;

//meter array de X e Y, de posición del ratón para la media y un contador.
#define N 5

int manos[5] = {};

float sumaX = 0;					//suma del valor en X
float sumaY = 0;					//suma del valor en Y
int contador = 0;					//llevar la cuenta del número de valores que leemos y estamos sumando en X e Y 
int contadorClick = 0;
int contadorManos = 0;
float valorAnteriorx = 50.0;
float valorAnteriory = 50.0;
float ultimaReferenciax = 0;
float ultimaReferenciay = 0;

float sumaXM2 = 0;
float sumaYM2 = 0;
int contadorMano2 = 0;
int contadorClickM2 = 0;
float valorAnteriorxM2 = 50.0;
float valorAnterioryM2 = 50.0;
float ultimaReferenciaxM2 = 0;
float ultimaReferenciayM2 = 0;

int g_nXRes = 0, g_nYRes = 0;

void SampleViewer::glutIdle()
{
	glutPostRedisplay();
}
void SampleViewer::glutDisplay()
{
	SampleViewer::ms_self->Display();
}
void SampleViewer::glutKeyboard(unsigned char key, int x, int y)
{
	SampleViewer::ms_self->OnKey(key, x, y);
}

SampleViewer::SampleViewer(const char* strSampleName)
{
	ms_self = this;
	strncpy(m_strSampleName, strSampleName, ONI_MAX_STR);
	m_pHandTracker = new nite::HandTracker;
}
SampleViewer::~SampleViewer()
{
	Finalize();

	delete[] m_pTexMap;

	ms_self = NULL;
}

void SampleViewer::Finalize()
{
	delete m_pHandTracker;
	nite::NiTE::shutdown();
	openni::OpenNI::shutdown();
}

openni::Status SampleViewer::Init(int argc, char **argv)
{
	m_pTexMap = NULL;

	openni::OpenNI::initialize();

	const char* deviceUri = openni::ANY_DEVICE;
	for (int i = 1; i < argc-1; ++i)
	{
		if (strcmp(argv[i], "-device") == 0)
		{
			deviceUri = argv[i+1];
			break;
		}
	}

	openni::Status rc = m_device.open(deviceUri);
	if (rc != openni::STATUS_OK)
	{
		printf("Open Device failed:\n%s\n", openni::OpenNI::getExtendedError());
		return rc;
	}

	nite::NiTE::initialize();

	if (m_pHandTracker->create(&m_device) != nite::STATUS_OK)
	{
		return openni::STATUS_ERROR;
	}

	m_pHandTracker->startGestureDetection(nite::GESTURE_WAVE);
	m_pHandTracker->startGestureDetection(nite::GESTURE_CLICK);

	return InitOpenGL(argc, argv);

}
openni::Status SampleViewer::Run()	//Does not return
{
	glutMainLoop();

	return openni::STATUS_OK;
}

float Colors[][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {1, 1, 1}};
int colorCount = 3;

void DrawHistory(nite::HandTracker* pHandTracker, int id, HistoryBuffer<20>* pHistory)
{
	glColor3f(Colors[id % colorCount][0], Colors[id % colorCount][1], Colors[id % colorCount][2]);
	float coordinates[60] = {0};
	float factorX = GL_WIN_SIZE_X / (float)g_nXRes;
	float factorY = GL_WIN_SIZE_Y / (float)g_nYRes;

	for (int i = 0; i < pHistory->GetSize(); ++i)
	{
		const nite::Point3f& position = pHistory->operator[](i);
		
		pHandTracker->convertHandCoordinatesToDepth(position.x, position.y, position.z, &coordinates[i*3], &coordinates[i*3+1]);

		coordinates[i*3]   *= factorX;
		coordinates[i*3+1] *= factorY;
	}

	glPointSize(8);
	glVertexPointer(3, GL_FLOAT, 0, coordinates);
	glDrawArrays(GL_LINE_STRIP, 0, pHistory->GetSize());

	glPointSize(12);
	glVertexPointer(3, GL_FLOAT, 0, coordinates);
	glDrawArrays(GL_POINTS, 0, 1);

}

#ifndef USE_GLES
void glPrintString(void *font, const char *str)
{
	int i,l = (int)strlen(str);

	for(i=0; i<l; i++)
	{   
		glutBitmapCharacter(font,*str++);
	}   
}
#endif
void DrawFrameId(int frameId)
{
	char buffer[80] = "";
	sprintf(buffer, "%d", frameId);
	glColor3f(1.0f, 0.0f, 0.0f);
	glRasterPos2i(20, 20);
	glPrintString(GLUT_BITMAP_HELVETICA_18, buffer);
}


void SampleViewer::Display()
{
	nite::HandTrackerFrameRef handFrame;
	openni::VideoFrameRef depthFrame;
	nite::Status rc = m_pHandTracker->readFrame(&handFrame);
	if (rc != nite::STATUS_OK)
	{
		printf("GetNextData failed\n");
		return;
	}

	depthFrame = handFrame.getDepthFrame();

	if (m_pTexMap == NULL)
	{
		// Texture map init
		m_nTexMapX = MIN_CHUNKS_SIZE(depthFrame.getVideoMode().getResolutionX(), TEXTURE_SIZE);
		m_nTexMapY = MIN_CHUNKS_SIZE(depthFrame.getVideoMode().getResolutionY(), TEXTURE_SIZE);
		m_pTexMap = new openni::RGB888Pixel[m_nTexMapX * m_nTexMapY];
	}


	glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrtho(0, GL_WIN_SIZE_X, GL_WIN_SIZE_Y, 0, -1.0, 1.0);

	if (depthFrame.isValid())
	{
		calculateHistogram(m_pDepthHist, MAX_DEPTH, depthFrame);
	}

	memset(m_pTexMap, 0, m_nTexMapX*m_nTexMapY*sizeof(openni::RGB888Pixel));

	float factor[3] = {1, 1, 1};
	// check if we need to draw depth frame to texture
	if (depthFrame.isValid() && g_drawDepth)
	{
		const openni::DepthPixel* pDepthRow = (const openni::DepthPixel*)depthFrame.getData();
		openni::RGB888Pixel* pTexRow = m_pTexMap + depthFrame.getCropOriginY() * m_nTexMapX;
		int rowSize = depthFrame.getStrideInBytes() / sizeof(openni::DepthPixel);

		for (int y = 0; y < depthFrame.getHeight(); ++y)
		{
			const openni::DepthPixel* pDepth = pDepthRow;
			openni::RGB888Pixel* pTex = pTexRow + depthFrame.getCropOriginX();

			for (int x = 0; x < depthFrame.getWidth(); ++x, ++pDepth, ++pTex)
			{
				if (*pDepth != 0)
				{
					factor[0] = Colors[colorCount][0];
					factor[1] = Colors[colorCount][1];
					factor[2] = Colors[colorCount][2];

					int nHistValue = m_pDepthHist[*pDepth];
					pTex->r = nHistValue*factor[0];
					pTex->g = nHistValue*factor[1];
					pTex->b = nHistValue*factor[2];

					factor[0] = factor[1] = factor[2] = 1;
				}
			}

			pDepthRow += rowSize;
			pTexRow += m_nTexMapX;
		}
	}

	glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, m_nTexMapX, m_nTexMapY, 0, GL_RGB, GL_UNSIGNED_BYTE, m_pTexMap);

	// Display the OpenGL texture map
	glColor4f(1,1,1,1);

	glEnable(GL_TEXTURE_2D);
	glBegin(GL_QUADS);

	g_nXRes = depthFrame.getVideoMode().getResolutionX();
	g_nYRes = depthFrame.getVideoMode().getResolutionY();

	// upper left
	glTexCoord2f(0, 0);
	glVertex2f(0, 0);
	// upper right
	glTexCoord2f((float)g_nXRes/(float)m_nTexMapX, 0);
	glVertex2f(GL_WIN_SIZE_X, 0);
	// bottom right
	glTexCoord2f((float)g_nXRes/(float)m_nTexMapX, (float)g_nYRes/(float)m_nTexMapY);
	glVertex2f(GL_WIN_SIZE_X, GL_WIN_SIZE_Y);
	// bottom left
	glTexCoord2f(0, (float)g_nYRes/(float)m_nTexMapY);
	glVertex2f(0, GL_WIN_SIZE_Y);

	glEnd();
	glDisable(GL_TEXTURE_2D);

	const nite::Array<nite::GestureData>& gestures = handFrame.getGestures();
	for (int i = 0; i < gestures.getSize(); ++i)
	{
		if (gestures[i].isComplete())
		{
			const nite::Point3f& position = gestures[i].getCurrentPosition();
			printf("Gesture %d at (%f,%f,%f)\n", gestures[i].getType(), position.x, position.y, position.z);

			nite::HandId newId;
			m_pHandTracker->startHandTracking(gestures[i].getCurrentPosition(), &newId);
		}
	}

	const nite::Array<nite::HandData>& hands= handFrame.getHands();
	for (int i = 0; i < hands.getSize(); ++i)
	{
		const nite::HandData& user = hands[i];

		if (!user.isTracking())
		{
			printf("Lost hand %d\n", user.getId());
			
			//si la mano se ha perdido, vamos a borrarla del array que la identifica
			for (int i = 0; i < sizeof(manos); i++) {
				
				if (manos[i] == user.getId()) {
					manos[i] = NULL;
					break;
				}
			}
			nite::HandId id = user.getId();
			HistoryBuffer<20>* pHistory = g_histories[id];
			g_histories.erase(g_histories.find(id));
			delete pHistory;
		}
		else
		{
			if (user.isNew())
			{
				printf("Found hand %d\n", user.getId());

				//si encontramos una mano, la añadimos al espacio en nulo que hay
				for (int i = 0; i < sizeof(manos); i++) {
					if(manos[i]==NULL){
						manos[i] = user.getId();
						break;
					}
				}
				g_histories[user.getId()] = new HistoryBuffer<20>;
			}
			// Add to history
			HistoryBuffer<20>* pHistory = g_histories[user.getId()];

			//si hay una mano encontrada en la posición zero(por lo general la primera encontrada)
			if (manos[0]==user.getId()) {
				
				//le añandimos un puntero
				pHistory->AddPoint(user.getPosition());

				//sumamos los valores de posición en el eje X e Y
				sumaX += user.getPosition().x;
				sumaY += user.getPosition().y;
				contador++;

				//si hemos sumado ya 5 valores de X e Y
				if (contador == N) {

					//reseteamos el contador y calculamos la media 
					contador = 0;
					sumaX = sumaX / N;
					sumaY = sumaY / N;

					//acumulamos la media y guardamos la ultima referencia
					valorAnteriorx += sumaX;
					valorAnteriory += sumaY;
					ultimaReferenciax = sumaX;
					ultimaReferenciay = sumaY;
					contadorClick++;

					//hacemos que el cursor se mueva al vamor medio calculado (multiplicados para poder moverse por toda la pantalla)
					SetCursorPos((int)sumaX * 4, (int)sumaY*(-5));

					//reseteamos suma
					sumaX = 0;
					sumaY = 0;
				}

				//si ya tenemos la suma de 4 medias
				if (contadorClick == 4) {

					//comprobamos si los valores medios distan de la ultima referencia, es decir, si tenemos la mano quieta
					if (((valorAnteriorx / 4) - ultimaReferenciax) <= 1 && ((valorAnteriory / 4) - ultimaReferenciay) <= 1 && ((valorAnteriorx / 4) - ultimaReferenciax) >= -1 && ((valorAnteriory / 4) - ultimaReferenciay) >= -21) {

						//llamamos a la funcion de windows.h para hacer un click y reseteamos valores
						mouse_event(MOUSEEVENTF_LEFTDOWN, (int)ultimaReferenciax * 4, (int)ultimaReferenciay*(-5), 0, 0);
						mouse_event(MOUSEEVENTF_LEFTUP, (int)ultimaReferenciax * 4, (int)ultimaReferenciay*(-5), 0, 0);
						valorAnteriorx = 0;
						valorAnteriory = 0;
						ultimaReferenciax = 0;
						ultimaReferenciay = 0;
						contadorClick = 0;
					}
					else {

						//si la mano no esta quieta, reseteamos valores
						valorAnteriorx = 0;
						valorAnteriory = 0;
						ultimaReferenciax = 0;
						ultimaReferenciay = 0;
						contadorClick = 0;
					}
				}
			}
			
			//si hay una mano en la posición 1 de manos(por lo general la segunda mano)
			if (manos[1]==user.getId()) {

				//Obtengo los datos de la mano y le asigno un puntero
				HistoryBuffer<20>* pHistory = g_histories[user.getId()];
				pHistory->AddPoint(user.getPosition());

				//sumamos los valores de posición en el eje X e Y de la segunda mano
				sumaXM2 += user.getPosition().x;
				sumaYM2 += user.getPosition().y;
				contadorMano2++;

				//si hemos sumado ya 5 valores de X e Y
				if (contadorMano2 == N) {

					//reseteamos el contador y calculamos la media
					contadorMano2 = 0;
					sumaXM2 = sumaXM2 / N;
					sumaYM2 = sumaYM2 / N;

					//acumulamos la media y guardamos la ultima referencia
					valorAnteriorxM2 += sumaXM2;
					valorAnterioryM2 += sumaYM2;
					ultimaReferenciaxM2 = sumaXM2;
					ultimaReferenciayM2 = sumaYM2;
					contadorClickM2++;

					//reseteamos suma
					sumaXM2 = 0;
					sumaYM2 = 0;
				}

				//si ya tenemos la suma de 4 medias
				if (contadorClickM2 == 4) {
					//printf("Valor de la resta valor anterior y ultima referencia es %f\n", ((valorAnteriorxM2 / 4) - ultimaReferenciaxM2));

					//comprobamos si los valores medios distan de la ultima referencia para saber si hemos movido
					if (((valorAnteriorxM2 / 4) - ultimaReferenciaxM2)>=60 || ((valorAnteriorxM2 / 4) - ultimaReferenciaxM2)<= -60) {

						//si hemos movido suficiente la mano, realizaremos un click derecho típico de ratón
						mouse_event(MOUSEEVENTF_RIGHTDOWN, (int)ultimaReferenciax * 4, (int)ultimaReferenciay*(-5), 0, 0);
						mouse_event(MOUSEEVENTF_RIGHTUP, (int)ultimaReferenciax * 4, (int)ultimaReferenciay*(-5), 0, 0);

						//reeteamos valores
						valorAnteriorxM2 = 0;
						valorAnterioryM2 = 0;
						ultimaReferenciaxM2 = 0;
						ultimaReferenciayM2 = 0;
						contadorClickM2 = 0;
					}
					else {

						//si tenemos la mano quieta, reseteamos valores
						valorAnteriorxM2 = 0;
						valorAnterioryM2 = 0;
						ultimaReferenciaxM2 = 0;
						ultimaReferenciayM2 = 0;
						contadorClickM2 = 0;
					}
				}

				//si en algún momento hacemos algún gesto(definidos por Nite, como pulsar) entonces hacemos doble click
				for (int i = 0; i < gestures.getSize(); ++i)
				{
					if (gestures[i].isComplete())
					{
						mouse_event(MOUSEEVENTF_LEFTDOWN, (int)ultimaReferenciax * 4, (int)ultimaReferenciay*(-5), 0, 0);
						mouse_event(MOUSEEVENTF_LEFTUP, (int)ultimaReferenciax * 4, (int)ultimaReferenciay*(-5), 0, 0);
						mouse_event(MOUSEEVENTF_LEFTDOWN, (int)ultimaReferenciax * 4, (int)ultimaReferenciay*(-5), 0, 0);
						mouse_event(MOUSEEVENTF_LEFTUP, (int)ultimaReferenciax * 4, (int)ultimaReferenciay*(-5), 0, 0);
						break;
					}
				}
			}

			// Draw history
			DrawHistory(m_pHandTracker, user.getId(), pHistory);
		}
	}

	if (g_drawFrameId)
	{
		DrawFrameId(handFrame.getFrameIndex());
	}

	// Swap the OpenGL display buffers
	glutSwapBuffers();

}

void SampleViewer::OnKey(unsigned char key, int /*x*/, int /*y*/)
{
	switch (key)
	{
	case 27:
		Finalize();
		exit (1);
	case 'd':
		g_drawDepth = !g_drawDepth;
		break;
	case 's':
		if (g_smoothing)
		{
			// Turn off smoothing
			m_pHandTracker->setSmoothingFactor(0);
			g_smoothing = FALSE;
		}
		else
		{
			m_pHandTracker->setSmoothingFactor(0.1);
			g_smoothing = TRUE;
		}
		break;
	case 'f':
		// Draw frame ID
		g_drawFrameId = !g_drawFrameId;
		break;
	}

}

openni::Status SampleViewer::InitOpenGL(int argc, char **argv)
{
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);
	glutInitWindowSize(GL_WIN_SIZE_X, GL_WIN_SIZE_Y);
	glutCreateWindow (m_strSampleName);
	// 	glutFullScreen();
	glutSetCursor(GLUT_CURSOR_NONE);

	InitOpenGLHooks();

	glDisable(GL_DEPTH_TEST);
	glEnable(GL_TEXTURE_2D);

	glEnableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);

	return openni::STATUS_OK;

}
void SampleViewer::InitOpenGLHooks()
{
	glutKeyboardFunc(glutKeyboard);
	glutDisplayFunc(glutDisplay);
	glutIdleFunc(glutIdle);
}
