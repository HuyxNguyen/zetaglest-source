// ==============================================================
//	This file is part of Glest (www.glest.org)
//
//	Copyright (C) 2001-2008 Martiño Figueroa
//
//	You can redistribute this code and/or modify it under
//	the terms of the GNU General Public License as published
//	by the Free Software Foundation; either version 2 of the
//	License, or (at your option) any later version
// ==============================================================


#include "map_preview.h"

#include "math_wrapper.h"
#include <cstdlib>
#include <stdexcept>
#include <set>
#include <iterator>
#include "platform_util.h"
#include "conversion.h"
#include "byte_order.h"

#ifndef WIN32
#include <errno.h>
#endif

using namespace Shared::Util;
using namespace std;

namespace Shared {
	namespace Map {

		// ===============================================
		//	class MapPreview
		// ===============================================

		// ================== PUBLIC =====================

		MapPreview::MapPreview() {
			mapFileLoaded = "";
			fileLoaded = false;
			heightFactor = DEFAULT_MAP_CELL_HEIGHT_FACTOR;
			waterLevel = DEFAULT_MAP_WATER_DEPTH;
			cliffLevel = DEFAULT_CLIFF_HEIGHT;
			cameraHeight = 0;
			//cells = NULL;
			cells.clear();
			//startLocations = NULL;
			startLocations.clear();
			reset(DEFAULT_MAP_CELL_WIDTH, DEFAULT_MAP_CELL_LENGTH, (float) DEFAULT_MAP_CELL_HEIGHT, DEFAULT_MAP_CELL_SURFACE_TYPE);
			resetFactions(DEFAULT_MAP_FACTIONCOUNT);
			title = "";
			desc = "";
			author = "";
			maxFactions = 0;
			refAlt = DEFAULT_MAP_CELL_HEIGHT;
			hasChanged = false;
		}

		MapPreview::~MapPreview() {
			//delete [] startLocations;
			//startLocations = NULL;
			startLocations.clear();

			//for (int i = 0; i < h; i++) {
			//	delete [] cells[i];
			//}
			//delete [] cells;
			//cells = NULL;
			cells.clear();
		}

		float MapPreview::getHeight(int x, int y) const {
			return cells[x][y].height;
		}

		bool MapPreview::isCliff(int x, int y) {
			if (cliffLevel == 0)
				return false;
			for (int k = -1; k <= 1; ++k) {
				for (int l = -1; l <= 1; ++l) {
					int xToCheck = x + l;
					int yToCheck = y + k;
					if (xToCheck < 0 || yToCheck < 0 || xToCheck >= w || yToCheck >= h) {
						//ignore
					} else if (cliffLevel <= abs((int) (getHeight(x, y) - getHeight(xToCheck, yToCheck)))) {
						return true;
					}
				}
			}
			return false;
		}

		MapSurfaceType MapPreview::getSurface(int x, int y) const {
			return static_cast<MapSurfaceType>(cells[x][y].surface);
		}

		int MapPreview::getObject(int x, int y) const {
			return cells[x][y].object;
		}

		int MapPreview::getResource(int x, int y) const {
			return cells[x][y].resource;
		}

		int MapPreview::getStartLocationX(int index) const {
			return startLocations[index].x;
		}

		int MapPreview::getStartLocationY(int index) const {
			return startLocations[index].y;
		}

		static int get_dist(int delta_x, int delta_y) {
			float dx = (float) delta_x;
			float dy = (float) delta_y;
#ifdef USE_STREFLOP
			return static_cast<int>(streflop::sqrtf(static_cast<streflop::Simple>(dx * dx + dy * dy)) + 0.5); // round correctly
#else
			return static_cast<int>(sqrtf(dx * dx + dy * dy) + 0.5); // round correctly
#endif
		}

		void MapPreview::glestChangeHeight(int x, int y, int height, int radius) {

			for (int i = x - radius + 1; i < x + radius; i++) {
				for (int j = y - radius + 1; j < y + radius; j++) {
					if (inside(i, j)) {
						int dist = get_dist(i - x, j - y);
						if (radius > dist) {
							int oldAlt = static_cast<int>(cells[i][j].height);
							int altInc = height * (radius - dist - 1) / radius;
							if (height > 0) {
								altInc++;
							}
							if (height < 0) {
								altInc--;
							}
							int newAlt = refAlt + altInc;
							if ((height > 0 && newAlt > oldAlt) || (height < 0 && newAlt < oldAlt) || height == 0) {
								if (newAlt >= 0 && newAlt <= 20) {
									cells[i][j].height = static_cast<float>(newAlt);
									hasChanged = true;
								}
							}
						}
					}
				}
			}
		}


