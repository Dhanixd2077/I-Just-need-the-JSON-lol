#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <fmod.hpp>

using namespace geode::prelude;

// --- Configuración Global ---
static float g_zoomMax = 0.15f;
static float g_bassIntensity = 1.2f;

// --- Shaders (Código Raw) ---
const char* shockwaveVert = R"(
    attribute vec4 a_position;
    attribute vec2 a_texCoord;
    varying vec2 v_texCoord;
    void main() {
        gl_Position = CC_PMatrix * a_position;
        v_texCoord = a_texCoord;
    }
)";

const char* shockwaveFrag = R"(
    varying vec2 v_texCoord;
    uniform sampler2D CC_Texture0;
    uniform vec2 u_center;
    uniform float u_time;
    void main() {
        float dist = distance(v_texCoord, u_center);
        if (dist <= u_time + 0.1 && dist >= u_time - 0.1) {
            float diff = dist - u_time;
            float powDiff = 1.0 - pow(abs(diff * 10.0), 0.8);
            float diffTime = diff * powDiff;
            vec2 uv = v_texCoord + (normalize(v_texCoord - u_center) * diffTime * 0.15);
            gl_FragColor = texture2D(CC_Texture0, uv);
        } else {
            gl_FragColor = texture2D(CC_Texture0, v_texCoord);
        }
    }
)";

// --- Clase del Shader ---
class ShockwaveNode : public CCNode {
public:
    CCGLProgram* m_program = nullptr;
    float m_time = 2.0f;

    static ShockwaveNode* create() {
        auto ret = new ShockwaveNode();
        if (ret && ret->init()) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }

    bool init() {
        m_program = new CCGLProgram();
        m_program->initWithByteArrays(shockwaveVert, shockwaveFrag);
        m_program->addAttribute(kCCAttributeNamePosition, kCCVertexAttrib_Position);
        m_program->addAttribute(kCCAttributeNameTexCoord, kCCVertexAttrib_TexCoords);
        m_program->link();
        m_program->updateUniforms();
        this->scheduleUpdate();
        return true;
    }

    void update(float dt) override {
        if (m_time < 1.0f) m_time += dt * 1.8f;
    }

    void trigger() { m_time = 0.0f; }
};

// --- Hook de PlayLayer para Geode v5 ---
class $modify(MyPlayLayer, PlayLayer) {
    // Estructura de campos obligatoria en Geode v4/v5
    struct Fields {
        ShockwaveNode* m_shaderNode = nullptr;
        FMOD::DSP* m_fftDSP = nullptr;
        float m_smoothBass = 0.0f;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

        // Configurar FMOD
        auto engine = FMODAudioEngine::sharedEngine();
        FMOD::System* sys = engine->m_system;
        FMOD::ChannelGroup* master;
        sys->getMasterChannelGroup(&master);
        
        sys->createDSPByType(FMOD_DSP_TYPE_FFT, &m_fields->m_fftDSP);
        m_fields->m_fftDSP->setParameterInt(FMOD_DSP_FFT_WINDOWSIZE, 1024);
        master->addDSP(0, m_fields->m_fftDSP);

        // Crear el nodo del shader
        m_fields->m_shaderNode = ShockwaveNode::create();
        this->addChild(m_fields->m_shaderNode);

        return true;
    }

    void update(float dt) override {
        PlayLayer::update(dt);

        if (!m_fields->m_fftDSP || !m_background) return;

        // Obtener Bass
        FMOD_DSP_PARAMETER_FFT* fft = nullptr;
        m_fields->m_fftDSP->getParameterData(FMOD_DSP_FFT_SPECTRUMDATA, (void**)&fft, nullptr, nullptr, 0);

        if (fft && fft->numchannels > 0) {
            float bass = 0.0f;
            for (int i = 1; i < 12; i++) bass += fft->spectrum[0][i];
            bass = (bass / 11.0f) * g_bassIntensity;
            m_fields->m_smoothBass += (bass - m_fields->m_smoothBass) * 0.2f;
        }

        // Efecto de pulso en fondo
        float currentBass = m_fields->m_smoothBass;
        m_background->setScale(1.0f + (currentBass * g_zoomMax));

        // Disparar shockwave si hay un golpe fuerte
        if (currentBass > 0.5f && m_fields->m_shaderNode->m_time > 0.7f) {
            m_fields->m_shaderNode->trigger();
        }

        // Aplicar Shader al fondo
        auto shader = m_fields->m_shaderNode;
        if (shader->m_time < 1.0f) {
            m_background->setShaderProgram(shader->m_program);
            shader->m_program->use();
            
            // Pasar parámetros al shader de forma segura
            glUniform2f(shader->m_program->getUniformLocationForName("u_center"), 0.5f, 0.5f);
            glUniform1f(shader->m_program->getUniformLocationForName("u_time"), shader->m_time);
        } else {
            // Volver al shader normal de Cocos si no hay onda
            auto normalProg = CCShaderCache::sharedShaderCache()->programForKey(kCCShader_PositionTextureColor);
            m_background->setShaderProgram(normalProg);
        }
    }
};
