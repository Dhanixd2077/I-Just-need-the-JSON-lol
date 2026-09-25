#include <Geode/Geode.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

// im sick sorry for dat

class $modify(MyPlayerObject, PlayerObject) {
    void playerDestroyed(bool p0) {
        // player death
        PlayerObject::playerDestroyed(p0);

        // Only on level
        if (auto playLayer = PlayLayer::get()) {
            
            // this just generates a random level id between 1 and 22
            int randomLevelID = (std::rand() % 22) + 1;
            
            // get lvl by id
            auto level = GameLevelManager::sharedState()->getMainLevel(randomLevelID, false);

            if (level) {
                // this changes the level to the new random level w fade lol 
                auto scene = PlayLayer::scene(level, false, false);
                CCDirector::sharedDirector()->replaceScene(CCTransitionFade::create(0.5f, scene));
            }
        }
    }