		void MapPreview::pirateChangeHeight(int x, int y, int height, int radius) {
			// Make sure not to try and blanket change the height over the bounds
			// Find our goal height for the center of the brush

			int overBounds = refAlt + height;
			int goalAlt = overBounds;
			if (overBounds > 20) {
				goalAlt = 20;
			} else if (overBounds < 0) {
				goalAlt = 0;
			}

			// If the radius is 1 don't bother doing any calculations
			if (radius == 1) {
				if (inside(x, y)) {
					cells[x][y].height = (float) goalAlt;
					hasChanged = true;
				}
				return;
			}

			// Get Old height reference points and compute gradients
			// from the heights of the sides and corners of the brush to the center goal height
			float gradient[3][3]; // [i][j]
			int indexI = 0;
			for (int i = x - radius; i <= x + radius; i += radius) {
				int indexJ = 0;
				for (int j = y - radius; j <= y + radius; j += radius) {
					// round off the corners
					int ti = 0, tj = 0;
					if (abs(i - x) == abs(j - y)) {
						ti = (int) ((i - x) * 0.707 + x + 0.5);
						tj = (int) ((j - y) * 0.707 + y + 0.5);
					} else {
						ti = i;
						tj = j;
					}
					if (inside(ti, tj)) {
						gradient[indexI][indexJ] = (cells[ti][tj].height - (float) goalAlt) / (float) radius;
						//} else if (dist == 0) {
							//gradient[indexI][indexJ] = 0;
					} else {
						// assume outside the map bounds is height 10
						gradient[indexI][indexJ] = (10.0f - (float) goalAlt) / (float) radius;
					}
					//std::cout << "gradient[" << indexI << "][" << indexJ << "] = " << gradient[indexI][indexJ] << std::endl;
					//std::cout << "derived from height " << cells[ti][tj].height << " at " << ti << " " << tj << std::endl;
					indexJ++;
				}
				indexI++;
			}
			//std::cout << endl;

			//  // A brush with radius n cells should have a true radius of n-1 distance  // No becasue then "radius" 1==2
			// radius -= 1;
			for (int i = x - radius; i <= x + radius; i++) {
				for (int j = y - radius; j <= y + radius; j++) {
					int dist = get_dist(i - x, j - y);
					if (inside(i, j) && dist < radius) {
						// Normalize di and dj and round them to an int so they can be used as indicies
						float normIf = (float(i - x) / radius);
						float normJf = (float(j - y) / radius);
						int normI[2];
						int normJ[2];
						float usedGrad;

						// Build a search box to find the gradients we are concerned about
						// Find the nearest i indices
						if (normIf < -0.33) {
							normI[0] = 0;
							if (normIf == 0) {
								normI[1] = 0;
							} else {
								normI[1] = 1;
							}
						} else if (normIf < 0.33) {
							normI[0] = 1;
							if (normIf > 0) {
								normI[1] = 2;
							} else if (normIf < 0) {
								normI[1] = 0;
							} else /*(normIf == 0)*/ {
								normI[1] = 1;
							}
						} else {
							normI[0] = 2;
							if (normIf == 1) {
								normI[1] = 2;
							} else {
								normI[1] = 1;
							}
						}
						// find nearest j indices
						if (normJf < -0.33) {
							normJ[0] = 0;
							if (normJf == 0) {
								normJ[1] = 0;
							} else {
								normJ[1] = 1;
							}
						} else if (normJf < 0.33) {
							normJ[0] = 1;
							if (normJf > 0) {
								normJ[1] = 2;
							} else if (normJf < 0) {
								normJ[1] = 0;
							} else /*(normJf == 0)*/ {
								normJ[1] = 1;
							}
						} else {
							normJ[0] = 2;
							if (normJf == 1) {
								normJ[1] = 2;
							} else {
								normJ[1] = 1;
							}
						}

						// Determine which gradients to use and take a weighted average
#ifdef USE_STREFLOP
						if (streflop::fabs(static_cast<streflop::Simple>(normIf)) > streflop::fabs(static_cast<streflop::Simple>(normJf))) {

							usedGrad =
								gradient[normI[0]][normJ[0]] * streflop::fabs(static_cast<streflop::Simple>(normJf)) +
								gradient[normI[0]][normJ[1]] * (1 - streflop::fabs(static_cast<streflop::Simple>(normJf)));
						} else if (streflop::fabs(static_cast<streflop::Simple>(normIf)) < streflop::fabs(static_cast<streflop::Simple>(normJf))) {
							usedGrad =
								gradient[normI[0]][normJ[0]] * streflop::fabs(static_cast<streflop::Simple>(normIf)) +
								gradient[normI[1]][normJ[0]] * (1 - streflop::fabs(static_cast<streflop::Simple>(normIf)));
						} else {
							usedGrad =
								gradient[normI[0]][normJ[0]];
						}

#else
						if (fabs(normIf) > fabs(normJf)) {

							usedGrad =
								gradient[normI[0]][normJ[0]] * fabs(normJf) +
								gradient[normI[0]][normJ[1]] * (1 - fabs(normJf));
						} else if (fabs(normIf) < fabs(normJf)) {
							usedGrad =
								gradient[normI[0]][normJ[0]] * fabs(normIf) +
								gradient[normI[1]][normJ[0]] * (1 - fabs(normIf));
						} else {
							usedGrad =
								gradient[normI[0]][normJ[0]];
						}
#endif

						float newAlt = usedGrad * dist + goalAlt;

						// if the change in height and what is supposed to be the change in height
						// are the same sign then we can change the height
						if (((newAlt - cells[i][j].height) > 0 && height > 0) ||
							((newAlt - cells[i][j].height) < 0 && height < 0) ||
							height == 0) {
							cells[i][j].height = newAlt;
							hasChanged = true;
						}
					}
				}
			}
		}

		void MapPreview::setHeight(int x, int y, float height) {
			cells[x][y].height = height;
			hasChanged = true;
		}

		void MapPreview::setRefAlt(int x, int y) {
			if (inside(x, y)) {
				refAlt = static_cast<int>(cells[x][y].height);
				hasChanged = true;
			}
		}

		void MapPreview::flipX() {
			//Cell **oldCells = cells;
			std::vector<std::vector<Cell> > oldCells = cells;

			//cells = new Cell*[w];
			cells.clear();
			cells.resize(w);
			for (int i = 0; i < w; i++) {
				//cells[i] = new Cell[h];
				cells[i].resize(h);
				for (int j = 0; j < h; j++) {
					cells[i][j].height = oldCells[w - i - 1][j].height;
					cells[i][j].object = oldCells[w - i - 1][j].object;
					cells[i][j].resource = oldCells[w - i - 1][j].resource;
					cells[i][j].surface = oldCells[w - i - 1][j].surface;
				}
			}

			for (int i = 0; i < maxFactions; ++i) {
				startLocations[i].x = w - startLocations[i].x - 1;
			}

			//for (int i = 0; i < w; i++) {
			//	delete [] oldCells[i];
			//}
			//delete [] oldCells;

			hasChanged = true;
		}

