#include "project.hpp"

#include <chrono>
#include <cstdio>
#include <vector>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <random>
#include <ppl.h>


/* The following defines the different visualization modes that you will
 * implement (several modes towards the end are optional, though).
 */
enum class EVisualizeMode {
	// "none" is the default mode that you see when you first start the
	// application. It just draws the bounding box around the volume.
	none,

	// The following are the visualization modes that you will implement in the
	// different tasks.
	solidPoints,               // Task 1.
	additivePoints,            // Task 2
	colorAlphaPoints,          // Task 3 (3a + 3b)
	phongPoints,               // Task 4
	selectedPointsOnly,        // Task 5
	enhanceSelectedPoints,     // Optional Task 1
	billboards,                // Optional Task 2a
	billboardsWithLOD,         // Optional Task 2b
	drawAsArray,               // Optional Task 3a
	drawAsArrayFromVRAM        // Optional Task 3b, 3c, and 3d
};

// gVisualizeMode defines the currently selected visualization mode. See
// project_on_key_press(), which defines a few default keys to switch between
// the different modes. By default, you can do so with the 0-9 keys.
EVisualizeMode gVisualizeMode = EVisualizeMode::none;

// gVolume contains the currently loaded volume. See volume.hpp for an overview
// on how to access the data in the Volume class.
Volume gVolume(0, 0, 0);

// gVolumeLargestDimension is set to the largest dimensions of the volume. The
// volumes are not cubic...
std::size_t gVolumeLargestDimension = 0;

// gLightPosition contains the light position. This will be used for
// EVisualizeMode::phongPoints and latter visualization modes. You can change
// the light position to the current camera position by pressing 'l'. Return it
// to the default position with 'k'. See project_on_key_press() for details.
Vec3Df gLightPosition(2.f, 2.f, 0.f);

// gLightChanged is set to true when the light has changed. Use this to
// determine whether or not you need to update the data when the visualization
// mode is EVisualizeMode::drawAsArray or EVisualizeMode::drawAsArrayFromVRAM.
bool gLightChanged = false;

// The following defines the three types of regions by which you are asked to
// restrict the visible parts of the volume. The currently selected region 
// type is specified by 'gSelectiveRegionType' below.
enum class ESelectiveRegionType {
	sphere,
	cube,
	slab
};

// gSelectiveRegionType is the currently selected region type by which to
// restrict which parts of the volume should be drawn in Task 5. You can
// switch the volume type using the keys 't' (sphere), 'g' (cube) and 'b'
// (slab).  See project_on_key_press() for details.
ESelectiveRegionType gSelectiveRegionType = ESelectiveRegionType::cube;

// TODO: if you need store other information between frames or between the
// different functions, you may add it here:
enum class AXIS { X, Y, Z };
enum class DIRECTION { POS, NEG };
enum class TRANSFERTYPE { BONSAI, BACKPACK };
enum class BILLBOARDSHAPE { QUAD, TRIANGLEFAN };

TRANSFERTYPE global_ttype = TRANSFERTYPE::BONSAI;

// storage of all data-files
std::vector<const char*> global_files = {};
int global_files_idx = 0;

// position where the drawing of the shape begins
Vec3Df global_position = Vec3Df();

// cube
float global_width = 1.0f;
float global_height = 1.0f;
float global_depth = 1.0f;

// sphere
float global_radius = 1.0f;

// slab
AXIS global_slab_axis = AXIS::X;
float global_slab_length = 0.5f;

// billboards
BILLBOARDSHAPE bbShape = BILLBOARDSHAPE::QUAD;
Volume gVolumeSmall(0, 0, 0);

std::vector<Volume> vols = std::vector<Volume>();
int global_vols_idx = 0;

// drawAsArray
std::vector<float> gDrawPositions = std::vector<float>();
std::vector<float> gDrawColors = std::vector<float>();
DIRECTION current_dir = DIRECTION::POS;
GLuint gColorVBO;
GLuint gPositionVBO;

// declaration of functions beforehand:
bool checkIntersection(float X, float Y, float Z);
float linearInterPolation(float x, float x1, float x2, float q00, float q01);
float triLinearInterpolation(float x, float y, float z, float p000, float p001, float p010, float p011,
	float p100, float p101, float p110, float p111, float x1, float x2, float y1, float y2, float z1, float z2);

// inner classes
class IsoSurface {
public:
	IsoSurface(float l, float h) {
		low = l;
		high = h;
	}
	bool hasAfter(float density) {
		return density >= low;
	}
	bool hasBetween(float density) {
		return density >= low && density <= high;
	}
private:
	float low;
	float high;
};

// TODO: if you want to declare and define additional functions, you may add
// them here.

// http://www.lighthouse3d.com/opengl/billboarding/index.php
void billboard(Vec3Df right, Vec3Df up, Vec3Df center, Vec3Df color, float alpha, float size) {
	Vec3Df leftbot = Vec3Df(center.p[0] - (right.p[0] + up.p[0]) * size,
		center.p[1] - (right.p[1] + up.p[1]) * size,
		center.p[2] - (right.p[2] + up.p[2]) * size);

	Vec3Df rightbot = Vec3Df(center.p[0] + (right.p[0] - up.p[0]) * size,
		center.p[1] + (right.p[1] - up.p[1]) * size,
		center.p[2] + (right.p[2] - up.p[2]) * size);

	Vec3Df rightup = Vec3Df(center.p[0] + (right.p[0] + up.p[0]) * size,
		center.p[1] + (right.p[1] + up.p[1]) * size,
		center.p[2] + (right.p[2] + up.p[2]) * size);

	Vec3Df leftup = Vec3Df(center.p[0] - (right.p[0] - up.p[0]) * size,
		center.p[1] - (right.p[1] - up.p[1]) * size,
		center.p[2] - (right.p[2] - up.p[2]) * size);

	glColor4f(color.p[0], color.p[1], color.p[2], alpha);
	glVertex3f(leftbot.p[0], leftbot.p[1], leftbot.p[2]);
	glVertex3f(rightbot.p[0], rightbot.p[1], rightbot.p[2]);
	glVertex3f(rightup.p[0], rightup.p[1], rightup.p[2]);
	glVertex3f(leftup.p[0], leftup.p[1], leftup.p[2]);
}

Volume load_lod_volume(Volume& vol) {
	Volume res(vol.width() / 2 + 1, vol.height() / 2 + 1, vol.depth() / 2 + 1);

	std::cout << "Creating reduced volume version of size: " << res.width() << "x" << res.height() << "x" << res.depth() << std::endl;

	//for (int z = 0; z < gVolume.depth(); z += 2) {
	concurrency::parallel_for(size_t(0), size_t(vol.depth()), size_t(2), [&](size_t z) {
		for (int y = 0; y < gVolume.height(); y += 2) {
			for (int x = 0; x < gVolume.width(); x += 2) {
				float density = vol(x, y, z);

				if (x != 0 && y != 0 && z != 0 && x != vol.width() - 1 && y != vol.height() - 1 && z != vol.depth() - 1) {
					// avg of neighbours
					float avg = 0.0f;
					float total = 0.0f;
					for (int hor = x - 1; hor <= x + 1; hor++) {
						for (int ver = y - 1; ver <= y + 1; ver++) {
							for (int dep = z - 1; dep <= z + 1; dep++) {
								avg += vol(hor, ver, dep);
								total++;
							}
						}
					}
					avg /= total;
					res(std::floorf(x / 2.0f), std::floor(y / 2.0f), std::floor(z / 2.0f)) = avg;
				} else {
					res(std::floorf(x / 2.0f), std::floor(y / 2.0f), std::floor(z / 2.0f)) = density;
				}
			}
		}
	});
	return res;
}

void performTransfer(AXIS axis, int i, int j, int k) {
	float XT;
	float YT;
	float ZT;

	int x, y, z;

	if (axis == AXIS::X) {
		XT = 2.f*float(k) / gVolumeLargestDimension - 1.f;
		YT = 2.f*float(j) / gVolumeLargestDimension - 1.f;
		ZT = 2.f*float(i) / gVolumeLargestDimension - 1.f;
		x = k;
		y = j;
		z = i;
	} else if (axis == AXIS::Y) {
		XT = 2.f*float(i) / gVolumeLargestDimension - 1.f;
		YT = 2.f*float(k) / gVolumeLargestDimension - 1.f;
		ZT = 2.f*float(j) / gVolumeLargestDimension - 1.f;
		x = i;
		y = k;
		z = j;
	} else {
		XT = 2.f*float(j) / gVolumeLargestDimension - 1.f;
		YT = 2.f*float(i) / gVolumeLargestDimension - 1.f;
		ZT = 2.f*float(k) / gVolumeLargestDimension - 1.f;
		x = j;
		y = i;
		z = k;
	}

	// Don't do anything if the coords are outside the ESelectiveRegionType::{shape}
	if (!checkIntersection(XT, YT, ZT)) return;

	float density = gVolume(x, y, z);

	// BONSAI:
	// trunk:
	IsoSurface trunk = IsoSurface(0.5f, 0.6f);
	//IsoSurface trunk = IsoSurface(0.2f, 0.6f);
	//IsoSurface trunk = IsoSurface(FLT_MAX, FLT_MIN); // ignores trunk when this line is uncommented

	// leaves:
	IsoSurface leaves = IsoSurface(0.13f, 0.2f);
	//IsoSurface leaves = IsoSurface(FLT_MAX, FLT_MIN); // ignores leaves when this line is uncommented

	// BACKPACK:
	IsoSurface lightgrey = IsoSurface(0.25f, 0.3f);
	//IsoSurface lightgrey = IsoSurface(FLT_MAX, FLT_MIN);
	IsoSurface darkgrey = IsoSurface(0.18f, 0.25);
	//IsoSurface darkgrey = IsoSurface(FLT_MAX, FLT_MIN);
	IsoSurface red = IsoSurface(0.9f, 1.0f);
	//IsoSurface red = IsoSurface(FLT_MAX, FLT_MIN);
	IsoSurface lightblue = IsoSurface(0.61f, 0.9f);
	//IsoSurface lightblue = IsoSurface(FLT_MAX, FLT_MIN);
	IsoSurface yellow = IsoSurface(0.4f, 0.55f);
	//IsoSurface yellow = IsoSurface(FLT_MAX, FLT_MIN);

	// points with low density have a low alpha and vice versa
	float alpha = 1.0f / (1.0f + std::pow(density / (1.0f - density), -1.5f));
	Vec3Df c = Vec3Df();

	switch (global_ttype) {
	case TRANSFERTYPE::BONSAI:
		if (trunk.hasBetween(density)) {
			c = Vec3Df(.33f, .21f, .1f); // brown
			c *= density;
			glColor4f(c.p[0], c.p[1], c.p[2], alpha);
			glVertex3f(XT, YT, ZT);
		} else if (leaves.hasBetween(density)) {
			c = Vec3Df(0.0f, 1.0f, 0.0f); // green
			c *= density;
			glColor4f(c.p[0], c.p[1], c.p[2], alpha);
			glVertex3f(XT, YT, ZT);
		}
		break;
	case TRANSFERTYPE::BACKPACK:
		if (lightgrey.hasBetween(density)) {
			c = Vec3Df(0.85f, 0.85f, 0.85f); // lightgrey
			c *= density;
			glColor4f(c.p[0], c.p[1], c.p[2], alpha);
			glVertex3f(XT, YT, ZT);
		} else if (darkgrey.hasBetween(density)) {
			c = Vec3Df(0.66f, 0.66f, 0.66f); // darkgrey
			c *= density;
			glColor4f(c.p[0], c.p[1], c.p[2], alpha);
			glVertex3f(XT, YT, ZT);
		} else if (red.hasBetween(density)) {
			c = Vec3Df(1.0f, 0.0f, 0.0f); // red
			c *= density;
			glColor4f(c.p[0], c.p[1], c.p[2], alpha);
			glVertex3f(XT, YT, ZT);
		} else if (lightblue.hasBetween(density)) {
			c = Vec3Df(0.0f, 1.0f, 1.0f); // lightblue
			c *= density;
			glColor4f(c.p[0], c.p[1], c.p[2], alpha);
			glVertex3f(XT, YT, ZT);
		} else if (yellow.hasBetween(density)) {
			c = Vec3Df(1.0f, 1.0f, 0.0f); // yellow
			c *= density;
			glColor4f(c.p[0], c.p[1], c.p[2], alpha);
			glVertex3f(XT, YT, ZT);
		}
		break;
	default:
		break;
	}
}

