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
            // Forzar salida de audio a 7.1 para que el mezclador de Dolby Atmos / Windows Sonic
            // en el sistema operativo intercepte los canales de altura y profundidad.
            system->setSoftwareFormat(48000, FMOD_SPEAKERMODE_7POINT1, 0);

            // Inicializar las coordenadas del Oyente (Listener) en el centro de la cabeza
            FMOD_VECTOR posOyente = { 0.0f, 0.0f, 0.0f };
            FMOD_VECTOR velOyente = { 0.0f, 0.0f, 0.0f };
            FMOD_VECTOR frenteOyente = { 0.0f, 0.0f, 1.0f }; // Mirando adelante (eje Z)
            FMOD_VECTOR arribaOyente = { 0.0f, 1.0f, 0.0f }; // El techo (eje Y)
            system->set3DListenerAttributes(0, &posOyente, &velOyente, &frenteOyente, &arribaOyente);
        }
        return true;
    }

    // Interceptamos la reproducción musical usando la estructura de strings compatible (gd::string)
    void playMusic(gd::string path, bool loop, float fadeIn, int location) {
        // Ejecutamos la carga base original del juego
        FMODAudioEngine::playMusic(path, loop, fadeIn, location);

        FMOD::System* system = m_system;
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
            // Crear filtro Paso Banda (Pone énfasis en las frecuencias medias de voz humana)
            FMOD::DSP* dspVoz;
            system->createDSPByType(FMOD_DSP_TYPE_PARAMEQ, &dspVoz);
            dspVoz->setParameterFloat(FMOD_DSP_PARAMEQ_CENTER, 1500.0f); // Rango de la voz
            dspVoz->setParameterFloat(FMOD_DSP_PARAMEQ_BANDWIDTH, 2.0f);
            g_canalVoz->addDSP(0, dspVoz);

            // Bloquear en modo 2D plano justo en el centro matemático
            g_canalVoz->setMode(FMOD_2D);
            g_canalVoz->setPan(0.0f); 
        }

        // REPRODUCCIÓN CANAL 2: Instrumentos (Vuelo Atmosférico 3D)
        system->playSound(sonidoActual, masterGroup, false, &g_canalInstrumentos);
        if (g_canalInstrumentos) {
            // Crear filtro Muesca para debilitar la voz en este canal y aislar instrumentos
            FMOD::DSP* dspInstrumentos;
            system->createDSPByType(FMOD_DSP_TYPE_PARAMEQ, &dspInstrumentos);
            dspInstrumentos->setParameterFloat(FMOD_DSP_PARAMEQ_CENTER, 1500.0f);
            dspInstrumentos->setParameterFloat(FMOD_DSP_PARAMEQ_GAIN, -15.0f); // Atenuar frecuencias de voz
            g_canalInstrumentos->addDSP(0, dspInstrumentos);

            // Activar el procesamiento de posicionamiento 3D para este canal
            g_canalInstrumentos->setMode(FMOD_3D | FMOD_3D_INVERSEROLLOFF);
            g_canalInstrumentos->set3DMinMaxDistance(1.0f, 50.0f);
        }
    }
};

// ==========================================
// HOOK: GAMEPLAY (REACCIÓN AL CUBO Y ORBES)
// ==========================================
class $modify(MyPlayLayer, PlayLayer) {
    
    // Capturar cuando el jugador interactúa con objetos (Orbes)
    void playerActivatedObject(PlayerObject* player, PlayerButtonCommand command) {
        PlayLayer::playerActivatedObject(player, command);
        
        // m_isDashing determina si el jugador está propulsado por una Dash Orb en la 2.2
        if (player && player->m_isDashing && !g_enDashOrb) {
            g_enDashOrb = true;
            g_anguloDash = 0.0f; // Inicializar ángulo de la rotación espacial
        }
    }

    // Bucle principal frame a frame durante el nivel
    void update(float dt) {
        PlayLayer::update(dt);

        if (!g_canalInstrumentos || !m_player1) return;

        FMOD_VECTOR posInstrumentos = { 0.0f, 0.0f, 0.0f };
        FMOD_VECTOR velocidad = { 0.0f, 0.0f, 0.0f };

        // 1. COMPORTAMIENTO EFECTO DASH ORB (GIRO CINEMÁTICO 360)
        if (g_enDashOrb) {
            g_anguloDash += dt * 10.0f; // Velocidad del giro alrededor de la cabeza

            posInstrumentos.x = std::sin(g_anguloDash) * 12.0f; // Lados
            posInstrumentos.y = 2.0f;                           // Elevación
            posInstrumentos.z = std::cos(g_anguloDash) * 12.0f; // Profundidad (Adelante/Atrás)

            if (g_anguloDash >= 6.28f) { // Una vuelta completa en radianes (~2*PI)
                g_enDashOrb = false;
            }
        } 
        // 2. COMPORTAMIENTO DINÁMICO NORMAL (SEGUIMIENTO AL CUBO)
        else {
            float cuboY = m_player1->getPositionY();
            bool vaEnReversa = m_bIsGoingBackward; // Detecta portales espejo y mecánicas de reversa nativas

            // Reducimos la escala de Y para mapearla fluidamente al espacio envolvente de FMOD
            float alturaEscalada = (cuboY - 105.0f) / 35.0f; 
            
            posInstrumentos.x = 0.0f;            
            posInstrumentos.y = alturaEscalada;  // Eje vertical sigue al icono

            // Invertir profundidad según la dirección del Gameplay (Normal vs Reversa)
            if (vaEnReversa) {
                posInstrumentos.z = -7.0f; // Mandar música atrás si va a la izquierda
            } else {
                posInstrumentos.z = 7.0f;  // Colocar música al frente si va a la derecha
            }
        }

        // Aplicar coordenadas calculadas al canal de instrumentos en el mezclador del SO
        g_canalInstrumentos->set3DAttributes(&posInstrumentos, &velocidad);
    }
};