		void MapPreview::flipY() {
			//Cell **oldCells = cells;
			std::vector<std::vector<Cell> > oldCells = cells;

			//cells = new Cell*[w];
			cells.clear();
			cells.resize(w);

			for (int i = 0; i < w; i++) {
				//cells[i] = new Cell[h];
				cells[i].resize(h);
				for (int j = 0; j < h; j++) {
					cells[i][j].height = oldCells[i][h - j - 1].height;
					cells[i][j].object = oldCells[i][h - j - 1].object;
					cells[i][j].resource = oldCells[i][h - j - 1].resource;
					cells[i][j].surface = oldCells[i][h - j - 1].surface;
				}
			}

			for (int i = 0; i < maxFactions; ++i) {
				startLocations[i].y = h - startLocations[i].y - 1;
			}

			//for (int i = 0; i < w; i++) {
			//	delete [] oldCells[i];
			//}
			//delete [] oldCells;

			hasChanged = true;
		}

		// Copy a cell in the map from one cell to another, used by MirrorXY etc
		void MapPreview::copyXY(int x, int y, int sx, int sy) {
			cells[x][y].height = cells[sx][sy].height;
			cells[x][y].object = cells[sx][sy].object;
			cells[x][y].resource = cells[sx][sy].resource;
			cells[x][y].surface = cells[sx][sy].surface;

			hasChanged = true;
		}

		// swap a cell in the map with another, used by rotate etc
		void MapPreview::swapXY(int x, int y, int sx, int sy) {
			if (inside(x, y) && inside(sx, sy)) {
				float tmpHeight = cells[x][y].height;
				cells[x][y].height = cells[sx][sy].height;
				cells[sx][sy].height = tmpHeight;

				int tmpObject = cells[x][y].object;
				cells[x][y].object = cells[sx][sy].object;
				cells[sx][sy].object = tmpObject;

				int tmpResource = cells[x][y].resource;
				cells[x][y].resource = cells[sx][sy].resource;
				cells[sx][sy].resource = tmpResource;

				int tmpSurface = cells[x][y].surface;
				cells[x][y].surface = cells[sx][sy].surface;
				cells[sx][sy].surface = tmpSurface;

				hasChanged = true;
			}
		}

		void MapPreview::changeSurface(int x, int y, MapSurfaceType surface, int radius) {
			int i = 0, j = 0;
			int dist = 0;

			for (i = x - radius + 1; i < x + radius; i++) {
				for (j = y - radius + 1; j < y + radius; j++) {
					if (inside(i, j)) {
						dist = get_dist(i - x, j - y);
						if (radius > dist) {  // was >=
							cells[i][j].surface = surface;
							hasChanged = true;
						}
					}
				}
			}
		}

		void MapPreview::setSurface(int x, int y, MapSurfaceType surface) {
			cells[x][y].surface = surface;
			hasChanged = true;
		}

		void MapPreview::changeObject(int x, int y, int object, int radius) {
			int i = 0, j = 0;
			int dist = 0;

			for (i = x - radius + 1; i < x + radius; i++) {
				for (j = y - radius + 1; j < y + radius; j++) {
					if (inside(i, j)) {
						dist = get_dist(i - x, j - y);
						if (radius > dist) {  // was >=
							cells[i][j].object = object;
							cells[i][j].resource = 0;
							hasChanged = true;
						}
					}
				}
			}
		}

		void MapPreview::setObject(int x, int y, int object) {
			cells[x][y].object = object;
			if (object != 0) {
				cells[x][y].resource = 0;
			}
			hasChanged = true;
		}

		void MapPreview::changeResource(int x, int y, int resource, int radius) {
			int i = 0, j = 0;
			int dist = 0;

			for (i = x - radius + 1; i < x + radius; i++) {
				for (j = y - radius + 1; j < y + radius; j++) {
					if (inside(i, j)) {
						dist = get_dist(i - x, j - y);
						if (radius > dist) {  // was >=
							cells[i][j].resource = resource;
							cells[i][j].object = 0;
							hasChanged = true;
						}
					}
				}
			}
		}

		void MapPreview::setResource(int x, int y, int resource) {
			cells[x][y].resource = resource;
			if (resource != 0) {
				cells[x][y].object = 0;
			}
			hasChanged = true;
		}

		void MapPreview::changeStartLocation(int x, int y, int faction) {
			if ((faction - 1) < maxFactions && inside(x, y)) {
				startLocations[faction].x = x;
				startLocations[faction].y = y;
				hasChanged = true;
			}
		}

		bool MapPreview::inside(int x, int y) {
			return (x >= 0 && x < w && y >= 0 && y < h);
		}

