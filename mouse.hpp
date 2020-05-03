/* mouse.h
 *
 * Interact with the scene using the mouse. Originally called "traqueboule.h"
 * for reasons lost to the passings of time -- neverhteless, this is the 
 * reason for the "tb"/"tb_" prefixes in this file.
 *
 * We've translated the original french comments to english, but otherwise
 * left the file as-is.
 */
#ifndef MOUSE_H
#define MOUSE_H

#ifdef WIN32
#	include <windows.h>
#endif

#include <cmath>
#include <cstdio>

#include <GL/glut.h>

#include "vec3D.hpp"

static const float speedfact = 0.2f;

/** Display function */
void display();

/** View transformation */
GLdouble tb_matrix[16] =   { 1,0,0,0,
                             0,1,0,0,
                             0,0,1,0,
                             0,0,0,1  };
GLdouble tb_inverse[16] =  { 1,0,0,0,
                             0,1,0,0,
                             0,0,1,0,
                             0,0,0,1  };

/** Mouse variables */
int tb_oldX, tb_oldY, tb_rotateXY=0, tb_translateXY=0, tb_moveZ=0;


/** Initialize the current transformation matrix with the initial viewpoint */
namespace detail_matrix
{
	void inverse( const double *m, double *p )
	{
		{   // Transpose rotation
			for(int i=0; i<3; ++i )
				for(int j=0; j<3; ++j )
					p[i+4*j] = m[j+4*i];
		}

		{   // Translation
			double q[3]={0,0,0};
			for(int i=0; i<3; ++i ){
				for(int j=0; j<3; ++j )
					q[i] -= p[i+4*j]*m[12+j];
			}
			p[12]=q[0]; p[13]=q[1]; p[14]=q[2];
		}

		// Last line
		p[3] = p[7] = p[11] = 0; p[15] = 1;
	}
}

void tbInitTransform()
{
    glGetDoublev( GL_MODELVIEW_MATRIX, tb_matrix );
    detail_matrix::inverse( tb_matrix, tb_inverse );
}

/** Apply view transformation */
void tbVisuTransform()
{
    glMultMatrixd( tb_matrix );
};

/** Print help */
void tbHelp()
{
    printf("Left button: turn in XY,\n");
    printf("Right button: translate in XY,\n");
    printf("Middle button: move along Z.\n");
}

/** Process mouse buttons */
void tbMouseFunc( int button, int state, int x, int y )
{
    /* Left button press */
    if( button==GLUT_LEFT_BUTTON && state==GLUT_DOWN )
    {
        tb_rotateXY = 1;
        tb_oldX = x;
        tb_oldY = y;
    }
    /* Left button release */
    else if( button==GLUT_LEFT_BUTTON && state==GLUT_UP )
    {
        tb_rotateXY = 0;
    }
    /* Middle button press */
    if( button==GLUT_MIDDLE_BUTTON && state==GLUT_DOWN )
    {
        tb_moveZ = 1;
        tb_oldX = x;
        tb_oldY = y;
    }
    /* Middle button release */
    else if( button==GLUT_MIDDLE_BUTTON && state==GLUT_UP )
    {
        tb_moveZ = 0;
    }
    /* Right button press */
    else if( button==GLUT_RIGHT_BUTTON && state==GLUT_DOWN )
    {
        tb_translateXY = 1;
        tb_oldX = x;
        tb_oldY = y;
    }
    /* Right button release */
    else if( button==GLUT_RIGHT_BUTTON && state==GLUT_UP )
    {
        tb_translateXY = 0;
    }
}