void performTransferWithLighting(AXIS axis, int i, int j, int k, int p, int q, int r, Vec3Df lightdir) {
	float XT;
	float YT;
	float ZT;

	int x, y, z;

	if (axis == AXIS::X) {
		XT = 2.f*float(k) / gVolumeLargestDimension - 1.f;
		YT = 2.f*float(j) / gVolumeLargestDimension - 1.f;
		ZT = 2.f*float(i) / gVolumeLargestDimension - 1.f;
		x = k;
		y = j;
		z = i;
	} else if (axis == AXIS::Y) {
		XT = 2.f*float(i) / gVolumeLargestDimension - 1.f;
		YT = 2.f*float(k) / gVolumeLargestDimension - 1.f;
		ZT = 2.f*float(j) / gVolumeLargestDimension - 1.f;
		x = i;
		y = k;
		z = j;
	} else {
		XT = 2.f*float(j) / gVolumeLargestDimension - 1.f;
		YT = 2.f*float(i) / gVolumeLargestDimension - 1.f;
		ZT = 2.f*float(k) / gVolumeLargestDimension - 1.f;
		x = j;
		y = i;
		z = k;
	}

	// Don't do anything if the coords are outside the ESelectiveRegionType::{shape}
	if (!checkIntersection(XT, YT, ZT)) return;

	float density = gVolume(x, y, z);

	if (k != 0 && j != 0 && i != 0 && k != p - 1 && j != q - 1 && i != r - 1) {
		const int XBack = x - 1;
		const int YBack = y - 1;
		const int ZBack = z - 1;
		const int XFront = x + 1;
		const int YFront = y + 1;
		const int ZFront = z + 1;

		// partial derivative
		float partDerX = (gVolume(XFront, y, z) - gVolume(XBack, y, z)) / 2.0f;
		float partDerY = (gVolume(x, YFront, z) - gVolume(x, YBack, z)) / 2.0f;
		float partDerZ = (gVolume(x, y, ZFront) - gVolume(x, y, ZBack)) / 2.0f;

		Vec3Df normal = Vec3Df(-partDerX, -partDerY, -partDerZ);
		normal.normalize();

		float dot = Vec3Df::dotProduct(normal, lightdir);

		// BONSAI:
		// trunk:
		IsoSurface trunk = IsoSurface(0.5f, 0.9f);
		//IsoSurface trunk = IsoSurface(0.2f, 0.6f);
		//IsoSurface trunk = IsoSurface(FLT_MAX, FLT_MIN); // ignores trunk when this line is uncommented

		// leaves:
		IsoSurface leaves = IsoSurface(0.13f, 0.2f);
		//IsoSurface leaves = IsoSurface(FLT_MAX, FLT_MIN); // ignores leaves when this line is uncommented

		// BACKPACK:
		IsoSurface lightgrey = IsoSurface(0.25f, 0.3f);
		//IsoSurface lightgrey = IsoSurface(FLT_MAX, FLT_MIN);
		IsoSurface darkgrey = IsoSurface(0.18f, 0.25);
		//IsoSurface darkgrey = IsoSurface(FLT_MAX, FLT_MIN);
		IsoSurface red = IsoSurface(0.9f, 1.0f);
		//IsoSurface red = IsoSurface(FLT_MAX, FLT_MIN);
		IsoSurface lightblue = IsoSurface(0.61f, 0.9f);
		//IsoSurface lightblue = IsoSurface(FLT_MAX, FLT_MIN);
		IsoSurface yellow = IsoSurface(0.4f, 0.55f);
		//IsoSurface yellow = IsoSurface(FLT_MAX, FLT_MIN);

		// points with low density have a low alpha and vice versa
		float alpha = 1.0f / (1.0f + std::pow(density / (1.0f - density), -1.5f));

		Vec3Df c = Vec3Df();
		switch (global_ttype) {
		case TRANSFERTYPE::BONSAI:
			if (trunk.hasBetween(density)) {
				c = Vec3Df(.33f, .21f, .1f); // brown
				c *= density * dot;
				glColor4f(c.p[0], c.p[1], c.p[2], alpha);
				glVertex3f(XT, YT, ZT);
			} else if (leaves.hasBetween(density)) {
				c = Vec3Df(0.0f, 1.0f, 0.0f); // green
				c *= density * dot;
				glColor4f(c.p[0], c.p[1], c.p[2], alpha);
				glVertex3f(XT, YT, ZT);
			}
			break;
		case TRANSFERTYPE::BACKPACK: {
			if (lightgrey.hasBetween(density)) {
				c = Vec3Df(0.85f, 0.85f, 0.85f); // lightgrey
				c *= density * dot;
				glColor4f(c.p[0], c.p[1], c.p[2], alpha);
				glVertex3f(XT, YT, ZT);
			} else if (darkgrey.hasBetween(density)) {
				c = Vec3Df(0.66f, 0.66f, 0.66f); // darkgrey
				c *= density * dot;
				glColor4f(c.p[0], c.p[1], c.p[2], alpha);
				glVertex3f(XT, YT, ZT);
			} else if (red.hasBetween(density)) {
				c = Vec3Df(1.0f, 0.0f, 0.0f); // red
				c *= density * dot;
				glColor4f(c.p[0], c.p[1], c.p[2], alpha);
				glVertex3f(XT, YT, ZT);
			} else if (lightblue.hasBetween(density)) {
				c = Vec3Df(0.0f, 1.0f, 1.0f); // lightblue
				c *= density * dot;
				glColor4f(c.p[0], c.p[1], c.p[2], alpha);
				glVertex3f(XT, YT, ZT);
			} else if (yellow.hasBetween(density)) {
				c = Vec3Df(1.0f, 1.0f, 0.0f); // yellow
				c *= density * dot;
				glColor4f(c.p[0], c.p[1], c.p[2], alpha);
				glVertex3f(XT, YT, ZT);
			}
		} break;
		default:
			break;
		}
	} else {
		performTransfer(axis, i, j, k);
	}
}

void performTransferWithTrilinearInterpolation(AXIS axis, int i, int j, int k, int p, int q, int r,
	Vec3Df lightdir, bool checkTrilinearInterpolation) {
	float XT;
	float YT;
	float ZT;

	int x, y, z;

	if (axis == AXIS::X) {
		XT = 2.f*float(k) / gVolumeLargestDimension - 1.f;
		YT = 2.f*float(j) / gVolumeLargestDimension - 1.f;
		ZT = 2.f*float(i) / gVolumeLargestDimension - 1.f;
		x = k;
		y = j;
		z = i;
	} else if (axis == AXIS::Y) {
		XT = 2.f*float(i) / gVolumeLargestDimension - 1.f;
		YT = 2.f*float(k) / gVolumeLargestDimension - 1.f;
		ZT = 2.f*float(j) / gVolumeLargestDimension - 1.f;
		x = i;
		y = k;
		z = j;
	} else {
		XT = 2.f*float(j) / gVolumeLargestDimension - 1.f;
		YT = 2.f*float(i) / gVolumeLargestDimension - 1.f;
		ZT = 2.f*float(k) / gVolumeLargestDimension - 1.f;
		x = j;
		y = i;
		z = k;
	}

	// Don't do anything if the coords are outside the ESelectiveRegionType::{shape}
	if (!checkIntersection(XT, YT, ZT)) return;

	float density = gVolume(x, y, z);

	if (k != 0 && j != 0 && i != 0 && k != p - 1 && j != q - 1 && i != r - 1) {

		float p000 = gVolume(x - 1, y - 1, z - 1);
		float p100 = gVolume(x + 1, y - 1, z - 1);
		float p110 = gVolume(x + 1, y + 1, z - 1);
		float p010 = gVolume(x - 1, y + 1, z - 1);

		float p001 = gVolume(x - 1, y - 1, z + 1);
		float p101 = gVolume(x + 1, y - 1, z + 1);
		float p111 = gVolume(x + 1, y + 1, z + 1);
		float p011 = gVolume(x - 1, y + 1, z + 1);

		float trilin = triLinearInterpolation(x, y, z, p000, p001, p010, p011, p100, p101,
			p110, p111, x - 1, x + 1, y - 1, y + 1, z - 1, z + 1);


		// partial derivative
		float partDerX = (gVolume(x + 1, y, z) - gVolume(x - 1, y, z)) / 2.0f;
		float partDerY = (gVolume(x, y + 1, z) - gVolume(x, y - 1, z)) / 2.0f;
		float partDerZ = (gVolume(x, y, z + 1) - gVolume(x, y, z - 1)) / 2.0f;

		Vec3Df normal = Vec3Df(-partDerX, -partDerY, -partDerZ);
		normal.normalize();

		float dot = Vec3Df::dotProduct(normal, lightdir);

		// BONSAI:
		// trunk:
		IsoSurface trunk = IsoSurface(0.5f, 0.9f);
		//IsoSurface trunk = IsoSurface(0.2f, 0.6f);
		//IsoSurface trunk = IsoSurface(FLT_MAX, FLT_MIN); // ignores trunk when this line is uncommented

		// leaves:
		IsoSurface leaves = IsoSurface(0.13f, 0.2f);
		//IsoSurface leaves = IsoSurface(FLT_MAX, FLT_MIN); // ignores leaves when this line is uncommented

		//// BACKPACK:
		IsoSurface lightgrey = IsoSurface(0.25f, 0.3f);
		//IsoSurface lightgrey = IsoSurface(FLT_MAX, FLT_MIN);
		IsoSurface darkgrey = IsoSurface(0.18f, 0.25);
		//IsoSurface darkgrey = IsoSurface(FLT_MAX, FLT_MIN);
		IsoSurface red = IsoSurface(0.9f, 1.0f);
		//IsoSurface red = IsoSurface(FLT_MAX, FLT_MIN);
		IsoSurface lightblue = IsoSurface(0.61f, 0.9f);
		//IsoSurface lightblue = IsoSurface(FLT_MAX, FLT_MIN);
		IsoSurface yellow = IsoSurface(0.4f, 0.55f);
		//IsoSurface yellow = IsoSurface(FLT_MAX, FLT_MIN);

		// points with low density have a low alpha and vice versa
		float alpha = 1.0f / (1.0f + std::pow(density / (1.0f - density), -1.5f));

		Vec3Df c = Vec3Df();
		switch (global_ttype) {
		case TRANSFERTYPE::BONSAI:
			if (trunk.hasBetween(density)) {
				c = Vec3Df(.33f, .21f, .1f); // brown
				c *= density * dot;

				glColor4f(c.p[0], c.p[1], c.p[2], alpha);
				glVertex3f(XT, YT, ZT);
			} else if (leaves.hasBetween(density)) {
				c = Vec3Df(0.0f, 1.0f, 0.0f); // green
				c *= density * dot;

				glColor4f(c.p[0], c.p[1], c.p[2], alpha);
				glVertex3f(XT, YT, ZT);
			}
			if (checkTrilinearInterpolation) {
				if (trunk.hasBetween(trilin)) {
					c = Vec3Df(.33f, .21f, .1f); // brown
					c *= density * dot;

					glColor4f(c.p[0], c.p[1], c.p[2], alpha);
					glVertex3f(XT, YT, ZT);
				} else if (leaves.hasBetween(trilin)) {
					c = Vec3Df(0.0f, 1.0f, 0.0f); // green
					c *= density * dot;

					glColor4f(c.p[0], c.p[1], c.p[2], alpha);
					glVertex3f(XT, YT, ZT);
				}
			}
			break;
		case TRANSFERTYPE::BACKPACK: {
			if (lightgrey.hasBetween(density)) {
				c = Vec3Df(0.85f, 0.85f, 0.85f); // lightgrey
				c *= density * dot;

				glColor4f(c.p[0], c.p[1], c.p[2], alpha);
				glVertex3f(XT, YT, ZT);
			} else if (darkgrey.hasBetween(density)) {
				c = Vec3Df(0.66f, 0.66f, 0.66f); // darkgrey
				c *= density * dot;

				glColor4f(c.p[0], c.p[1], c.p[2], alpha);
				glVertex3f(XT, YT, ZT);
			} else if (red.hasBetween(density)) {
				c = Vec3Df(1.0f, 0.0f, 0.0f); // red
				c *= density * dot;

				glColor4f(c.p[0], c.p[1], c.p[2], alpha);
				glVertex3f(XT, YT, ZT);
			} else if (lightblue.hasBetween(density)) {
				c = Vec3Df(0.0f, 1.0f, 1.0f); // lightblue
				c *= density * dot;

				glColor4f(c.p[0], c.p[1], c.p[2], alpha);
				glVertex3f(XT, YT, ZT);
			} else if (yellow.hasBetween(density)) {
				c = Vec3Df(1.0f, 1.0f, 0.0f); // yellow
				c *= density * dot;

				glColor4f(c.p[0], c.p[1], c.p[2], alpha);
				glVertex3f(XT, YT, ZT);
			}
			if (checkTrilinearInterpolation) {
				if (lightgrey.hasBetween(trilin)) {
					c = Vec3Df(0.85f, 0.85f, 0.85f); // lightgrey
					c *= density * dot;

					glColor4f(c.p[0], c.p[1], c.p[2], alpha);
					glVertex3f(XT, YT, ZT);
				} else if (darkgrey.hasBetween(trilin)) {
					c = Vec3Df(0.66f, 0.66f, 0.66f); // darkgrey
					c *= density * dot;

					glColor4f(c.p[0], c.p[1], c.p[2], alpha);
					glVertex3f(XT, YT, ZT);
				} else if (red.hasBetween(trilin)) {
					c = Vec3Df(1.0f, 0.0f, 0.0f); // red
					c *= density * dot;

					glColor4f(c.p[0], c.p[1], c.p[2], alpha);
					glVertex3f(XT, YT, ZT);
				} else if (lightblue.hasBetween(trilin)) {
					c = Vec3Df(0.0f, 1.0f, 1.0f); // lightblue
					c *= density * dot;

					glColor4f(c.p[0], c.p[1], c.p[2], alpha);
					glVertex3f(XT, YT, ZT);
				} else if (yellow.hasBetween(trilin)) {
					c = Vec3Df(1.0f, 1.0f, 0.0f); // yellow
					c *= density * dot;

					glColor4f(c.p[0], c.p[1], c.p[2], alpha);
					glVertex3f(XT, YT, ZT);
				}
			}
		} break;
		default:
			break;
		}
	} else {
		performTransfer(axis, i, j, k);
	}
}