		void MapPreview::reset(int w, int h, float alt, MapSurfaceType surf) {
			if (w < MIN_MAP_CELL_DIMENSION || h < MIN_MAP_CELL_DIMENSION) {
				char szBuf[8096] = "";
				snprintf(szBuf, 8096, "Size of map must be at least %dx%d", MIN_MAP_CELL_DIMENSION, MIN_MAP_CELL_DIMENSION);
				throw megaglest_runtime_error(szBuf);
				//return;
			}

			if (w > MAX_MAP_CELL_DIMENSION || h > MAX_MAP_CELL_DIMENSION) {
				char szBuf[8096] = "";
				snprintf(szBuf, 8096, "Size of map can be at most %dx%d", MAX_MAP_CELL_DIMENSION, MAX_MAP_CELL_DIMENSION);
				throw megaglest_runtime_error(szBuf);
			}

			if (alt < MIN_MAP_CELL_HEIGHT || alt > MAX_MAP_CELL_HEIGHT) {
				char szBuf[8096] = "";
				snprintf(szBuf, 8096, "Height must be in the range %d-%d", MIN_MAP_CELL_HEIGHT, MAX_MAP_CELL_HEIGHT);
				throw megaglest_runtime_error(szBuf);
			}

			if (surf < st_Grass || surf > st_Ground) {
				char szBuf[8096] = "";
				snprintf(szBuf, 8096, "Surface must be in the range %d-%d", st_Grass, st_Ground);
				throw megaglest_runtime_error(szBuf);
			}

			//if (cells != NULL) {
			//	for (int i = 0; i < this->w; i++) {
			//		delete [] cells[i];
			//	}
			//	delete [] cells;
			//}
			cells.clear();

			this->w = w;
			this->h = h;
			//this->maxFactions = maxFactions;

			//cells = new Cell*[w];
			cells.resize(w);
			for (int i = 0; i < w; i++) {
				//cells[i] = new Cell[h];
				cells[i].resize(h);
				for (int j = 0; j < h; j++) {
					cells[i][j].height = alt;
					cells[i][j].object = 0;
					cells[i][j].resource = 0;
					cells[i][j].surface = surf;
				}
			}
			hasChanged = true;
		}

		void MapPreview::resize(int w, int h, float alt, MapSurfaceType surf) {
			if (w < MIN_MAP_CELL_DIMENSION || h < MIN_MAP_CELL_DIMENSION) {
				char szBuf[8096] = "";
				snprintf(szBuf, 8096, "Size of map must be at least %dx%d", MIN_MAP_CELL_DIMENSION, MIN_MAP_CELL_DIMENSION);
				throw megaglest_runtime_error(szBuf);
				//return;
			}

			if (w > MAX_MAP_CELL_DIMENSION || h > MAX_MAP_CELL_DIMENSION) {
				char szBuf[8096] = "";
				snprintf(szBuf, 8096, "Size of map can be at most %dx%d", MAX_MAP_CELL_DIMENSION, MAX_MAP_CELL_DIMENSION);
				throw megaglest_runtime_error(szBuf);
			}

			if (alt < MIN_MAP_CELL_HEIGHT || alt > MAX_MAP_CELL_HEIGHT) {
				char szBuf[8096] = "";
				snprintf(szBuf, 8096, "Height must be in the range %d-%d", MIN_MAP_CELL_HEIGHT, MAX_MAP_CELL_HEIGHT);
				throw megaglest_runtime_error(szBuf);
			}

			if (surf < st_Grass || surf > st_Ground) {
				char szBuf[8096] = "";
				snprintf(szBuf, 8096, "Surface must be in the range %d-%d", st_Grass, st_Ground);
				throw megaglest_runtime_error(szBuf);
			}

			int oldW = this->w;
			int oldH = this->h;
			this->w = w;
			this->h = h;
			//this->maxFactions = maxFactions;

			//create new cells
			//Cell **oldCells = cells;
			std::vector<std::vector<Cell> > oldCells = cells;

			//cells = new Cell*[w];
			cells.resize(w);
			for (int i = 0; i < w; i++) {
				//cells[i] = new Cell[h];
				cells[i].resize(h);
				for (int j = 0; j < h; j++) {
					cells[i][j].height = alt;
					cells[i][j].object = 0;
					cells[i][j].resource = 0;
					cells[i][j].surface = surf;
				}
			}

			int wOffset = w < oldW ? 0 : (w - oldW) / 2;
			int hOffset = h < oldH ? 0 : (h - oldH) / 2;
			//assign old values to cells
			for (int i = 0; i < oldW; i++) {
				for (int j = 0; j < oldH; j++) {
					if (i + wOffset < w && j + hOffset < h) {
						cells[i + wOffset][j + hOffset].height = oldCells[i][j].height;
						cells[i + wOffset][j + hOffset].object = oldCells[i][j].object;
						cells[i + wOffset][j + hOffset].resource = oldCells[i][j].resource;
						cells[i + wOffset][j + hOffset].surface = oldCells[i][j].surface;
					}
				}
			}
			for (int i = 0; i < maxFactions; ++i) {
				startLocations[i].x += wOffset;
				startLocations[i].y += hOffset;
			}

			//delete old cells
			//if (oldCells != NULL) {
			//	for (int i = 0; i < oldW; i++)
			//		delete [] oldCells[i];
			//	delete [] oldCells;
			//}

			hasChanged = true;
		}

		void MapPreview::resetFactions(int maxPlayers) {
			if (maxPlayers < MIN_MAP_FACTIONCOUNT || maxPlayers > MAX_MAP_FACTIONCOUNT) {
				char szBuf[8096] = "";
				snprintf(szBuf, 8096, "Max Players must be in the range %d-%d", MIN_MAP_FACTIONCOUNT, MAX_MAP_FACTIONCOUNT);
				throw megaglest_runtime_error(szBuf);
			}

			//if (startLocations != NULL) {
			//	delete [] startLocations;
			//	startLocations = NULL;
			//}
			startLocations.clear();

			maxFactions = maxPlayers;

			//startLocations = new StartLocation[maxFactions];
			startLocations.resize(maxFactions);
			for (int i = 0; i < maxFactions; i++) {
				startLocations[i].x = 0;
				startLocations[i].y = 0;
			}

			hasChanged = true;
		}

		void MapPreview::setTitle(const string &title) {
			this->title = title;
			hasChanged = true;
		}

		void MapPreview::setDesc(const string &desc) {
			this->desc = desc;
			hasChanged = true;
		}

