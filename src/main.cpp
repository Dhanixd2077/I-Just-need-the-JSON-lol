#include <Geode/Geode.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

class $modify(MyPlayerObject, PlayerObject) {
    void playerDestroyed(bool p0) {
        PlayerObject::playerDestroyed(p0);

        // Solo ejecutar si estamos en un nivel
        if (auto playLayer = PlayLayer::get()) {
            
            // Genera un ID de nivel aleatorio entre 1 y 22
            int randomLevelID = (std::rand() % 22) + 1;
            
            // Usar .get() en lugar de sharedState() para adaptarlo a Geode
            auto level = GameLevelManager::get()->getMainLevel(randomLevelID, false);

            if (level) {
                auto scene = PlayLayer::scene(level, false, false);
                // Usar CCDirector::get() en lugar de sharedDirector()
                CCDirector::get()->replaceScene(CCTransitionFade::create(0.5f, scene));
            }
        }
    }
};
