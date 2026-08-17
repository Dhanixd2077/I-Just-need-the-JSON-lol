#include <Geode/Geode.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <cmath>

using namespace geode::prelude;

// ==========================================
// VARIABLES GLOBALES PARA CONTROL DE AUDIO
// ==========================================
FMOD::Channel* g_canalVoz = nullptr;
FMOD::Channel* g_canalInstrumentos = nullptr;
bool g_enDashOrb = false;
float g_anguloDash = 0.0f;

// ==========================================
// HOOK: CONFIGURACIÓN DE FMOD (DOLBY ATMOS)
// ==========================================
class $modify(MyFMODAudioEngine, FMODAudioEngine) {
    bool init() {
        if (!FMODAudioEngine::init()) return false;

        // Recuperar el sistema de FMOD nativo desde el motor de Geode
        FMOD::System* system = m_system;
        if (system) {
            // Forzar salida de audio a 7.1 para Dolby Atmos
            system->setSoftwareFormat(48000, FMOD_SPEAKERMODE_7POINT1, 0);

            // Inicializar las coordenadas del Oyente (Listener) en el centro de la cabeza
            FMOD_VECTOR posOyente = { 0.0f, 0.0f, 0.0f };
            FMOD_VECTOR velOyente = { 0.0f, 0.0f, 0.0f };
            FMOD_VECTOR frenteOyente = { 0.0f, 0.0f, 1.0f }; 
            FMOD_VECTOR arribaOyente = { 0.0f, 1.0f, 0.0f }; 
            system->set3DListenerAttributes(0, &posOyente, &velOyente, &frenteOyente, &arribaOyente);
        }
        return true;
    }

    // Usamos std::string para compatibilidad total y evitar errores de compilación
    void playMusic(std::string path, bool loop, float fadeIn, int location) {
        // Ejecutamos la carga base original del juego
        FMODAudioEngine::playMusic(path, loop, fadeIn, location);

        FMOD::System* system = m_system;
        // En Geode moderno accedemos al sonido usando el puntero m_currentSound de la clase base
        if (!system || !m_currentSound) return;

        FMOD::Sound* sonidoActual = m_currentSound;
        FMOD::ChannelGroup* masterGroup = m_masterChannelGroup;

        // Detenemos el canal original por defecto del juego para evitar eco plano
        if (m_currentChannel) {
            m_currentChannel->stop();
        }

        // REPRODUCCIÓN CANAL 1: Voz (Fija en el Centro)
        system->playSound(sonidoActual, masterGroup, false, &g_canalVoz);
        if (g_canalVoz) {
            FMOD::DSP* dspVoz;
            system->createDSPByType(FMOD_DSP_TYPE_PARAMEQ, &dspVoz);
            dspVoz->setParameterFloat(FMOD_DSP_PARAMEQ_CENTER, 1500.0f); 
            dspVoz->setParameterFloat(FMOD_DSP_PARAMEQ_BANDWIDTH, 2.0f);
            g_canalVoz->addDSP(0, dspVoz);

            g_canalVoz->setMode(FMOD_2D);
            g_canalVoz->setPan(0.0f); 
        }

        // REPRODUCCIÓN CANAL 2: Instrumentos (Vuelo Atmosférico 3D)
        system->playSound(sonidoActual, masterGroup, false, &g_canalInstrumentos);
        if (g_canalInstrumentos) {
            FMOD::DSP* dspInstrumentos;
            system->createDSPByType(FMOD_DSP_TYPE_PARAMEQ, &dspInstrumentos);
            dspInstrumentos->setParameterFloat(FMOD_DSP_PARAMEQ_CENTER, 1500.0f);
            dspInstrumentos->setParameterFloat(FMOD_DSP_PARAMEQ_GAIN, -15.0f); 
            g_canalInstrumentos->addDSP(0, dspInstrumentos);

            g_canalInstrumentos->setMode(FMOD_3D | FMOD_3D_INVERSEROLLOFF);
            g_canalInstrumentos->set3DMinMaxDistance(1.0f, 50.0f);
        }
    }
};

// ==========================================
// HOOK: GAMEPLAY (REACCIÓN AL CUBO Y ORBES)
// ==========================================
class $modify(MyPlayLayer, PlayLayer) {
    
    void playerActivatedObject(PlayerObject* player, PlayerButtonCommand command) {
        PlayLayer::playerActivatedObject(player, command);
        
        if (player && player->m_isDashing && !g_enDashOrb) {
            g_enDashOrb = true;
            g_anguloDash = 0.0f; 
        }
    }

    void update(float dt) {
        PlayLayer::update(dt);

        if (!g_canalInstrumentos || !m_player1) return;

        FMOD_VECTOR posInstrumentos = { 0.0f, 0.0f, 0.0f };
        FMOD_VECTOR velocidad = { 0.0f, 0.0f, 0.0f };

        if (g_enDashOrb) {
            g_anguloDash += dt * 10.0f; 

            posInstrumentos.x = std::sin(g_anguloDash) * 12.0f; 
            posInstrumentos.y = 2.0f;                           
            posInstrumentos.z = std::cos(g_anguloDash) * 12.0f; 

            if (g_anguloDash >= 6.28f) { 
                g_enDashOrb = false;
            }
        } 
        else {
            float cuboY = m_player1->getPositionY();
            bool vaEnReversa = m_bIsGoingBackward; 

            float alturaEscalada = (cuboY - 105.0f) / 35.0f; 
            
            posInstrumentos.x = 0.0f;            
            posInstrumentos.y = alturaEscalada;  

            if (vaEnReversa) {
                posInstrumentos.z = -7.0f; 
            } else {
                posInstrumentos.z = 7.0f;  
            }
        }

        g_canalInstrumentos->set3DAttributes(&posInstrumentos, &velocidad);
    }
};