		void MapPreview::setAuthor(const string &author) {
			this->author = author;
			hasChanged = true;
		}

		void MapPreview::setAdvanced(int heightFactor, int waterLevel, int cliffLevel, int cameraHeight) {
			this->heightFactor = heightFactor;
			this->waterLevel = waterLevel;
			this->cliffLevel = cliffLevel;
			this->cameraHeight = cameraHeight;
			hasChanged = true;
		}

		void MapPreview::randomizeHeights(bool withReset, int minimumHeight, int maximumHeight, int chanceDivider, int smoothRecursions) {
			if (withReset) resetHeights(random.randRange(8, 10));
			realRandomize(minimumHeight, maximumHeight, chanceDivider, smoothRecursions);
			hasChanged = true;
		}

		void MapPreview::randomizeFactions() {
			int slPlaceFactorX = random.randRange(0, 1);
			int slPlaceFactorY = random.randRange(0, 1) * 2;

			for (int i = 0; i < maxFactions; ++i) {
				StartLocation sl;
				float slNoiseFactor = random.randRange(0.5f, 0.8f);

				sl.x = static_cast<int>(w * slNoiseFactor * ((i + slPlaceFactorX) % 2) + w * (1.f - slNoiseFactor) / 2.f);
				sl.y = static_cast<int>(h * slNoiseFactor * (((i + slPlaceFactorY) / 2) % 2) + h * (1.f - slNoiseFactor) / 2.f);
				startLocations[i] = sl;
			}
			hasChanged = true;
		}

		void MapPreview::smoothSurface(bool limitHeight) {
			//float oldHeights [w*h];
			float *oldHeights = new float[w*h]; // dumb microsoft compiler cannot handle dynamic arrays, doing it manually
			for (int j = 0; j < h; ++j) {
				for (int i = 0; i < w; ++i) {
					oldHeights[w*j + i] = cells[i][j].height;
					//printf("w=%d h=%d i=%d j=%d w*h=%d  w*j+i=%d \n",w,h,i,j,w*h,w*j+i);
				}
			}

			for (int j = 1; j < h - 1; ++j) {
				for (int i = 1; i < w - 1; ++i) {
					float height = 0.f;
					float numUsedToSmooth = 0.f;
					for (int k = -1; k <= 1; ++k) {
						for (int l = -1; l <= 1; ++l) {
							int tmpHeight = (int) oldHeights[(j + k) * w + (i + l)];
							if (limitHeight && tmpHeight > 20) {
								tmpHeight = 20;
							}
							if (limitHeight && tmpHeight < 0) {
								tmpHeight = 0;
							}
							height += tmpHeight;
							numUsedToSmooth++;
						}
					}
					height /= numUsedToSmooth;
					cells[i][j].height = height;
				}
			}
			delete[] oldHeights;
		}

		void MapPreview::switchSurfaces(MapSurfaceType surf1, MapSurfaceType surf2) {
			if (surf1 >= st_Grass && surf1 <= st_Ground && surf2 >= st_Grass && surf2 <= st_Ground) {
				for (int i = 0; i < w; ++i) {
					for (int j = 0; j < h; ++j) {
						if (cells[i][j].surface == surf1) {
							cells[i][j].surface = surf2;
							hasChanged = true;
						} else if (cells[i][j].surface == surf2) {
							cells[i][j].surface = surf1;
							hasChanged = true;
						}
					}
				}
			} else {
				throw megaglest_runtime_error("Incorrect surfaces");
			}
		}

		void toEndianMapFileHeader(MapFileHeader &header) {
			header.version = Shared::PlatformByteOrder::toCommonEndian(header.version);
			header.maxFactions = Shared::PlatformByteOrder::toCommonEndian(header.maxFactions);
			header.width = Shared::PlatformByteOrder::toCommonEndian(header.width);
			header.height = Shared::PlatformByteOrder::toCommonEndian(header.height);
			header.heightFactor = Shared::PlatformByteOrder::toCommonEndian(header.heightFactor);
			header.waterLevel = Shared::PlatformByteOrder::toCommonEndian(header.waterLevel);
			for (unsigned int i = 0; i < (unsigned int) MAX_TITLE_LENGTH; ++i) {
				header.title[i] = Shared::PlatformByteOrder::toCommonEndian(header.title[i]);
			}
			for (unsigned int i = 0; i < (unsigned int) MAX_DESCRIPTION_LENGTH; ++i) {
				header.description[i] = Shared::PlatformByteOrder::toCommonEndian(header.description[i]);
			}
		}
		void fromEndianMapFileHeader(MapFileHeader &header) {
			header.version = Shared::PlatformByteOrder::fromCommonEndian(header.version);
			header.maxFactions = Shared::PlatformByteOrder::fromCommonEndian(header.maxFactions);
			header.width = Shared::PlatformByteOrder::fromCommonEndian(header.width);
			header.height = Shared::PlatformByteOrder::fromCommonEndian(header.height);
			header.heightFactor = Shared::PlatformByteOrder::fromCommonEndian(header.heightFactor);
			header.waterLevel = Shared::PlatformByteOrder::fromCommonEndian(header.waterLevel);
			for (unsigned int i = 0; i < (unsigned int) MAX_TITLE_LENGTH; ++i) {
				header.title[i] = Shared::PlatformByteOrder::fromCommonEndian(header.title[i]);
			}
			for (unsigned int i = 0; i < (unsigned int) MAX_DESCRIPTION_LENGTH; ++i) {
				header.description[i] = Shared::PlatformByteOrder::fromCommonEndian(header.description[i]);
			}
		}

