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

#ifndef _GLEST_GAME_RESOURCETYPE_H_
#   define _GLEST_GAME_RESOURCETYPE_H_

#   ifdef WIN32
#      include <winsock2.h>
#      include <winsock.h>
#   endif

#   include "element_type.h"
#   include "model.h"
#   include "checksum.h"
#   include "leak_dumper.h"
#   include "unit_particle_type.h"
#   include "object_type.h"

namespace Glest {
	namespace Game {

		using Shared::Graphics::Model;
		using Shared::Util::Checksum;

		enum ResourceClass {
			rcTech,
			rcTileset,
			rcStatic,
			rcConsumable
		};

		// =====================================================
		//      class ResourceType
		//
		///     A type of resource that can be harvested or not
		// =====================================================


		class ResourceType :public DisplayableType {
		private:
			ResourceClass resourceClass;
			int tilesetObject;        //used only if class==rcTileset
			int resourceNumber;       //used only if class==rcTech, resource number in the map
			int interval;             //used only if class==rcConsumable
			int defResPerPatch;       //used only if class==rcTileset || class==rcTech
			bool recoup_cost;
			bool displayInHud;

			Model *model;
			ObjectParticleSystemTypes particleTypes;
			bool cleanupMemory;

		public:
			ResourceType();
			~ResourceType();
			void load(const string & dir, Checksum * checksum,
				Checksum * techtreeChecksum, std::map < string,
				vector < pair < string, string > > >&loadedFileList,
				string techtreePath);

			virtual string getName(bool translatedValue = false) const;
			//get
			int getClass() const {
				return resourceClass;
			}
			int getTilesetObject() const {
				return tilesetObject;
			}
			int getResourceNumber() const {
				return resourceNumber;
			}
			int getInterval() const {
				return interval;
			}
			int getDefResPerPatch() const {
				return defResPerPatch;
			}
			Model *getModel() const {
				return model;
			}
			bool getRecoup_cost() const {
				return recoup_cost;
			}
			bool getDisplayInHud() const {
				return displayInHud;
			}

			bool hasParticles() const {
				return !particleTypes.empty();
			}
			const ObjectParticleSystemTypes *getObjectParticleSystemTypes() const {
				return &particleTypes;
			}

			void setCleanupMemory(bool value) {
				cleanupMemory = value;
			}

			static ResourceClass strToRc(const string & s);
			void deletePixels();

			void saveGame(XmlNode * rootNode);
		};

	}
}                              //end namespace

#endif