void performTransferWithBillboards(AXIS axis, int i, int j, int k, int p, int q, int r,
	Vec3Df lightdir, bool checkTrilinearInterpolation, Vec3Df right, Vec3Df up, float size) {
	float XT;
	float YT;
	float ZT;

	int x, y, z;

	if (axis == AXIS::X) {
		XT = 2.f*float(k) / gVolumeLargestDimension - 1.f;
		YT = 2.f*float(j) / gVolumeLargestDimension - 1.f;
		ZT = 2.f*float(i) / gVolumeLargestDimension - 1.f;
		x = k;
		y = j;
		z = i;
	} else if (axis == AXIS::Y) {
		XT = 2.f*float(i) / gVolumeLargestDimension - 1.f;
		YT = 2.f*float(k) / gVolumeLargestDimension - 1.f;
		ZT = 2.f*float(j) / gVolumeLargestDimension - 1.f;
		x = i;
		y = k;
		z = j;
	} else {
		XT = 2.f*float(j) / gVolumeLargestDimension - 1.f;
		YT = 2.f*float(i) / gVolumeLargestDimension - 1.f;
		ZT = 2.f*float(k) / gVolumeLargestDimension - 1.f;
		x = j;
		y = i;
		z = k;
	}

	// Don't do anything if the coords are outside the ESelectiveRegionType::{shape}
	if (!checkIntersection(XT, YT, ZT)) return;

	float density = gVolume(x, y, z);

	// BONSAI:
	// trunk:
	IsoSurface trunk = IsoSurface(0.5f, 0.9f);
	//IsoSurface trunk = IsoSurface(0.2f, 0.6f);
	//IsoSurface trunk = IsoSurface(FLT_MAX, FLT_MIN); // ignores trunk when this line is uncommented

	// leaves:
	IsoSurface leaves = IsoSurface(0.13f, 0.2f);
	//IsoSurface leaves = IsoSurface(FLT_MAX, FLT_MIN); // ignores leaves when this line is uncommented

	//// BACKPACK:
	IsoSurface lightgrey = IsoSurface(0.25f, 0.3f);
	//IsoSurface lightgrey = IsoSurface(FLT_MAX, FLT_MIN);
	IsoSurface darkgrey = IsoSurface(0.18f, 0.25);
	//IsoSurface darkgrey = IsoSurface(FLT_MAX, FLT_MIN);
	IsoSurface red = IsoSurface(0.9f, 1.0f);
	//IsoSurface red = IsoSurface(FLT_MAX, FLT_MIN);
	IsoSurface lightblue = IsoSurface(0.61f, 0.9f);
	//IsoSurface lightblue = IsoSurface(FLT_MAX, FLT_MIN);
	IsoSurface yellow = IsoSurface(0.4f, 0.55f);
	//IsoSurface yellow = IsoSurface(FLT_MAX, FLT_MIN);

	Vec3Df center = Vec3Df(XT, YT, ZT);

	if (k != 0 && j != 0 && i != 0 && k != p - 1 && j != q - 1 && i != r - 1) {
		float p000 = gVolume(x - 1, y - 1, z - 1);
		float p100 = gVolume(x + 1, y - 1, z - 1);
		float p110 = gVolume(x + 1, y + 1, z - 1);
		float p010 = gVolume(x - 1, y + 1, z - 1);

		float p001 = gVolume(x - 1, y - 1, z + 1);
		float p101 = gVolume(x + 1, y - 1, z + 1);
		float p111 = gVolume(x + 1, y + 1, z + 1);
		float p011 = gVolume(x - 1, y + 1, z + 1);

		float trilin = triLinearInterpolation(x, y, z, p000, p001, p010, p011, p100, p101,
			p110, p111, x - 1, x + 1, y - 1, y + 1, z - 1, z + 1);

		// partial derivative
		float partDerX = (gVolume(x + 1, y, z) - gVolume(x - 1, y, z)) / 2.0f;
		float partDerY = (gVolume(x, y + 1, z) - gVolume(x, y - 1, z)) / 2.0f;
		float partDerZ = (gVolume(x, y, z + 1) - gVolume(x, y, z - 1)) / 2.0f;

		Vec3Df normal = Vec3Df(-partDerX, -partDerY, -partDerZ);
		normal.normalize();

		float dot = Vec3Df::dotProduct(normal, lightdir);

		// points with low density have a low alpha and vice versa
		float alpha = 1.0f / (1.0f + std::pow(density / (1.0f - density), -1.5f));

		Vec3Df c = Vec3Df();
		switch (global_ttype) {
		case TRANSFERTYPE::BONSAI:
			if (trunk.hasBetween(density)) {
				c = Vec3Df(.33f, .21f, .1f); // brown
				c *= density * dot;

				billboard(right, up, center, c, alpha, size);
			} else if (leaves.hasBetween(density)) {
				c = Vec3Df(0.0f, 1.0f, 0.0f); // green
				c *= density * dot;

				billboard(right, up, center, c, alpha, size);
			}
			if (checkTrilinearInterpolation) {
				if (trunk.hasBetween(trilin)) {
					c = Vec3Df(.33f, .21f, .1f); // brown
					c *= density * dot;

					billboard(right, up, center, c, alpha, size);
				} else if (leaves.hasBetween(trilin)) {
					c = Vec3Df(0.0f, 1.0f, 0.0f); // green
					c *= density * dot;

					billboard(right, up, center, c, alpha, size);
				}
			}
			break;
		case TRANSFERTYPE::BACKPACK: {
			if (lightgrey.hasBetween(density)) {
				c = Vec3Df(0.85f, 0.85f, 0.85f); // lightgrey
				c *= density * dot;

				billboard(right, up, center, c, alpha, size);
			} else if (darkgrey.hasBetween(density)) {
				c = Vec3Df(0.66f, 0.66f, 0.66f); // darkgrey
				c *= density * dot;

				billboard(right, up, center, c, alpha, size);
			} else if (red.hasBetween(density)) {
				c = Vec3Df(1.0f, 0.0f, 0.0f); // red
				c *= density * dot;

				billboard(right, up, center, c, alpha, size);
			} else if (lightblue.hasBetween(density)) {
				c = Vec3Df(0.0f, 1.0f, 1.0f); // lightblue
				c *= density * dot;

				billboard(right, up, center, c, alpha, size);
			} else if (yellow.hasBetween(density)) {
				c = Vec3Df(1.0f, 1.0f, 0.0f); // yellow
				c *= density * dot;

				billboard(right, up, center, c, alpha, size);
			}
			if (checkTrilinearInterpolation) {
				if (lightgrey.hasBetween(trilin)) {
					c = Vec3Df(0.85f, 0.85f, 0.85f); // lightgrey
					c *= density * dot;

					billboard(right, up, center, c, alpha, size);
				} else if (darkgrey.hasBetween(trilin)) {
					c = Vec3Df(0.66f, 0.66f, 0.66f); // darkgrey
					c *= density * dot;

					billboard(right, up, center, c, alpha, size);
				} else if (red.hasBetween(trilin)) {
					c = Vec3Df(1.0f, 0.0f, 0.0f); // red
					c *= density * dot;

					billboard(right, up, center, c, alpha, size);
				} else if (lightblue.hasBetween(trilin)) {
					c = Vec3Df(0.0f, 1.0f, 1.0f); // lightblue
					c *= density * dot;

					billboard(right, up, center, c, alpha, size);
				} else if (yellow.hasBetween(trilin)) {
					c = Vec3Df(1.0f, 1.0f, 0.0f); // yellow
					c *= density * dot;

					billboard(right, up, center, c, alpha, size);
				}
			}
		} break;
		default:
			break;
		}
	} else {
		// points with low density have a low alpha and vice versa
		float alpha = 1.0f / (1.0f + std::pow(density / (1.0f - density), -1.5f));
		Vec3Df c = Vec3Df();
		switch (global_ttype) {
		case TRANSFERTYPE::BONSAI:
			if (trunk.hasBetween(density)) {
				c = Vec3Df(.33f, .21f, .1f); // brown
				c *= density;

				billboard(right, up, center, c, alpha, size);
			} else if (leaves.hasBetween(density)) {
				c = Vec3Df(0.0f, 1.0f, 0.0f); // green
				c *= density;
				
				billboard(right, up, center, c, alpha, size);
			}
			break;
		case TRANSFERTYPE::BACKPACK:
			if (lightgrey.hasBetween(density)) {
				c = Vec3Df(0.85f, 0.85f, 0.85f); // lightgrey
				c *= density;
				
				billboard(right, up, center, c, alpha, size);
			} else if (darkgrey.hasBetween(density)) {
				c = Vec3Df(0.66f, 0.66f, 0.66f); // darkgrey
				c *= density;
				
				billboard(right, up, center, c, alpha, size);
			} else if (red.hasBetween(density)) {
				c = Vec3Df(1.0f, 0.0f, 0.0f); // red
				c *= density;
		
				billboard(right, up, center, c, alpha, size);
			} else if (lightblue.hasBetween(density)) {
				c = Vec3Df(0.0f, 1.0f, 1.0f); // lightblue
				c *= density;

				billboard(right, up, center, c, alpha, size);
			} else if (yellow.hasBetween(density)) {
				c = Vec3Df(1.0f, 1.0f, 0.0f); // yellow
				c *= density;

				billboard(right, up, center, c, alpha, size);
			}
			break;
		default:
			break;
		}
	}
}