		void MapPreview::loadFromFile(const string &path) {

			// "Could not open file, result: 3 - 2 No such file or directory [C:\Documents and Settings\人間五\Application Data\megaglest\maps\clearings_in_the_woods.gbm]

#ifdef WIN32
			wstring wstr = utf8_decode(path);
			FILE* f1 = _wfopen(wstr.c_str(), L"rb");
			int fileErrno = errno;
#else
			FILE *f1 = fopen(path.c_str(), "rb");
#endif
			if (f1 != NULL) {

				//read header
				MapFileHeader header;
				size_t bytes = fread(&header, sizeof(MapFileHeader), 1, f1);
				if (bytes != 1) {
					char szBuf[8096] = "";
					snprintf(szBuf, 8096, "fread returned wrong size = " MG_SIZE_T_SPECIFIER " on line: %d.", bytes, __LINE__);
					fclose(f1);
					throw megaglest_runtime_error(szBuf);
				}
				fromEndianMapFileHeader(header);

				heightFactor = header.heightFactor;
				waterLevel = header.waterLevel;
				title = header.title;
				author = header.author;
				cliffLevel = 0;
				if (header.version == 1) {
					desc = header.description;
				} else if (header.version == 2) {
					desc = header.version2.short_desc;
					cliffLevel = header.version2.cliffLevel;
					cameraHeight = header.version2.cameraHeight;
				}

				//read start locations
				resetFactions(header.maxFactions);
				for (int i = 0; i < maxFactions; ++i) {
					bytes = fread(&startLocations[i].x, sizeof(int32), 1, f1);
					if (bytes != 1) {
						char szBuf[8096] = "";
						snprintf(szBuf, 8096, "fread returned wrong size = " MG_SIZE_T_SPECIFIER " on line: %d.", bytes, __LINE__);
						throw megaglest_runtime_error(szBuf);
					}
					startLocations[i].x = Shared::PlatformByteOrder::fromCommonEndian(startLocations[i].x);

					bytes = fread(&startLocations[i].y, sizeof(int32), 1, f1);
					if (bytes != 1) {
						char szBuf[8096] = "";
						snprintf(szBuf, 8096, "fread returned wrong size = " MG_SIZE_T_SPECIFIER " on line: %d.", bytes, __LINE__);
						throw megaglest_runtime_error(szBuf);
					}
					startLocations[i].y = Shared::PlatformByteOrder::fromCommonEndian(startLocations[i].y);
				}

				//read Heights
				reset(header.width, header.height, (float) DEFAULT_MAP_CELL_HEIGHT, DEFAULT_MAP_CELL_SURFACE_TYPE);
				for (int j = 0; j < h; ++j) {
					for (int i = 0; i < w; ++i) {
						bytes = fread(&cells[i][j].height, sizeof(float), 1, f1);
						if (bytes != 1) {
							char szBuf[8096] = "";
							snprintf(szBuf, 8096, "fread returned wrong size = " MG_SIZE_T_SPECIFIER " on line: %d.", bytes, __LINE__);
							throw megaglest_runtime_error(szBuf);
						}
						cells[i][j].height = Shared::PlatformByteOrder::fromCommonEndian(cells[i][j].height);
					}
				}

				//read surfaces
				for (int j = 0; j < h; ++j) {
					for (int i = 0; i < w; ++i) {
						bytes = fread(&cells[i][j].surface, sizeof(int8), 1, f1);
						if (bytes != 1) {
							char szBuf[8096] = "";
							snprintf(szBuf, 8096, "fread returned wrong size = " MG_SIZE_T_SPECIFIER " on line: %d.", bytes, __LINE__);
							throw megaglest_runtime_error(szBuf);
						}
						cells[i][j].surface = Shared::PlatformByteOrder::fromCommonEndian(cells[i][j].surface);
					}
				}

				//read objects
				for (int j = 0; j < h; ++j) {
					for (int i = 0; i < w; ++i) {
						int8 obj;
						bytes = fread(&obj, sizeof(int8), 1, f1);
						if (bytes != 1) {
							char szBuf[8096] = "";
							snprintf(szBuf, 8096, "fread returned wrong size = " MG_SIZE_T_SPECIFIER " on line: %d.", bytes, __LINE__);
							throw megaglest_runtime_error(szBuf);
						}
						obj = Shared::PlatformByteOrder::fromCommonEndian(obj);

						if (obj <= 10) {
							cells[i][j].object = obj;
						} else {
							cells[i][j].resource = obj - 10;
						}
					}
				}

				if (f1) fclose(f1);

				fileLoaded = true;
				mapFileLoaded = path;
				hasChanged = false;
			} else {
#ifdef WIN32
				DWORD error = GetLastError();
				string strError = "[#5] Could not open file, result: " + intToStr(error) + " - " + intToStr(fileErrno) + " " + strerror(fileErrno) + " [" + path + "]";
				throw strError;
#else
				throw megaglest_runtime_error("[#5] error opening map file: " + path);
#endif
			}
		}


