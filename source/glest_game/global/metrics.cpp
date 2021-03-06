// ==============================================================
//      This file is part of Glest (www.glest.org)
//
//      Copyright (C) 2001-2008 Martiño Figueroa
//
//      You can redistribute this code and/or modify it under 
//      the terms of the GNU General Public License as published 
//      by the Free Software Foundation; either version 2 of the 
//      License, or (at your option) any later version
// ==============================================================

#include "metrics.h"
#include <stdexcept>
#include "platform_util.h"
#include "leak_dumper.h"

using namespace std;

namespace Glest {
	namespace Game {


		// =====================================================
		//      class Metrics
		// =====================================================

		Metrics::Metrics() {
			reloadData();
		}

		void Metrics::reload(int resWidth, int resHeight) {
			Metrics *metrics = getInstancePtr();
			metrics->reloadData(resWidth, resHeight);
		}

		void Metrics::reloadData(int resWidth, int resHeight) {
			Config & config = Config::getInstance();

			virtualW = 1000;
			virtualH = 750;

			if (resWidth > 0) {
				screenW = resWidth;
			} else {
				screenW = config.getInt("ScreenWidth");
			}
			if (resHeight > 0) {
				screenH = resHeight;
			} else {
				screenH = config.getInt("ScreenHeight");
			}

			minimapX = 10;
			minimapY = 750 - 128 - 30 + 16;
			minimapW = 128;
			minimapH = 128;

			displayX = 800;
			displayY = 250;
			displayW = 160;
			displayH = 480;
		}

		Metrics *Metrics::getInstancePtr() {
			static Metrics metrics;
			return &metrics;
		}

		const Metrics & Metrics::getInstance() {
			Metrics *metrics = getInstancePtr();
			return *metrics;
		}

		float Metrics::getAspectRatio() const {
			if (screenH == 0) {
				throw megaglest_runtime_error("div by 0 screenH == 0");
			}
			return static_cast <float>(screenW) / screenH;
		}

		int Metrics::toVirtualX(int w) const {
			if (screenW == 0) {
				throw megaglest_runtime_error("div by 0 screenW == 0");
			}
			return w * virtualW / screenW;
		}

		int Metrics::toVirtualY(int h) const {
			if (screenH == 0) {
				throw megaglest_runtime_error("div by 0 screenH == 0");
			}

			//printf("h [%d] virtualH [%d] screenH [%d] result = %d\n",h,virtualH,screenH,(h*virtualH/screenH));

			return h * virtualH / screenH;
		}

		bool Metrics::isInDisplay(int x, int y) const {
			return
				x > displayX &&
				y > displayY && x < displayX + displayW && y < displayY + displayH;
		}

		bool Metrics::isInMinimap(int x, int y) const {
			return
				x > minimapX &&
				y > minimapY && x < minimapX + minimapW && y < minimapY + minimapH;
		}

	}
}                               // end namespace