void performTransferWithArrays(AXIS axis, int i, int j, int k, int p, int q, int r, Vec3Df lightdir) {
	float XT;
	float YT;
	float ZT;

	int x, y, z;

	if (axis == AXIS::X) {
		XT = 2.f*float(k) / gVolumeLargestDimension - 1.f;
		YT = 2.f*float(j) / gVolumeLargestDimension - 1.f;
		ZT = 2.f*float(i) / gVolumeLargestDimension - 1.f;
		x = k;
		y = j;
		z = i;
	} else if (axis == AXIS::Y) {
		XT = 2.f*float(i) / gVolumeLargestDimension - 1.f;
		YT = 2.f*float(k) / gVolumeLargestDimension - 1.f;
		ZT = 2.f*float(j) / gVolumeLargestDimension - 1.f;
		x = i;
		y = k;
		z = j;
	} else {
		XT = 2.f*float(j) / gVolumeLargestDimension - 1.f;
		YT = 2.f*float(i) / gVolumeLargestDimension - 1.f;
		ZT = 2.f*float(k) / gVolumeLargestDimension - 1.f;
		x = j;
		y = i;
		z = k;
	}

	// Don't do anything if the coords are outside the ESelectiveRegionType::{shape}
	if (!checkIntersection(XT, YT, ZT)) return;

	float density = gVolume(x, y, z);

	if (k != 0 && j != 0 && i != 0 && k != p - 1 && j != q - 1 && i != r - 1) {
		const int XBack = x - 1;
		const int YBack = y - 1;
		const int ZBack = z - 1;
		const int XFront = x + 1;
		const int YFront = y + 1;
		const int ZFront = z + 1;

		// partial derivative
		float partDerX = (gVolume(XFront, y, z) - gVolume(XBack, y, z)) / 2.0f;
		float partDerY = (gVolume(x, YFront, z) - gVolume(x, YBack, z)) / 2.0f;
		float partDerZ = (gVolume(x, y, ZFront) - gVolume(x, y, ZBack)) / 2.0f;

		Vec3Df normal = Vec3Df(-partDerX, -partDerY, -partDerZ);
		normal.normalize();

		float dot = Vec3Df::dotProduct(normal, lightdir);

		// BONSAI:
		// trunk:
		IsoSurface trunk = IsoSurface(0.5f, 0.9f);
		//IsoSurface trunk = IsoSurface(0.2f, 0.6f);
		//IsoSurface trunk = IsoSurface(FLT_MAX, FLT_MIN); // ignores trunk when this line is uncommented

		// leaves:
		IsoSurface leaves = IsoSurface(0.13f, 0.2f);
		//IsoSurface leaves = IsoSurface(FLT_MAX, FLT_MIN); // ignores leaves when this line is uncommented

		//// BACKPACK:
		IsoSurface lightgrey = IsoSurface(0.25f, 0.3f);
		//IsoSurface lightgrey = IsoSurface(FLT_MAX, FLT_MIN);
		IsoSurface darkgrey = IsoSurface(0.18f, 0.25);
		//IsoSurface darkgrey = IsoSurface(FLT_MAX, FLT_MIN);
		IsoSurface red = IsoSurface(0.9f, 1.0f);
		//IsoSurface red = IsoSurface(FLT_MAX, FLT_MIN);
		IsoSurface lightblue = IsoSurface(0.61f, 0.9f);
		//IsoSurface lightblue = IsoSurface(FLT_MAX, FLT_MIN);
		IsoSurface yellow = IsoSurface(0.4f, 0.55f);
		//IsoSurface yellow = IsoSurface(FLT_MAX, FLT_MIN);

		// points with low density have a low alpha and vice versa
		float alpha = 1.0f / (1.0f + std::pow(density / (1.0f - density), -1.5f));

		Vec3Df c = Vec3Df();
		switch (global_ttype) {
		case TRANSFERTYPE::BONSAI:
			if (trunk.hasBetween(density)) {
				c = Vec3Df(.33f, .21f, .1f); // brown
				c *= density * dot;

				gDrawColors.push_back(c.p[0]);
				gDrawColors.push_back(c.p[1]);
				gDrawColors.push_back(c.p[2]);
				gDrawColors.push_back(alpha);
				gDrawPositions.push_back(XT);
				gDrawPositions.push_back(YT);
				gDrawPositions.push_back(ZT);
			} else if (leaves.hasBetween(density)) {
				c = Vec3Df(0.0f, 1.0f, 0.0f); // green
				c *= density * dot;

				gDrawColors.push_back(c.p[0]);
				gDrawColors.push_back(c.p[1]);
				gDrawColors.push_back(c.p[2]);
				gDrawColors.push_back(alpha);
				gDrawPositions.push_back(XT);
				gDrawPositions.push_back(YT);
				gDrawPositions.push_back(ZT);
			}
			break;
		case TRANSFERTYPE::BACKPACK: {
			if (lightgrey.hasBetween(density)) {
				c = Vec3Df(0.85f, 0.85f, 0.85f); // lightgrey
				c *= density * dot;

				gDrawColors.push_back(c.p[0]);
				gDrawColors.push_back(c.p[1]);
				gDrawColors.push_back(c.p[2]);
				gDrawColors.push_back(alpha);
				gDrawPositions.push_back(XT);
				gDrawPositions.push_back(YT);
				gDrawPositions.push_back(ZT);
			} else if (darkgrey.hasBetween(density)) {
				c = Vec3Df(0.66f, 0.66f, 0.66f); // darkgrey
				c *= density * dot;

				gDrawColors.push_back(c.p[0]);
				gDrawColors.push_back(c.p[1]);
				gDrawColors.push_back(c.p[2]);
				gDrawColors.push_back(alpha);
				gDrawPositions.push_back(XT);
				gDrawPositions.push_back(YT);
				gDrawPositions.push_back(ZT);
			} else if (red.hasBetween(density)) {
				c = Vec3Df(1.0f, 0.0f, 0.0f); // red
				c *= density * dot;

				gDrawColors.push_back(c.p[0]);
				gDrawColors.push_back(c.p[1]);
				gDrawColors.push_back(c.p[2]);
				gDrawColors.push_back(alpha);
				gDrawPositions.push_back(XT);
				gDrawPositions.push_back(YT);
				gDrawPositions.push_back(ZT);
			} else if (lightblue.hasBetween(density)) {
				c = Vec3Df(0.0f, 1.0f, 1.0f); // lightblue
				c *= density * dot;

				gDrawColors.push_back(c.p[0]);
				gDrawColors.push_back(c.p[1]);
				gDrawColors.push_back(c.p[2]);
				gDrawColors.push_back(alpha);
				gDrawPositions.push_back(XT);
				gDrawPositions.push_back(YT);
				gDrawPositions.push_back(ZT);
			} else if (yellow.hasBetween(density)) {
				c = Vec3Df(1.0f, 1.0f, 0.0f); // yellow
				c *= density * dot;

				gDrawColors.push_back(c.p[0]);
				gDrawColors.push_back(c.p[1]);
				gDrawColors.push_back(c.p[2]);
				gDrawColors.push_back(alpha);
				gDrawPositions.push_back(XT);
				gDrawPositions.push_back(YT);
				gDrawPositions.push_back(ZT);
			}
		} break;
		default:
			break;
		}
	}
}

void drawSphere(float radius, Vec3Df position) {
	const float PI = 3.141592653589793238463f;

	float CX = position.p[0];
	float CY = position.p[1];
	float CZ = position.p[2];

	// circle X around Y-axis
	glColor3f(1.f, 0.0f, 0.0f);
	glBegin(GL_LINE_STRIP);
	for (float angle = 0.0; angle < 2 * PI; angle += 0.1f) {
		glVertex3f(CX + std::cos(angle)*radius, CY + std::sin(angle)*radius, CZ);
	}
	glVertex3f(CX + std::cos(2 * PI)*radius, CY + std::sin(2 * PI)*radius, CZ);
	glEnd();

	// rotate 45 degrees
	glPushMatrix();
	glTranslatef(CX, CY, CZ);
	glRotatef(45, 0, 1, 0);
	glTranslatef(-CX, -CY, -CZ);
	glBegin(GL_LINE_STRIP);
	for (float angle = 0.0; angle < 2 * PI; angle += 0.1f) {
		glVertex3f(CX + std::cos(angle)*radius, CY + std::sin(angle)*radius, CZ);
	}
	glVertex3f(CX + std::cos(2 * PI)*radius, CY + std::sin(2 * PI)*radius, CZ);
	glEnd();
	glPopMatrix();

	// rotate -45 degrees
	glPushMatrix();
	glTranslatef(CX, CY, CZ);
	glRotatef(315, 0, 1, 0);
	glTranslatef(-CX, -CY, -CZ);
	glBegin(GL_LINE_STRIP);
	for (float angle = 0.0; angle < 2 * PI; angle += 0.1f) {
		glVertex3f(CX + std::cos(angle)*radius, CY + std::sin(angle)*radius, CZ);
	}
	glVertex3f(CX + std::cos(2 * PI)*radius, CY + std::sin(2 * PI)*radius, CZ);
	glEnd();
	glPopMatrix();
	// --------------------------------------------------------------------------------

	// circle X around Z-axis
	glColor3f(1.f, 1.0f, 0.0f);
	glBegin(GL_LINE_STRIP);
	for (float angle = 0.0; angle < 2 * PI; angle += 0.1f) {
		glVertex3f(CX + std::cos(angle)*radius, CY, CZ + std::sin(angle)*radius);
	}
	glVertex3f(CX + std::cos(2 * PI)*radius, CY, CZ + std::sin(2 * PI)*radius);
	glEnd();

	// rotate 45 degrees
	glPushMatrix();
	glTranslatef(CX, CY, CZ);
	glRotatef(45, 0, 0, 1);
	glTranslatef(-CX, -CY, -CZ);
	glBegin(GL_LINE_STRIP);
	for (float angle = 0.0; angle < 2 * PI; angle += 0.1f) {
		glVertex3f(CX + std::cos(angle)*radius, CY, CZ + std::sin(angle)*radius);
	}
	glVertex3f(CX + std::cos(2 * PI)*radius, CY, CZ + std::sin(2 * PI)*radius);
	glEnd();
	glPopMatrix();

	// rotate -45 degrees
	glPushMatrix();
	glTranslatef(CX, CY, CZ);
	glRotatef(315, 0, 0, 1);
	glTranslatef(-CX, -CY, -CZ);
	glBegin(GL_LINE_STRIP);
	for (float angle = 0.0; angle < 2 * PI; angle += 0.1f) {
		glVertex3f(CX + std::cos(angle)*radius, CY, CZ + std::sin(angle)*radius);
	}
	glVertex3f(CX + std::cos(2 * PI)*radius, CY, CZ + std::sin(2 * PI)*radius);
	glEnd();
	glPopMatrix();
	// --------------------------------------------------------------------------------

	// circle Y around Z-axis
	glColor3f(0.0f, 0.0f, 1.0f);
	glBegin(GL_LINE_STRIP);
	for (float angle = 0.0; angle < 2 * PI; angle += 0.1f) {
		glVertex3f(CX, CY + std::cos(angle)*radius, CZ + std::sin(angle)*radius);
	}
	glVertex3f(CX, CY + std::cos(2 * PI)*radius, CZ + std::sin(2 * PI)*radius);
	glEnd();
}