		void MapPreview::saveToFile(const string &path) {
#ifdef WIN32
			FILE* f1 = _wfopen(utf8_decode(path).c_str(), L"wb");
#else
			FILE *f1 = fopen(path.c_str(), "wb");
#endif
			if (f1 != NULL) {

				//write header
				MapFileHeader header;
				memset(&header, 0, sizeof(header));

				header.version = MAP_FORMAT_VERSION;
				header.maxFactions = maxFactions;
				header.width = w;
				header.height = h;
				header.heightFactor = heightFactor;
				header.waterLevel = waterLevel;
				strncpy(header.title, title.c_str(), MAX_TITLE_LENGTH);
				header.title[MAX_TITLE_LENGTH - 1] = 0;
				strncpy(header.author, author.c_str(), MAX_AUTHOR_LENGTH);
				header.author[MAX_AUTHOR_LENGTH - 1] = 0;
				strncpy(header.version2.short_desc, desc.c_str(), MAX_DESCRIPTION_LENGTH_VERSION2);
				header.version2.short_desc[MAX_DESCRIPTION_LENGTH_VERSION2 - 1] = 0;
				header.version2.magic = 0x01020304;
				header.version2.cliffLevel = cliffLevel;
				header.version2.cameraHeight = cameraHeight;


				fwrite(&header, sizeof(MapFileHeader), 1, f1);

				//write start locations
				for (int i = 0; i < maxFactions; ++i) {
					fwrite(&startLocations[i].x, sizeof(int32), 1, f1);
					fwrite(&startLocations[i].y, sizeof(int32), 1, f1);
				}

				//write Heights
				for (int j = 0; j < h; ++j) {
					for (int i = 0; i < w; ++i) {
						fwrite(&cells[i][j].height, sizeof(float32), 1, f1);
					}
				}

				//write surfaces
				for (int j = 0; j < h; ++j) {
					for (int i = 0; i < w; ++i) {
						fwrite(&cells[i][j].surface, sizeof(int8), 1, f1);
					}
				}

				//write objects
				for (int j = 0; j < h; ++j) {
					for (int i = 0; i < w; ++i) {
						if (cells[i][j].resource == 0)
							fwrite(&cells[i][j].object, sizeof(int8), 1, f1);
						else {
							int8 res = cells[i][j].resource + 10;
							fwrite(&res, sizeof(int8), 1, f1);
						}
					}
				}

				if (f1) fclose(f1);

				hasChanged = false;
			} else {
				throw megaglest_runtime_error("Error opening map file: " + path);
			}

			//void randomHeight(int x, int y, int height);
		}

		// ==================== PRIVATE ====================

		void MapPreview::resetHeights(int height) {
			for (int i = 0; i < w; ++i) {
				for (int j = 0; j < h; ++j) {
					cells[i][j].height = static_cast<float>(height);
					hasChanged = true;
				}
			}
		}

		void MapPreview::realRandomize(int minimumHeight, int maximumHeight, int _chanceDivider, int _smoothRecursions) {
			int moduloParam = abs(maximumHeight - minimumHeight);
			int chanceDivider = _chanceDivider;
			int smoothRecursions = _smoothRecursions;
			if (moduloParam < 2) moduloParam = 2;
			//printf("moduloParam=%d   minimumHeight=%d  maximumHeight=%d\n",moduloParam,minimumHeight,maximumHeight);

			// set chanceDivider to something possible
			if (chanceDivider < 2) chanceDivider = 2;

			// set smoothRecursions to something useful
			if (smoothRecursions < 0) smoothRecursions = 0;
			if (smoothRecursions > 1000) smoothRecursions = 1000;

			for (int i = 1; i < w - 1; ++i) {
				for (int j = 1; j < h - 1; ++j) {
					if (rand() % chanceDivider == 1) {
						cells[i][j].height = (rand() % moduloParam) + minimumHeight;
					}
				}
			}
			for (int i = 0; i < smoothRecursions; ++i) {
				if (i + 1 == smoothRecursions)
					smoothSurface(true);
				else
					smoothSurface(false);
			}
		}

		void MapPreview::applyNewHeight(float newHeight, int x, int y, int strength) {
			cells[x][y].height = static_cast<float>(((cells[x][y].height * strength) + newHeight) / (strength + 1));
			hasChanged = true;
		}

		bool MapPreview::loadMapInfo(string file, MapInfo *mapInfo, string i18nMaxMapPlayersTitle, string i18nMapSizeTitle, bool errorOnInvalidMap) {
			bool validMap = false;
			FILE *f = NULL;
			try {
#ifdef WIN32
				f = _wfopen(utf8_decode(file).c_str(), L"rb");
#else
				f = fopen(file.c_str(), "rb");
#endif
				if (f == NULL) {
					throw megaglest_runtime_error("Can't open file");
				}

				MapFileHeader header;
				size_t readBytes = fread(&header, sizeof(MapFileHeader), 1, f);
				if (readBytes != 1) {
					validMap = false;

					if (errorOnInvalidMap == true) {
						char szBuf[8096] = "";
						snprintf(szBuf, 8096, "In [%s::%s Line: %d]\nfile [%s]\nreadBytes != sizeof(MapFileHeader) [" MG_SIZE_T_SPECIFIER "] [" MG_SIZE_T_SPECIFIER "]\n", extractFileFromDirectoryPath(__FILE__).c_str(), __FUNCTION__, __LINE__, file.c_str(), readBytes, sizeof(MapFileHeader));
						SystemFlags::OutputDebug(SystemFlags::debugError, "%s", szBuf);

						throw megaglest_runtime_error(szBuf);
					}
				} else {
					fromEndianMapFileHeader(header);

					if (header.version < mapver_1 || header.version >= mapver_MAX) {
						validMap = false;

						if (errorOnInvalidMap == true) {
							char szBuf[4096] = "";
							printf("In [%s::%s Line: %d]\file [%s]\nheader.version < mapver_1 || header.version >= mapver_MAX [%d] [%d]\n", extractFileFromDirectoryPath(__FILE__).c_str(), __FUNCTION__, __LINE__, file.c_str(), header.version, mapver_MAX);
							SystemFlags::OutputDebug(SystemFlags::debugError, "%s", szBuf);

							throw megaglest_runtime_error(szBuf);
						}
					} else if (header.maxFactions <= 0 || header.maxFactions > MAX_MAP_FACTIONCOUNT) {
						validMap = false;

						if (errorOnInvalidMap == true) {
							char szBuf[4096] = "";
							printf("In [%s::%s Line: %d]\file [%s]\nheader.maxFactions <= 0 || header.maxFactions > MAX_MAP_FACTIONCOUNT [%d] [%d]\n", extractFileFromDirectoryPath(__FILE__).c_str(), __FUNCTION__, __LINE__, file.c_str(), header.maxFactions, MAX_MAP_FACTIONCOUNT);
							SystemFlags::OutputDebug(SystemFlags::debugError, "%s", szBuf);

							throw megaglest_runtime_error(szBuf);
						}
					} else {
						mapInfo->size.x = header.width;
						mapInfo->size.y = header.height;
						mapInfo->players = header.maxFactions;

						// hardMaxPlayers is used in menu_state_custom_game, as part of
						// enhanced observer mode (issue #13)
						mapInfo->hardMaxPlayers = mapInfo->players;

						mapInfo->desc = i18nMaxMapPlayersTitle + ": " + intToStr(mapInfo->players) + "\n";
						mapInfo->desc += i18nMapSizeTitle + ": " + intToStr(mapInfo->size.x) + " x " + intToStr(mapInfo->size.y);

						validMap = true;
					}
				}

				fclose(f);
			} catch (exception &e) {
				if (f) fclose(f);

				//assert(0);
				SystemFlags::OutputDebug(SystemFlags::debugError, "In [%s::%s Line: %d] Error [%s] loading map [%s]\n", __FILE__, __FUNCTION__, __LINE__, e.what(), file.c_str());
				throw megaglest_runtime_error("Error loading map file: [" + file + "] msg: " + e.what() + " errno [" + intToStr(errno) + "] [" + strerror(errno) + "]");
			}

			return validMap;
		}

