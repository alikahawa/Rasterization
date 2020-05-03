#pragma once

#define GLEW_STATIC 1
#include <GL/glew.h>

#include "vec3D.hpp"
#include "volume.hpp"

bool project_initialize();

void project_draw_window( Vec3Df const& aCameraFwd, Vec3Df const& aCameraUp, Vec3Df const& aCameraPos);

void project_interact_mouse_wheel( bool aUp );
void project_on_key_press( int, Vec3Df const& aCameraPos );

Vec3Df gradient( Volume const&, int, int, int );