void drawSlab(float length, AXIS axis) {
	float const minX = -1.f;
	float const minY = -1.f;
	float const minZ = -1.f;

	float const maxX = 2.f*float(gVolume.width()) / gVolumeLargestDimension - 1.f;
	float const maxY = 2.f*float(gVolume.height()) / gVolumeLargestDimension - 1.f;
	float const maxZ = 2.f*float(gVolume.depth()) / gVolumeLargestDimension - 1.f;

	glColor4f(0.f, 0.f, 1.0f, 0.5f);
	switch (axis) {
	case AXIS::X:
		glBegin(GL_QUADS);
		glVertex3f(global_position.p[0], minY, minZ);
		glVertex3f(global_position.p[0], minY, maxZ);
		glVertex3f(global_position.p[0], maxY, maxZ);
		glVertex3f(global_position.p[0], maxY, minZ);

		glVertex3f(global_position.p[0] + length, maxY, minZ);
		glVertex3f(global_position.p[0] + length, maxY, maxZ);
		glVertex3f(global_position.p[0] + length, minY, maxZ);
		glVertex3f(global_position.p[0] + length, minY, minZ);
		glEnd();
		break;
	case AXIS::Y:
		glBegin(GL_QUADS);
		glVertex3f(maxX, global_position.p[1], minZ);
		glVertex3f(maxX, global_position.p[1], maxZ);
		glVertex3f(minX, global_position.p[1], maxZ);
		glVertex3f(minX, global_position.p[1], minZ);

		glVertex3f(minX, global_position.p[1] + length, minZ);
		glVertex3f(minX, global_position.p[1] + length, maxZ);
		glVertex3f(maxX, global_position.p[1] + length, maxZ);
		glVertex3f(maxX, global_position.p[1] + length, minZ);
		glEnd();
		break;
	case AXIS::Z:
		glBegin(GL_QUADS);
		glVertex3f(maxX, minY, global_position.p[2]);
		glVertex3f(minX, minY, global_position.p[2]);
		glVertex3f(minX, maxY, global_position.p[2]);
		glVertex3f(maxX, maxY, global_position.p[2]);

		glVertex3f(maxX, minY, global_position.p[2] + length);
		glVertex3f(minX, minY, global_position.p[2] + length);
		glVertex3f(minX, maxY, global_position.p[2] + length);
		glVertex3f(maxX, maxY, global_position.p[2] + length);
		glEnd();
		break;
	default:
		break;
	}
}

void drawCube(float width, float height, float depth, Vec3Df position) {
	glLineWidth(2.f);
	glColor3f(1.f, 1.0f, 0.2f);

	float minX = position.p[0];
	float minY = position.p[1];
	float minZ = position.p[2];

	float maxX = minX + width;
	float maxY = minY + height;
	float maxZ = minZ + depth;

	glBegin(GL_LINES);
	glVertex3f(minX, minY, minZ); glVertex3f(maxX, minY, minZ);
	glVertex3f(minX, minY, minZ); glVertex3f(minX, maxY, minZ);
	glVertex3f(minX, minY, minZ); glVertex3f(minX, minY, maxZ);

	glVertex3f(maxX, maxY, maxZ); glVertex3f(minX, maxY, maxZ);
	glVertex3f(maxX, maxY, maxZ); glVertex3f(maxX, minY, maxZ);
	glVertex3f(maxX, maxY, maxZ); glVertex3f(maxX, maxY, minZ);

	glVertex3f(maxX, minY, minZ); glVertex3f(maxX, maxY, minZ);
	glVertex3f(minX, maxY, minZ); glVertex3f(minX, maxY, maxZ);
	glVertex3f(minX, minY, maxZ); glVertex3f(maxX, minY, maxZ);

	glVertex3f(minX, maxY, maxZ); glVertex3f(minX, minY, maxZ);
	glVertex3f(maxX, minY, maxZ); glVertex3f(maxX, minY, minZ);
	glVertex3f(maxX, maxY, minZ); glVertex3f(minX, maxY, minZ);
	glEnd();
}

bool checkSphereIntersection(float X, float Y, float Z) {
	float CX = global_position.p[0];
	float CY = global_position.p[1];
	float CZ = global_position.p[2];
	float radius = global_radius;
	// A point P=(x, y, z) is inside the sphere with center C=(cx, cy, cz), if:
	return pow((X - CX), 2) + pow((Y - CY), 2) + pow((Z - CZ), 2) < pow(radius, 2);
}

bool checkCubeIntersection(float X, float Y, float Z) {
	Vec3Df min = Vec3Df(global_position.p[0], global_position.p[1], global_position.p[2]);
	Vec3Df max = Vec3Df(global_position.p[0] + global_width, global_position.p[1] + global_height, global_position.p[2] + global_depth);

	if (min.p[0] <= X && max.p[0] >= X &&
		min.p[1] <= Y && max.p[1] >= Y &&
		min.p[2] <= Z && max.p[2] >= Z) {
		return true;
	}

	return false;
}

bool checkSlabIntersection(float x, float y, float z) {
	switch (global_slab_axis) {
	case AXIS::X:
		return x >= global_position.p[0] && x <= (global_position.p[0] + global_slab_length);
	case AXIS::Y:
		return y >= global_position.p[1] && y <= (global_position.p[1] + global_slab_length);
	case AXIS::Z:
		return z >= global_position.p[2] && z <= (global_position.p[2] + global_slab_length);
	default:
		break;
	}
	return false;
}

bool checkIntersection(float X, float Y, float Z) {
	switch (gSelectiveRegionType) {
	case ESelectiveRegionType::sphere:
		return checkSphereIntersection(X, Y, Z);
	case ESelectiveRegionType::cube:
		return checkCubeIntersection(X, Y, Z);
	case ESelectiveRegionType::slab:
		return checkSlabIntersection(X, Y, Z);
	default:
		return false;
	}
}

float linearInterPolation(float x, float x1, float x2, float q00, float q01) {
	return ((x2 - x) / (x2 - x1)) * q00 + ((x - x1) / (x2 - x1)) * q01;
}

float biLinearInterplationp(float x, float y, float q11, float q12, float q21, float q22, float x1, float x2, float y1, float y2) {
	float r1 = linearInterPolation(x, x1, x2, q11, q21);
	float r2 = linearInterPolation(x, x1, x2, q12, q22);

	return linearInterPolation(y, y1, y2, r1, r2);
}

/*
Trilinear interpolation.
https://en.wikipedia.org/wiki/Trilinear_interpolation

a trilinear interpolation is identical to two bilinear interpolation combined with a linear interpolation.
*/
float triLinearInterpolation(float x, float y, float z, float p000, float p001, float p010, float p011,
	float p100, float p101, float p110, float p111, float x1, float x2, float y1, float y2, float z1, float z2) {

	float x00 = linearInterPolation(x, x1, x2, p000, p100);
	float x10 = linearInterPolation(x, x1, x2, p010, p110);
	float x01 = linearInterPolation(x, x1, x2, p001, p101);
	float x11 = linearInterPolation(x, x1, x2, p011, p111);
	float r0 = linearInterPolation(y, y1, y2, x00, x01);
	float r1 = linearInterPolation(y, y1, y2, x10, x11);

	return linearInterPolation(z, z1, z2, r0, r1);
}

// project_initialize() is called once when the application is started. You may
// perform any one-time initialization here. By default, project_initialize()
// just loads the volume data.
bool project_initialize() {
	///////////////////////////////////////////////////////////////////////////////////////////////////
	// Try to load a volume
	// Switch here to load a different volume.

	// push all possible data-files; 
	// IMPORTANT! should be pushed in the same order as the order of the enum TRANSFERTYPE
	global_files.push_back("data/bonsai_small.mhd");
	global_files.push_back("data/backpack_small.mhd");

	//global_files_idx = 1;
	//global_ttype = TRANSFERTYPE::BACKPACK;
	global_files_idx = 0;
	global_ttype = TRANSFERTYPE::BONSAI;
	gVolume = load_mhd_volume(global_files[global_files_idx]);

	if (0 == gVolume.total_element_count()) {
		std::fprintf(stderr, "Error: couldn't load volume\n");
		return false;
	}

	gVolumeLargestDimension = std::max(gVolume.width(), std::max(gVolume.height(), gVolume.depth()));

	// TODO: if you have other initialization code that needs to run once when
	// the program starts, you can place it here. (For example, Optional Tasks
	// 3{a,b,c,d} require such one-time initialization.)
	gLightChanged = true;
	glGenBuffers(1, &gColorVBO);
	glGenBuffers(1, &gPositionVBO);

	
	// 2b
	gVolumeSmall = load_lod_volume(gVolume);
	vols.push_back(gVolume);
	vols.push_back(gVolumeSmall);

	// Check for OpenGL errors.
	auto err = glGetError();
	if (GL_NO_ERROR != err) {
		std::fprintf(stderr, "OpenGL Error: %s\n", gluErrorString(err));
		return false;
	}

	return true;
}