		// static
		string MapPreview::getMapPath(const vector<string> &pathList, const string &mapName,
			string scenarioDir, bool errorOnNotFound) {
			for (unsigned int idx = 0; idx < pathList.size(); idx++) {
				string map_path = pathList[idx];
				endPathWithSlash(map_path);

				const string original = map_path + mapName;
				const string zeta = map_path + mapName + ".zgm";
				const string mega = map_path + mapName + ".mgm";
				const string glest = map_path + mapName + ".gbm";

				if ((	EndsWith(original, ".zgm") == true ||
							EndsWith(original, ".mgm") == true ||
							EndsWith(original, ".gbm") == true) &&
							fileExists(original)) {
					return original;
				} else if (fileExists(zeta)) {
					return zeta;
				} else if (fileExists(mega)) {
					return mega;
				} else if (fileExists(glest)) {
					return glest;
				}
			}

			if (errorOnNotFound == true) {
				throw megaglest_runtime_error("Map [" + mapName + "] not found, scenarioDir [" + scenarioDir + "]");
			}

			return "";
		}

		vector<string> MapPreview::findAllValidMaps(const vector<string> &pathList,
			string scenarioDir, bool getUserDataOnly, bool cutExtension,
			vector<string> *invalidMapList) {
			vector<string> results;

			if (getUserDataOnly == true) {
				if (pathList.size() > 1) {
					string path = pathList[1];
					endPathWithSlash(path);

					vector<string> results2;
					set<string> allMaps2;
					findAll(path + "*.gbm", results2, cutExtension, false);
					copy(results2.begin(), results2.end(), std::inserter(allMaps2, allMaps2.begin()));

					results2.clear();
					findAll(path + "*.zgm", results2, cutExtension, false);
					copy(results2.begin(), results2.end(), std::inserter(allMaps2, allMaps2.begin()));

					results2.clear();
					findAll(path + "*.mgm", results2, cutExtension, false);
					copy(results2.begin(), results2.end(), std::inserter(allMaps2, allMaps2.begin()));

					results2.clear();
					copy(allMaps2.begin(), allMaps2.end(), std::back_inserter(results2));
					results = results2;
					//printf("\n\nMap path [%s] mapFilesUserData.size() = %d\n\n\n",path.c_str(),mapFilesUserData.size());
				}
			} else {
				set<string> allMaps;
				findAll(pathList, "*.gbm", results, cutExtension, false);
				copy(results.begin(), results.end(), std::inserter(allMaps, allMaps.begin()));

				results.clear();
				findAll(pathList, "*.zgm", results, cutExtension, false);
				copy(results.begin(), results.end(), std::inserter(allMaps, allMaps.begin()));

				results.clear();
				findAll(pathList, "*.mgm", results, cutExtension, false);
				copy(results.begin(), results.end(), std::inserter(allMaps, allMaps.begin()));
				results.clear();

				copy(allMaps.begin(), allMaps.end(), std::back_inserter(results));
			}

			vector<string> mapFiles = results;
			results.clear();

			MapInfo mapInfo;
			for (unsigned int i = 0; i < mapFiles.size(); i++) {// fetch info and put map in right list
				//loadMapInfo(string file, MapInfo *mapInfo, string i18nMaxMapPlayersTitle,string i18nMapSizeTitle,bool errorOnInvalidMap=true);
				//printf("getMapPath [%s]\nmapFiles.at(i) [%s]\nscenarioDir [%s] getUserDataOnly = %d cutExtension = %d\n",getMapPath(pathList,mapFiles.at(i), scenarioDir, false).c_str(),mapFiles.at(i).c_str(),scenarioDir.c_str(), getUserDataOnly, cutExtension);

				if (loadMapInfo(getMapPath(pathList, mapFiles.at(i), scenarioDir, false), &mapInfo, "", "", false) == true) {
					results.push_back(mapFiles.at(i));
				} else {
					if (invalidMapList != NULL) {
						invalidMapList->push_back(getMapPath(pathList, mapFiles.at(i), scenarioDir, false));
					}
				}
			}

			return results;
		}

	}
}// end namespace