/** Process mouse motion for translation */
void tbMotionFunc( int x, int y )
{
    double dx,dy,nrm, tx,ty,tz;

    if( tb_rotateXY || tb_translateXY || tb_moveZ )
    {
        /* motion amount */
        dx = x - tb_oldX;
        dy = tb_oldY - y; /* Vertical axis direction is inverted */

        if( tb_rotateXY )
        {
            tx = tb_matrix[12];
            tb_matrix[12]=0;
            ty = tb_matrix[13];
            tb_matrix[13]=0;
            tz = tb_matrix[14];
            tb_matrix[14]=0;

            nrm = sqrt( dx*dx+dy*dy+dx*dx+dy*dy )*speedfact;
            glLoadIdentity();
            glRotated( nrm, -dy, dx, 0 );/* Axis perpendicular to motion */
            glMultMatrixd( tb_matrix );
            glGetDoublev( GL_MODELVIEW_MATRIX, tb_matrix );

            tb_matrix[12] = tx;
            tb_matrix[13] = ty;
            tb_matrix[14] = tz;
        }
        else if( tb_translateXY )
        {
            tb_matrix[12] += dx/100.0*speedfact;
            tb_matrix[13] += dy/100.0*speedfact;
        }
        else if( fabs(dx)>fabs(dy) )
        { // rotation z
            tx = tb_matrix[12];
            tb_matrix[12]=0;
            ty = tb_matrix[13];
            tb_matrix[13]=0;
            tz = tb_matrix[14];
            tb_matrix[14]=0;

            glLoadIdentity();
            glRotated( dx, 0,0,-1 );/* Axis perpendicular to the screen */
            glMultMatrixd( tb_matrix );
            glGetDoublev( GL_MODELVIEW_MATRIX, tb_matrix );

            tb_matrix[12] = tx;
            tb_matrix[13] = ty;
            tb_matrix[14] = tz;
        }
        else if( fabs(dy)>fabs(dx) )
        {
            tb_matrix[14] -= dy/100.0*speedfact;
        }
        tb_oldX = x;
        tb_oldY = y;
        detail_matrix::inverse( tb_matrix, tb_inverse );
        glutPostRedisplay();
    }
}

/** Process mouse motion for rotation */
void tbRotate( double angle, double x, double y, double z )
{
    double tx,ty,tz;

    tx = tb_matrix[12];
    tb_matrix[12]=0;
    ty = tb_matrix[13];
    tb_matrix[13]=0;
    tz = tb_matrix[14];
    tb_matrix[14]=0;

    glLoadIdentity();
    glRotated( angle, x, y, z );
    glMultMatrixd( tb_matrix );
    glGetDoublev( GL_MODELVIEW_MATRIX, tb_matrix );

    tb_matrix[12] = tx;
    tb_matrix[13] = ty;
    tb_matrix[14] = tz;

    detail_matrix::inverse( tb_matrix, tb_inverse );
    glutPostRedisplay();
}

namespace detail_matrix
{
	/// Projection q=Mp
	void project( const GLdouble* m, const GLdouble* p, GLdouble* q )
	{
		for( int i=0; i<3; ++i ){
			q[i] = 0;
			for( int j=0; j<4; ++j )
				q[i] += m[i+4*j]*p[j];
		}
		q[3] = p[3];
	}
}

/// Projection to world space
void tbProject( const GLdouble *m, const GLdouble* p, GLdouble* q )
{
    double pp[4];
    //cout<<"tb, matrix: "; printMatrix(tb_matrix); cout<<endl;
    //cout<<"tb, inverse: "; printMatrix(tb_inverse); cout<<endl;
    detail_matrix::project( m, p, pp );
    //cout<<"proj: "<<pp[0]<<", "<<pp[1]<<", "<<pp[2]<<", "<<pp[3]<<endl;
    detail_matrix::project( tb_inverse, pp, q );
    //cout<<"projRep: "<<q[0]<<", "<<q[1]<<", "<<q[2]<<", "<<q[3]<<endl;
}

void tbProject( const GLdouble* p, GLdouble* q )
{
    //cout<<"proj: "<<pp[0]<<", "<<pp[1]<<", "<<pp[2]<<", "<<pp[3]<<endl;
    detail_matrix::project( tb_inverse, p, q );
    //cout<<"projRep: "<<q[0]<<", "<<q[1]<<", "<<q[2]<<", "<<q[3]<<endl;
}
Vec3Df getCameraPosition()
{
	const GLdouble p[]={0,0,0,1};
	GLdouble LightP[4];
	tbProject(p, LightP);
	Vec3Df LightPos;
	LightPos[0] = (float)LightP[0];
	LightPos[1] = (float)LightP[1];
	LightPos[2] = (float)LightP[2];
	return LightPos;
}
Vec3Df getWorldPositionOfPixel(unsigned int px, unsigned int py)
{

	double mv[16];
	double pr[16];
	int vp[4];
	glGetDoublev(GL_MODELVIEW_MATRIX,mv);
	glGetDoublev(GL_PROJECTION_MATRIX,pr);
	glGetIntegerv(GL_VIEWPORT,vp);

	double x,y,z;
	gluUnProject(double(px),double(py),0,mv,pr,vp,&x,&y,&z);



	return Vec3Df((float)x, (float)y, (float)z);
}
#endif