// project_draw_window() is called whenever the window needs to be redrawn.
// Just before project_draw_window(), the screen is cleared and the camera
// matrices are set up.
//
// project_draw_window() is where you will implement the various visualization
// modes.
void project_draw_window(Vec3Df const& aCameraFwd, Vec3Df const& aCameraUp, Vec3Df const& aCameraPos) {

	// We start by resetting the state that will eventually be modified.  If
	// you want to manage the OpenGL state by yourself more explicitly, you may
	// do so, but we suggest that you leave this as-is, and simply set up the
	// necessary state each time you use it.
	glPointSize(1.f);
	glColor3f(1.f, 1.f, 1.f);
	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);

	// Make sure the volume is centered.
	Vec3Df volumeTranslation(
		float(gVolumeLargestDimension - gVolume.width()),
		float(gVolumeLargestDimension - gVolume.height()),
		float(gVolumeLargestDimension - gVolume.depth())
	);

	volumeTranslation /= float(gVolumeLargestDimension);
	glTranslatef(volumeTranslation[0], volumeTranslation[1], volumeTranslation[2]);


	// Select based on the current visualization mode. See EVisualizeMode and
	// gVisualizeMode above for more information.
	switch (gVisualizeMode) {

	case EVisualizeMode::none: {
		// Draw a placeholder for the volume.
		float const maxX = 2.f*float(gVolume.width()) / gVolumeLargestDimension - 1.f;
		float const maxY = 2.f*float(gVolume.height()) / gVolumeLargestDimension - 1.f;
		float const maxZ = 2.f*float(gVolume.depth()) / gVolumeLargestDimension - 1.f;

		// draw outer cube
		global_width = maxX + 1.0f;
		global_height = maxY + 1.0f;
		global_depth = maxZ + 1.0f;
		global_position = Vec3Df(-1.f, -1.f, -1.f);

		gSelectiveRegionType = ESelectiveRegionType::cube;
		drawCube(global_width, global_height, global_depth, global_position);
	} break;

	case EVisualizeMode::solidPoints: {
		// code for Task 1: (uncomment code to get results of images in report).

		// BONSAI:
		// trunk:
		//IsoSurface trunk = IsoSurface(0.5f, 0.6f);
		IsoSurface trunk = IsoSurface(0.2f, 0.6f);
		//IsoSurface trunk = IsoSurface(FLT_MAX, FLT_MIN); // ignores trunk when this line is uncommented

		// leaves:
		IsoSurface leaves = IsoSurface(0.15f, 0.17f);
		//IsoSurface leaves = IsoSurface(FLT_MAX, FLT_MIN); // ignores leaves when this line is uncommented

		// BACKPACK:
		IsoSurface lightgrey = IsoSurface(0.25f, 0.3f);
		//IsoSurface lightgrey = IsoSurface(FLT_MAX, FLT_MIN);
		IsoSurface darkgrey = IsoSurface(0.23f, 0.25f);
		//IsoSurface darkgrey = IsoSurface(FLT_MAX, FLT_MIN);
		IsoSurface red = IsoSurface(0.9f, 1.0f);
		//IsoSurface red = IsoSurface(FLT_MAX, FLT_MIN);
		IsoSurface lightblue = IsoSurface(0.6f, 0.9f);
		//IsoSurface lightblue = IsoSurface(FLT_MAX, FLT_MIN);
		IsoSurface yellow = IsoSurface(0.5f, 0.55f);
		//IsoSurface yellow = IsoSurface(FLT_MAX, FLT_MIN);

		glPointSize(2.f);
		glBegin(GL_POINTS);
		for (int i = 0; i < gVolume.width(); i++) {
			for (int j = 0; j < gVolume.height(); j++) {
				for (int k = 0; k < gVolume.depth(); k++) {

					float const X = 2.f*float(i) / gVolumeLargestDimension - 1.f;
					float const Y = 2.f*float(j) / gVolumeLargestDimension - 1.f;
					float const Z = 2.f*float(k) / gVolumeLargestDimension - 1.f;

					// Don't do anything if the coords are outside the ESelectiveRegionType::{shape}
					if (!checkIntersection(X, Y, Z)) continue;

					float density = gVolume(i, j, k);

					switch (global_ttype) {
					case TRANSFERTYPE::BONSAI:
						// Task 1.a: (comment 1.b and uncomment 1.a)
						// if (trunk.hasAfter(density)) {
						// 	 glVertex3f(X, Y, Z);
						// }

						// Task 1.b:
						if (trunk.hasBetween(density)) {
							glColor3f(.33f, .21f, .1f); // brown
							glVertex3f(X, Y, Z);
						} else if (leaves.hasBetween(density)) {
							glColor3f(.3f, .66f, .23f); // green
							glVertex3f(X, Y, Z);
						}
						break;
					case TRANSFERTYPE::BACKPACK:
						// Backpack was not asked for Task 1.
						if (lightgrey.hasBetween(density)) {
							glColor3f(0.85f * 0.05f, 0.85f* 0.05f, 0.85f* 0.05f);
							glVertex3f(X, Y, Z);
						} else if (darkgrey.hasBetween(density)) {
							glColor3f(0.66f* 0.05f, 0.66f* 0.05f, 0.66f* 0.05f);
							glVertex3f(X, Y, Z);
						} else if (red.hasBetween(density)) {
							glColor3f(1.0f, 0.0f, 0.0f); // red
							glVertex3f(X, Y, Z);
						} else if (lightblue.hasBetween(density)) {
							glColor3f(0.0f, 1.0, 1.0f); // lightblue
							glVertex3f(X, Y, Z);
						} else if (yellow.hasBetween(density)) {
							glColor3f(1.0f, 1.0f, 0.0f); // yellow
							glVertex3f(X, Y, Z);
						}
						break;
					default:
						break;
					}
				}
			}
		}
		glEnd();
	} break;

	case EVisualizeMode::additivePoints: {
		// code for Task 2:

		// add additive blending
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE, GL_ONE);
		// disable depth test, so we can draw all pixels and not only the ones closest to the camera
		glDisable(GL_DEPTH_TEST);
		glPointSize(2.f);

		// example from pdf:
		IsoSurface all = IsoSurface(0.15f, 1.0f);

		// BONSAI:
		IsoSurface trunk = IsoSurface(0.2f, 0.6f);
		IsoSurface leaves = IsoSurface(0.15f, 0.17f);

		// BACKPACK:
		IsoSurface lightgrey = IsoSurface(0.25f, 0.3f);
		//IsoSurface lightgrey = IsoSurface(FLT_MAX, FLT_MIN);
		IsoSurface darkgrey = IsoSurface(0.23f, 0.25f);
		//IsoSurface darkgrey = IsoSurface(FLT_MAX, FLT_MIN);
		IsoSurface red = IsoSurface(0.9f, 1.0f);
		//IsoSurface red = IsoSurface(FLT_MAX, FLT_MIN);
		IsoSurface lightblue = IsoSurface(0.6f, 0.9f);
		//IsoSurface lightblue = IsoSurface(FLT_MAX, FLT_MIN);
		IsoSurface yellow = IsoSurface(0.5f, 0.55f);
		//IsoSurface yellow = IsoSurface(FLT_MAX, FLT_MIN);

		glBegin(GL_POINTS);
		for (int i = 0; i < gVolume.width(); i++) {
			for (int j = 0; j < gVolume.height(); j++) {
				for (int k = 0; k < gVolume.depth(); k++) {
					float const X = 2.f*float(i) / gVolumeLargestDimension - 1.f;
					float const Y = 2.f*float(j) / gVolumeLargestDimension - 1.f;
					float const Z = 2.f*float(k) / gVolumeLargestDimension - 1.f;

					// Don't do anything if the coords are outside the ESelectiveRegionType::{shape}
					if (!checkIntersection(X, Y, Z)) continue;

					// this point is a point of the volume itself.
					float density = gVolume(i, j, k);

					//You can experiment with values around 0.001 to 0.05
					switch (global_ttype) {
					case TRANSFERTYPE::BONSAI:
						// Example from PDF (monotone):
						// if (all.hasAfter(density)) {
						//	 glColor3f(density * 0.05f, density * 0.05f, density * 0.05f);
						//	 glVertex3f(X, Y, Z);
						// }

						if (trunk.hasBetween(density)) {
							glColor3f(density *.33f* 0.1f, density *.21f* 0.1f, density * .1f* 0.1f); // brown
							glVertex3f(X, Y, Z);
						} else if (leaves.hasBetween(density)) {
							glColor3f(density *.3f* 0.1f, density * .66f* 0.1f, density * .23f* 0.1f); // green
							glVertex3f(X, Y, Z);
						}
						break;
					case TRANSFERTYPE::BACKPACK:
						// Example from PDF (monotone):
						// if (all.hasAfter(density)) {
						//	 glColor3f(density * 0.05f, density * 0.05f, density * 0.05f);
						//	 glVertex3f(X, Y, Z);
						// }

						if (lightgrey.hasBetween(density)) {
							glColor3f(density * 0.85f * 0.1f, density * 0.85f* 0.1f, density * 0.85f* 0.1f);
							glVertex3f(X, Y, Z);
						} else if (darkgrey.hasBetween(density)) {
							glColor3f(density * 0.66f * 0.1f, density * 0.66f* 0.1f, density * 0.66f* 0.1f);
							glVertex3f(X, Y, Z);
						} else if (red.hasBetween(density)) {
							glColor3f(density *1.0f* 0.1f, 0.0f, 0.0f); // red
							glVertex3f(X, Y, Z);
						} else if (lightblue.hasBetween(density)) {
							glColor3f(0.0f, density *1.0f* 0.1f, density * 1.0f* 0.1f); // lightblue
							glVertex3f(X, Y, Z);
						} else if (yellow.hasBetween(density)) {
							glColor3f(density *1.0f* 0.1f, density *1.0f* 0.1f, 0.0f); // yellow
							glVertex3f(X, Y, Z);
						}
						break;
					default:
						break;
					}
				}
			}
		}
		glEnd();

	} break;

	case EVisualizeMode::colorAlphaPoints: {
		// code for Task 3 goes here!
		// Note: start with Task 3a and then update it for Task 3b.

		// add blending + alpha
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glDisable(GL_DEPTH_TEST);
		glPointSize(2.f);
		glBegin(GL_POINTS);

		AXIS axis = (abs(aCameraFwd[0]) >= abs(aCameraFwd[1]) && abs(aCameraFwd[0]) >= abs(aCameraFwd[2])) ? AXIS::X : (abs(aCameraFwd[1]) >= abs(aCameraFwd[0]) && abs(aCameraFwd[1]) >= abs(aCameraFwd[2])) ? AXIS::Y : AXIS::Z;

		int p = axis == AXIS::X ? gVolume.width() : axis == AXIS::Y ? gVolume.height() : gVolume.depth(); // x y z
		int q = axis == AXIS::X ? gVolume.height() : axis == AXIS::Y ? gVolume.depth() : gVolume.width(); // y z x
		int r = axis == AXIS::X ? gVolume.depth() : axis == AXIS::Y ? gVolume.width() : gVolume.height(); // z x y
		DIRECTION dir = aCameraFwd[axis == AXIS::X ? 0 : axis == AXIS::Y ? 1 : 2] > 0 ? DIRECTION::POS : DIRECTION::NEG;

		if (dir == DIRECTION::POS) {
			for (int k = 0; k < p; k += 1) {
				for (int j = 0; j < q; j += 1) {
					for (int i = 0; i < r; i += 1) {
						performTransfer(axis, i, j, k);
					}
				}
			}
		} else if (dir == DIRECTION::NEG) {
			for (int k = p - 1; k >= 0; k -= 1) {
				for (int j = q - 1; j >= 0; j -= 1) {
					for (int i = r - 1; i >= 0; i -= 1) {
						performTransfer(axis, i, j, k);
					}
				}
			}
		}
		glEnd();

	} break;

	case EVisualizeMode::phongPoints: {
		// code for Task 4 goes here!
		// add blending + alpha
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glDisable(GL_DEPTH_TEST);
		glPointSize(2.f);
		glBegin(GL_POINTS);

		AXIS axis = (abs(aCameraFwd[0]) >= abs(aCameraFwd[1]) && abs(aCameraFwd[0]) >= abs(aCameraFwd[2])) ? AXIS::X : (abs(aCameraFwd[1]) >= abs(aCameraFwd[0]) && abs(aCameraFwd[1]) >= abs(aCameraFwd[2])) ? AXIS::Y : AXIS::Z;

		int p = axis == AXIS::X ? gVolume.width() : axis == AXIS::Y ? gVolume.height() : gVolume.depth(); // x y z
		int q = axis == AXIS::X ? gVolume.height() : axis == AXIS::Y ? gVolume.depth() : gVolume.width(); // y z x
		int r = axis == AXIS::X ? gVolume.depth() : axis == AXIS::Y ? gVolume.width() : gVolume.height(); // z x y
		DIRECTION dir = aCameraFwd[axis == AXIS::X ? 0 : axis == AXIS::Y ? 1 : 2] > 0 ? DIRECTION::POS : DIRECTION::NEG;

		// use gLightPosition and gLightChanged
		Vec3Df lightpos = gLightPosition;

		if (dir == DIRECTION::POS) {
			for (int k = 0; k < p; k += 1) {
				for (int j = 0; j < q; j += 1) {
					for (int i = 0; i < r; i += 1) {
						performTransferWithLighting(axis, i, j, k, p, q, r, lightpos);
					}
				}
			}
		} else if (dir == DIRECTION::NEG) {
			for (int k = p - 1; k >= 0; k -= 1) {
				for (int j = q - 1; j >= 0; j -= 1) {
					for (int i = r - 1; i >= 0; i -= 1) {
						performTransferWithLighting(axis, i, j, k, p, q, r, lightpos);
					}
				}
			}
		}
		glEnd();
	} break;

	case EVisualizeMode::selectedPointsOnly: {
		// code for Task 5 goes here!
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		switch (gSelectiveRegionType) {
		case ESelectiveRegionType::sphere:
			drawSphere(global_radius, global_position);
			break;
		case ESelectiveRegionType::cube:
			drawCube(global_width, global_height, global_depth, global_position);
			break;
		case ESelectiveRegionType::slab:
			drawSlab(global_slab_length, global_slab_axis);
			break;
		default:
			break;
		}
	} break;

	case EVisualizeMode::enhanceSelectedPoints: {
		// code for Optional Task 1 goes here!
		// add blending + alpha
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glDisable(GL_DEPTH_TEST);
		glPointSize(2.f);
		glBegin(GL_POINTS);

		AXIS axis = (abs(aCameraFwd[0]) >= abs(aCameraFwd[1]) && abs(aCameraFwd[0]) >= abs(aCameraFwd[2])) ? AXIS::X : (abs(aCameraFwd[1]) >= abs(aCameraFwd[0]) && abs(aCameraFwd[1]) >= abs(aCameraFwd[2])) ? AXIS::Y : AXIS::Z;

		int p = axis == AXIS::X ? gVolume.width() : axis == AXIS::Y ? gVolume.height() : gVolume.depth(); // x y z
		int q = axis == AXIS::X ? gVolume.height() : axis == AXIS::Y ? gVolume.depth() : gVolume.width(); // y z x
		int r = axis == AXIS::X ? gVolume.depth() : axis == AXIS::Y ? gVolume.width() : gVolume.height(); // z x y
		DIRECTION dir = aCameraFwd[axis == AXIS::X ? 0 : axis == AXIS::Y ? 1 : 2] > 0 ? DIRECTION::POS : DIRECTION::NEG;

		// use gLightPosition and gLightChanged
		Vec3Df lightpos = gLightPosition;

		Vec3Df cameraPos = aCameraPos;
		Vec3Df center = Vec3Df(0, 0, 0);
		float distance = Vec3Df::distance(center, cameraPos);

		if (dir == DIRECTION::POS) {
			for (int k = 0; k < p; k += 1) {
				for (int j = 0; j < q; j += 1) {
					for (int i = 0; i < r; i += 1) {
						performTransferWithTrilinearInterpolation(axis, i, j, k, p, q, r, lightpos, distance < 2.0f);
					}
				}
			}
		} else if (dir == DIRECTION::NEG) {
			for (int k = p - 1; k >= 0; k -= 1) {
				for (int j = q - 1; j >= 0; j -= 1) {
					for (int i = r - 1; i >= 0; i -= 1) {
						performTransferWithTrilinearInterpolation(axis, i, j, k, p, q, r, lightpos, distance < 2.0f);
					}
				}
			}
		}
		glEnd();

	} break;

	case EVisualizeMode::billboards: {
		// code for Optional Task 2a goes here!
		//TODO: your code for Optional Task 1 goes here!
		// add blending + alpha
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glDisable(GL_DEPTH_TEST);

		AXIS axis = (abs(aCameraFwd[0]) >= abs(aCameraFwd[1]) && abs(aCameraFwd[0]) >= abs(aCameraFwd[2])) ? AXIS::X : (abs(aCameraFwd[1]) >= abs(aCameraFwd[0]) && abs(aCameraFwd[1]) >= abs(aCameraFwd[2])) ? AXIS::Y : AXIS::Z;

		int p = axis == AXIS::X ? gVolume.width() : axis == AXIS::Y ? gVolume.height() : gVolume.depth(); // x y z
		int q = axis == AXIS::X ? gVolume.height() : axis == AXIS::Y ? gVolume.depth() : gVolume.width(); // y z x
		int r = axis == AXIS::X ? gVolume.depth() : axis == AXIS::Y ? gVolume.width() : gVolume.height(); // z x y
		DIRECTION dir = aCameraFwd[axis == AXIS::X ? 0 : axis == AXIS::Y ? 1 : 2] > 0 ? DIRECTION::POS : DIRECTION::NEG;

		// use gLightPosition and gLightChanged
		Vec3Df lightpos = gLightPosition;

		Vec3Df cameraPos = aCameraPos;
		Vec3Df center = Vec3Df(0, 0, 0);
		float distance = Vec3Df::distance(center, cameraPos);

		// Billboards params
		Vec3Df right = Vec3Df::crossProduct(aCameraUp, aCameraFwd);
		Vec3Df up = aCameraUp;
		float size = 0.004f;

		glBegin(GL_QUADS);
		if (dir == DIRECTION::POS) {
			for (int k = 0; k < p; k += 1) {
				for (int j = 0; j < q; j += 1) {
					for (int i = 0; i < r; i += 1) {
						performTransferWithBillboards(axis, i, j, k, p, q, r, lightpos, distance < 2.0f, right, up, size);
					}
				}
			}
		} else if (dir == DIRECTION::NEG) {
			for (int k = p - 1; k >= 0; k -= 1) {
				for (int j = q - 1; j >= 0; j -= 1) {
					for (int i = r - 1; i >= 0; i -= 1) {						
						performTransferWithBillboards(axis, i, j, k, p, q, r, lightpos, distance < 2.0f, right, up, size);
					}
				}
			}
		}
		glEnd();
	} break;

	case EVisualizeMode::billboardsWithLOD: {
		//TODO: your code for Optional Task 2b goes here!
		// IMPORTANT: Task 2b is not fully working properly.
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glDisable(GL_DEPTH_TEST);

		AXIS axis = (abs(aCameraFwd[0]) >= abs(aCameraFwd[1]) && abs(aCameraFwd[0]) >= abs(aCameraFwd[2])) ? AXIS::X : (abs(aCameraFwd[1]) >= abs(aCameraFwd[0]) && abs(aCameraFwd[1]) >= abs(aCameraFwd[2])) ? AXIS::Y : AXIS::Z;

		int p = axis == AXIS::X ? gVolume.width() : axis == AXIS::Y ? gVolume.height() : gVolume.depth(); // x y z
		int q = axis == AXIS::X ? gVolume.height() : axis == AXIS::Y ? gVolume.depth() : gVolume.width(); // y z x
		int r = axis == AXIS::X ? gVolume.depth() : axis == AXIS::Y ? gVolume.width() : gVolume.height(); // z x y
		DIRECTION dir = aCameraFwd[axis == AXIS::X ? 0 : axis == AXIS::Y ? 1 : 2] > 0 ? DIRECTION::POS : DIRECTION::NEG;

		// use gLightPosition and gLightChanged
		Vec3Df lightpos = gLightPosition;

		Vec3Df cameraPos = aCameraPos;
		Vec3Df center = Vec3Df(0, 0, 0);
		float distance = Vec3Df::distance(center, cameraPos);

		// Billboards params
		Vec3Df right = Vec3Df::crossProduct(aCameraUp, aCameraFwd);
		Vec3Df up = aCameraUp;
		float size = 0.004f;

		if (distance > 8.0f && global_vols_idx == 0) {
			global_vols_idx = 1;
			gVolume = vols[global_vols_idx];
			size *= 2.0f;
		} else if (distance < 8.0f && global_vols_idx == 1) {
			global_vols_idx = 0;
			gVolume = vols[global_vols_idx];
			size /= 2.0f;
		}

		glBegin(GL_QUADS);
		if (dir == DIRECTION::POS) {
			for (int k = 0; k < p; k += 1) {
				for (int j = 0; j < q; j += 1) {
					for (int i = 0; i < r; i += 1) {
						performTransferWithBillboards(axis, i, j, k, p, q, r, lightpos, distance < 2.0f, right, up, size);
					}
				}
			}
		} else if (dir == DIRECTION::NEG) {
			for (int k = p - 1; k >= 0; k -= 1) {
				for (int j = q - 1; j >= 0; j -= 1) {
					for (int i = r - 1; i >= 0; i -= 1) {
						performTransferWithBillboards(axis, i, j, k, p, q, r, lightpos, distance < 2.0f, right, up, size);
					}
				}
			}
		}
		glEnd();
	} break;

	case EVisualizeMode::drawAsArray: {
		//TODO: your code for Optional Task 3a goes here!

		// determine whether or not you need to update the data when the visualization
		// mode is EVisualizeMode::drawAsArray or EVisualizeMode::drawAsArrayFromVRAM.
		// gLightChanged
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glDisable(GL_DEPTH_TEST);
		AXIS axis = (abs(aCameraFwd[0]) >= abs(aCameraFwd[1]) && abs(aCameraFwd[0]) >= abs(aCameraFwd[2])) ? AXIS::X : (abs(aCameraFwd[1]) >= abs(aCameraFwd[0]) && abs(aCameraFwd[1]) >= abs(aCameraFwd[2])) ? AXIS::Y : AXIS::Z;
		DIRECTION dir = aCameraFwd[axis == AXIS::X ? 0 : axis == AXIS::Y ? 1 : 2] > 0 ? DIRECTION::POS : DIRECTION::NEG;
		
		if (gLightChanged || dir != current_dir) {
			current_dir = dir;
			gLightChanged = false;
			gDrawColors.clear();
			gDrawPositions.clear();

			int p = axis == AXIS::X ? gVolume.width() : axis == AXIS::Y ? gVolume.height() : gVolume.depth(); // x y z
			int q = axis == AXIS::X ? gVolume.height() : axis == AXIS::Y ? gVolume.depth() : gVolume.width(); // y z x
			int r = axis == AXIS::X ? gVolume.depth() : axis == AXIS::Y ? gVolume.width() : gVolume.height(); // z x y

			Vec3Df lightpos = gLightPosition;

			if (dir == DIRECTION::POS) {
				for (int k = 0; k < p; k += 1) {
					for (int j = 0; j < q; j += 1) {
						for (int i = 0; i < r; i += 1) {
							performTransferWithArrays(axis, i, j, k, p, q, r, lightpos);
						}
					}
				}
			} else if (dir == DIRECTION::NEG) {
				for (int k = p - 1; k >= 0; k -= 1) {
					for (int j = q - 1; j >= 0; j -= 1) {
						for (int i = r - 1; i >= 0; i -= 1) {
							performTransferWithArrays(axis, i, j, k, p, q, r, lightpos);
						}
					}
				}
			}
		}

		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_COLOR_ARRAY);

		glColorPointer(4, GL_FLOAT, 0, gDrawColors.data());
		glVertexPointer(3, GL_FLOAT, 0, gDrawPositions.data());
		glDrawArrays(GL_POINTS, 0, int(gDrawPositions.size() / 3));
		glDisableClientState(GL_COLOR_ARRAY);
		glDisableClientState(GL_VERTEX_ARRAY);
	} break;

	case EVisualizeMode::drawAsArrayFromVRAM: {
		//TODO: your code for Optional Task 3b goes here!
		// Note: change this code for Optional Task 3d

		// You cannot call this function after calling 9:

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glDisable(GL_DEPTH_TEST);
		AXIS axis = (abs(aCameraFwd[0]) >= abs(aCameraFwd[1]) && abs(aCameraFwd[0]) >= abs(aCameraFwd[2])) ? AXIS::X : (abs(aCameraFwd[1]) >= abs(aCameraFwd[0]) && abs(aCameraFwd[1]) >= abs(aCameraFwd[2])) ? AXIS::Y : AXIS::Z;
		DIRECTION dir = aCameraFwd[axis == AXIS::X ? 0 : axis == AXIS::Y ? 1 : 2] > 0 ? DIRECTION::POS : DIRECTION::NEG;

		if (gLightChanged || dir != current_dir) {
			current_dir = dir;
			gLightChanged = false;
			gDrawColors.clear();
			gDrawPositions.clear();

			int p = axis == AXIS::X ? gVolume.width() : axis == AXIS::Y ? gVolume.height() : gVolume.depth(); // x y z
			int q = axis == AXIS::X ? gVolume.height() : axis == AXIS::Y ? gVolume.depth() : gVolume.width(); // y z x
			int r = axis == AXIS::X ? gVolume.depth() : axis == AXIS::Y ? gVolume.width() : gVolume.height(); // z x y

			Vec3Df lightpos = gLightPosition;

			if (dir == DIRECTION::POS) {
				for (int k = 0; k < p; k += 1) {
					for (int j = 0; j < q; j += 1) {
						for (int i = 0; i < r; i += 1) {
							performTransferWithArrays(axis, i, j, k, p, q, r, lightpos);
						}
					}
				}
			} else if (dir == DIRECTION::NEG) {
				for (int k = p - 1; k >= 0; k -= 1) {
					for (int j = q - 1; j >= 0; j -= 1) {
						for (int i = r - 1; i >= 0; i -= 1) {
							performTransferWithArrays(axis, i, j, k, p, q, r, lightpos);
						}
					}
				}
			}

			glBindBuffer(GL_ARRAY_BUFFER, gPositionVBO);
			glBufferData(GL_ARRAY_BUFFER, gDrawPositions.size() * sizeof(float), gDrawPositions.data(), GL_STREAM_DRAW);
			glBindBuffer(GL_ARRAY_BUFFER, gColorVBO);
			glBufferData(GL_ARRAY_BUFFER, gDrawColors.size() * sizeof(float), gDrawColors.data(), GL_STREAM_DRAW);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
		}
		// At run-time:
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_COLOR_ARRAY);

		glBindBuffer(GL_ARRAY_BUFFER, gColorVBO);
		glColorPointer(4, GL_FLOAT, 0, nullptr);
		glBindBuffer(GL_ARRAY_BUFFER, gPositionVBO);
		glVertexPointer(3, GL_FLOAT, 0, nullptr);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glDrawArrays(GL_POINTS, 0, int(gDrawPositions.size() / 3));

		glDisableClientState(GL_COLOR_ARRAY);
		glDisableClientState(GL_VERTEX_ARRAY);

	} break;
	}

	auto err = glGetError();
	if (GL_NO_ERROR != err)
		std::fprintf(stderr, "OpenGL Error: %s\n", gluErrorString(err));
}

void project_on_key_press(int aKey, Vec3Df const& aCameraPos) {
	switch (aKey) {
		// Keys 0-9 are used to switch between the different visualization
		// modes that you will implement.
	case '1': gVisualizeMode = EVisualizeMode::solidPoints; break;
	case '2': gVisualizeMode = EVisualizeMode::additivePoints; break;
	case '3': gVisualizeMode = EVisualizeMode::colorAlphaPoints; break;
	case '4': gVisualizeMode = EVisualizeMode::phongPoints; break;
	case '5': gVisualizeMode = EVisualizeMode::selectedPointsOnly; break;
	case '6': gVisualizeMode = EVisualizeMode::enhanceSelectedPoints; break;
	case '7': gVisualizeMode = EVisualizeMode::billboards; break;
	case '8': gVisualizeMode = EVisualizeMode::billboardsWithLOD; break;
	case '9': gVisualizeMode = EVisualizeMode::drawAsArray; break;
	case '0': gVisualizeMode = EVisualizeMode::drawAsArrayFromVRAM; break;
	case 'n': gVisualizeMode = EVisualizeMode::none; break;
	case 'p':
		gVisualizeMode = EVisualizeMode::none;
		gLightChanged = true; // h4ck
		global_files_idx = (global_files_idx + 1) % global_files.size();
		gVolume = load_mhd_volume(global_files[global_files_idx]);
		global_ttype = TRANSFERTYPE(global_files_idx);
		break;

		// Keys 'l' and 'k' are used to change the light position. The light
		// position is used in Task 4 (Lighting).
	case 'l': gLightPosition = aCameraPos; gLightChanged = true; break;
	case 'k': gLightPosition = Vec3Df(2.f, 2.f, 0.f); gLightChanged = true; break;
		// movement of light: x
	case ',':
		gLightPosition.p[0] += 1.f;
		gLightChanged = true;
		break;
	case '.':
		gLightPosition.p[0] -= 1.f;
		gLightChanged = true;
		break;
		// movement of light: y
	case ';':
		gLightPosition.p[1] += 1.f;
		gLightChanged = true;
		break;
	case '\'':
		gLightPosition.p[1] -= 1.f;
		gLightChanged = true;
		break;
		// movement of light: z
	case '[':
		gLightPosition.p[2] += 1.f;
		gLightChanged = true;
		break;
	case ']':
		gLightPosition.p[2] -= 1.f;
		gLightChanged = true;
		break;

		// Select the region type for Task 5 (Intersection).
	case 't': // sphere gets drawn from the center as starting point
		gSelectiveRegionType = ESelectiveRegionType::sphere;
		// add 1 to the global position, such that it gets drawn center first
		global_position.p[0] += 1.0f;
		global_position.p[1] += 1.0f;
		global_position.p[2] += 1.0f;
		break;
	case 'g': // cube gets drawn with the left bottom vertex V=(minX, minY, miZ) as starting point
		gSelectiveRegionType = ESelectiveRegionType::cube;
		// subtract 1 to the global position, such that it gets drawn center first
		global_position.p[0] -= 1.0f;
		global_position.p[1] -= 1.0f;
		global_position.p[2] -= 1.0f;
		break;
	case 'b': gSelectiveRegionType = ESelectiveRegionType::slab; break;
	case 'w': // move z+ (towards you)
		switch (gSelectiveRegionType) {
		case ESelectiveRegionType::sphere:
		case ESelectiveRegionType::cube:
			global_position.p[2] -= 0.1f;
			break;
		case ESelectiveRegionType::slab:
			if (global_slab_axis == AXIS::Z) {
				global_position.p[2] -= 0.1f;
			}
			break;
		default:
			break;
		}
		break;
	case 's': // move z- (into depth)
		switch (gSelectiveRegionType) {
		case ESelectiveRegionType::sphere:
		case ESelectiveRegionType::cube:
			global_position.p[2] += 0.1f;
			break;
		case ESelectiveRegionType::slab:
			if (global_slab_axis == AXIS::Z) {
				global_position.p[2] += 0.1f;
			}
			break;
		default:
			break;
		}
		break;
	case 'a': // move x-
		switch (gSelectiveRegionType) {
		case ESelectiveRegionType::sphere:
		case ESelectiveRegionType::cube:
			global_position.p[0] -= 0.1f;
			break;
		case ESelectiveRegionType::slab:
			if (global_slab_axis == AXIS::X) {
				global_position.p[0] -= 0.1f;
			}
			break;
		default:
			break;
		}
		break;
	case 'd': // move x+
		switch (gSelectiveRegionType) {
		case ESelectiveRegionType::sphere:
		case ESelectiveRegionType::cube:
			global_position.p[0] += 0.1f;
			break;
		case ESelectiveRegionType::slab:
			if (global_slab_axis == AXIS::X) {
				global_position.p[0] += 0.1f;
			}
			break;
		default:
			break;
		}
		break;
	case 'q': // move y-
		switch (gSelectiveRegionType) {
		case ESelectiveRegionType::sphere:
		case ESelectiveRegionType::cube:
			global_position.p[1] -= 0.1f;
			break;
		case ESelectiveRegionType::slab:
			if (global_slab_axis == AXIS::Y) {
				global_position.p[1] -= 0.1f;
			}
			break;
		default:
			break;
		}
		break;
	case 'e': // move y+
		switch (gSelectiveRegionType) {
		case ESelectiveRegionType::sphere:
		case ESelectiveRegionType::cube:
			global_position.p[1] += 0.1f;
			break;
		case ESelectiveRegionType::slab:
			if (global_slab_axis == AXIS::Y) {
				global_position.p[1] += 0.1f;
			}
			break;
		default:
			break;
		}
		break;
	case 'x': // change radius/size +
		switch (gSelectiveRegionType) {
		case ESelectiveRegionType::sphere:
			global_radius += 0.1f;
			break;
		case ESelectiveRegionType::cube:
			global_width += 0.1f;
			global_height += 0.1f;
			global_width += 0.1f;
			break;
		case ESelectiveRegionType::slab:
			global_slab_length += 0.1f;
			break;
		default:
			break;
		}
		break;
	case 'z': // change radius/size -
		switch (gSelectiveRegionType) {
		case ESelectiveRegionType::sphere:
			global_radius -= 0.1f;
			break;
		case ESelectiveRegionType::cube:
			global_width -= 0.1f;
			global_height -= 0.1f;
			global_width -= 0.1f;
			break;
		case ESelectiveRegionType::slab:
			global_slab_length -= 0.1f;
			break;
		default:
			break;
		}
		break;
	case 'o':
		global_slab_axis = AXIS((static_cast<int>(global_slab_axis) + 1) % 3);
		break;
		// Any remaining keys, you may use as you wish.
		// Suggestion: for Task 5 (Intersection), use e.g., 'w', 's', 'a', 'd',
		// 'e', 'q' to move the sphere/cube along the three axes, 'x' 'z' to
		// change the radius/size. Pick a good set of keys for the slab by
		// yourself. You may also use the project_interact_mouse_wheel() below.
	}
}

void project_interact_mouse_wheel(bool aUp) {
	// You can use this function however you wish. Make sure it receives the
	// mouse wheel events.
}
