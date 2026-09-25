#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <fmod.hpp>
#include <algorithm>
#include <iomanip>

using namespace geode::prelude;

float g_zoomMax = 1.0f;[cite: 1]
float g_bassIntensity = 0.9f;[cite: 1]
float g_shakeIntensity = 12.0f;[cite: 1]
int g_shaderMode = 0; // 0 = Desactivado, 1 = Bulge

// Shader Fragment GLSL para el efecto Bulge
const GLchar* bulgeFrag = R"(
    #ifdef GL_ES
    precision mediump float;
    #endif
    varying vec2 v_texCoord;
    uniform sampler2D u_texture;
    uniform float u_bassIntensity;

    void main() {
        vec2 center = vec2(0.5, 0.5);
        vec2 uv = v_texCoord;
        float dist = distance(uv, center);
        
        if (dist < 0.5) {
            float percent = dist / 0.5;
            float weight = (u_bassIntensity * 0.5) * (1.0 - percent * percent);
            uv -= center;
            uv *= (1.0 - weight);
            uv += center;
        }
        
        gl_FragColor = texture2D(u_texture, uv);
    }
)";

class MySettingsLayer : public FLAlertLayer {[cite: 1]
    TextInput* m_zoomInput;[cite: 1]
    TextInput* m_intensityInput;[cite: 1]
    TextInput* m_shakeInput;[cite: 1]
    TextInput* m_shaderInput;

public:
    static MySettingsLayer* create() {[cite: 1]
        auto ret = new MySettingsLayer();[cite: 1]
        if (ret && ret->init(150)) {[cite: 1]
            ret->autorelease();[cite: 1]
            return ret;[cite: 1]
        }
        CC_SAFE_DELETE(ret);[cite: 1]
        return nullptr;[cite: 1]
    }

    bool init(int bgOpacity) {[cite: 1]
        if (!FLAlertLayer::init(bgOpacity)) return false;[cite: 1]

        auto winSize = CCDirector::sharedDirector()->getWinSize();[cite: 1]
        
        auto bg = CCScale9Sprite::create("GJ_square04.png");[cite: 1]
        bg->setContentSize({ 280, 290 });[cite: 1]
        bg->setPosition(winSize / 2);[cite: 1]
        m_mainLayer->addChild(bg);[cite: 1]

        m_buttonMenu = CCMenu::create();[cite: 1]
        m_mainLayer->addChild(m_buttonMenu);[cite: 1]

        auto title = CCLabelBMFont::create("Settings", "goldFont.fnt");[cite: 1]
        title->setPosition({ winSize.width / 2, winSize.height / 2 + 120 });[cite: 1]
        title->setScale(0.7f);[cite: 1]
        m_mainLayer->addChild(title);[cite: 1]

        g_zoomMax = Mod::get()->getSavedValue<float>("save_zoom", 1.0f);[cite: 1]
        g_bassIntensity = Mod::get()->getSavedValue<float>("save_intensity", 0.9f);[cite: 1]
        g_shakeIntensity = Mod::get()->getSavedValue<float>("save_shake", 12.0f);[cite: 1]
        g_shaderMode = Mod::get()->getSavedValue<int64_t>("save_shader_mode", 0);

        m_zoomInput = createInput("Zoom", 75, g_zoomMax, "save_zoom", &g_zoomMax);[cite: 1]
        m_intensityInput = createInput("Intensity", 25, g_bassIntensity, "save_intensity", &g_bassIntensity);[cite: 1]
        m_shakeInput = createInput("Shake", -25, g_shakeIntensity, "save_shake", &g_shakeIntensity);[cite: 1]

        // Input para cambiar el shader (0 = Apagado, 1 = Bulge)
        auto shaderLabel = CCLabelBMFont::create("Shader (0=Off, 1=Bulge)", "bigFont.fnt");
        shaderLabel->setScale(0.35f);
        shaderLabel->setPosition({0, -50});
        m_buttonMenu->addChild(shaderLabel);

        m_shaderInput = TextInput::create(100.f, "Shader", "chatFont.fnt");
        m_shaderInput->setFilter("01");
        m_shaderInput->setString(std::to_string(g_shaderMode));
        m_shaderInput->setPosition({0, -75});
        m_shaderInput->setCallback([](const std::string& text) {
            if (text.empty()) return;
            try {
                int val = std::stoi(text);
                g_shaderMode = val;
                Mod::get()->setSavedValue("save_shader_mode", static_cast<int64_t>(val));
            } catch(...) {}
        });
        m_buttonMenu->addChild(m_shaderInput);

        auto closeBtn = CCMenuItemSpriteExtra::create([cite: 1]
            CCSprite::createWithSpriteFrameName("GJ_closeBtn_001.png"),[cite: 1]
            this, menu_selector(MySettingsLayer::onClose));[cite: 1]
        closeBtn->setPosition({ -130, 130 });[cite: 1]
        m_buttonMenu->addChild(closeBtn);[cite: 1]

        auto infoBtn = CCMenuItemSpriteExtra::create([cite: 1]
            CCSprite::createWithSpriteFrameName("GJ_infoIcon_001.png"),[cite: 1]
            this, menu_selector(MySettingsLayer::onInfo));[cite: 1]
        infoBtn->setPosition({ 130, 130 });[cite: 1]
        m_buttonMenu->addChild(infoBtn);[cite: 1]

        this->setTouchEnabled(true);[cite: 1]
        this->setKeypadEnabled(true);[cite: 1]
        return true;[cite: 1]
    }

    TextInput* createInput(const char* labelStr, float y, float initialVal, std::string saveKey, float* globalVar) {[cite: 1]
        auto label = CCLabelBMFont::create(labelStr, "bigFont.fnt");[cite: 1]
        label->setScale(0.4f);[cite: 1]
        label->setPosition({0, y + 22});[cite: 1]
        m_buttonMenu->addChild(label);[cite: 1]

        auto input = TextInput::create(100.f, labelStr, "chatFont.fnt");[cite: 1]
        input->setFilter("0123456789.");[cite: 1]
        
        std::stringstream ss;[cite: 1]
        ss << std::fixed << std::setprecision(2) << initialVal;[cite: 1]
        input->setString(ss.str());[cite: 1]
        
        input->setPosition({0, y});[cite: 1]
        
        input->setCallback([saveKey, globalVar](const std::string& text) {[cite: 1]
            if (text.empty()) return;[cite: 1]
            try {[cite: 1]
                float val = std::stof(text);[cite: 1]
                *globalVar = val;[cite: 1]
                Mod::get()->setSavedValue(saveKey, val);[cite: 1]
            } catch(...) {}[cite: 1]
        });[cite: 1]

        m_buttonMenu->addChild(input);[cite: 1]
        return input;[cite: 1]
    }

    void onInfo(CCObject*) {[cite: 1]
        FLAlertLayer::create([cite: 1]
            "Help",[cite: 1]
            "<cy>Original Code by:</c> <cr>thesillydoggo</c> and <cp>EryManthus</c> luv for them <3!\n\n"[cite: 1]
            "<cg>Zoom:</c> How much the background scales with music.\n"[cite: 1]
            "<cg>Intensity:</c> Bass sensitivity.\n"[cite: 1]
            "<cg>Shake:</c> Background vibration.\n"[cite: 1]
            "<cg>Shader:</c> 0 = Off, 1 = Bulge Effect.\n\n"
            "<cy>PC:</c> Type with keyboard | <cg>Mobile:</c> Tap to type.",[cite: 1]
            "OK"[cite: 1]
        )->show();[cite: 1]
    }
    
    void onClose(CCObject*) { this->removeFromParentAndCleanup(true); }[cite: 1]
    void keyBackClicked() override { onClose(nullptr); }[cite: 1]
};

class BGPulsingNode : public CCNode {[cite: 1]
public:
    CCSprite* bg = nullptr;[cite: 1]
    FMOD::DSP* fftDSP = nullptr;[cite: 1]
    float smoothBass = 0.0f;[cite: 1]
    float baseScale = 1.0f;[cite: 1]
    CCPoint basePos;[cite: 1]
    CCGLProgram* shaderProgram = nullptr;

    static BGPulsingNode* create(CCSprite* bg) {[cite: 1]
        auto ret = new BGPulsingNode();[cite: 1]
        if (ret && ret->init(bg)) {[cite: 1]
            ret->autorelease();[cite: 1]
            return ret;[cite: 1]
        }
        CC_SAFE_DELETE(ret);[cite: 1]
        return nullptr;[cite: 1]
    }

    bool init(CCSprite* target) {[cite: 1]
        if (!CCNode::init()) return false;[cite: 1]
        bg = target;[cite: 1]
        baseScale = bg->getScale();[cite: 1]
        basePos = bg->getPosition();[cite: 1]

        auto engine = FMODAudioEngine::sharedEngine();[cite: 1]
        auto sys = engine->m_system;[cite: 1]
        FMOD::ChannelGroup* master = nullptr;[cite: 1]
        sys->getMasterChannelGroup(&master);[cite: 1]
        sys->createDSPByType(FMOD_DSP_TYPE_FFT, &fftDSP);[cite: 1]
        fftDSP->setParameterInt(FMOD_DSP_FFT_WINDOWSIZE, 512);[cite: 1]
        master->addDSP(0, fftDSP);[cite: 1]

        // Inicializar shader de Bulge si la opción 1 está activa
        if (g_shaderMode == 1) {
            shaderProgram = new CCGLProgram();
            shaderProgram->initWithVertexShaderByteArray(ccPositionTextureColor_vert, bulgeFrag);
            shaderProgram->addAttribute(kCCAttributeNamePosition, kCCVertexAttrib_Position);
            shaderProgram->addAttribute(kCCAttributeNameColor, kCCVertexAttrib_Color);
            shaderProgram->addAttribute(kCCAttributeNameTexCoord, kCCVertexAttrib_TexCoords);
            shaderProgram->link();
            shaderProgram->updateUniforms();
            bg->setShaderProgram(shaderProgram);
        }

        scheduleUpdate();[cite: 1]
        return true;[cite: 1]
    }

    void update(float dt) override {[cite: 1]
        if (!fftDSP || !bg) return;[cite: 1]
        
        FMOD_DSP_PARAMETER_FFT* fft = nullptr;[cite: 1]
        fftDSP->getParameterData(FMOD_DSP_FFT_SPECTRUMDATA, (void**)&fft, nullptr, nullptr, 0);[cite: 1]

        float bass = 0.0f;[cite: 1]
        if (fft && fft->numchannels > 0 && fft->spectrum[0]) {[cite: 1]
            for (int i = 0; i < 8; i++) bass += fft->spectrum[0][i];[cite: 1]
            bass /= 8;[cite: 1]
        }
        smoothBass += (bass - smoothBass) * dt * 14.0f;[cite: 1]

        float currentBassValue = smoothBass * g_bassIntensity;[cite: 1]
        bg->setScale(baseScale * (1.0f + (currentBassValue * g_zoomMax)));[cite: 1]
        
        float shake = currentBassValue * g_shakeIntensity;[cite: 1]
        bg->setPosition({[cite: 1]
            basePos.x + CCRANDOM_MINUS1_1() * shake,[cite: 1]
            basePos.y + CCRANDOM_MINUS1_1() * shake[cite: 1]
        });[cite: 1]

        // Pasar la intensidad de los bajos al Shader en cada cuadro
        if (g_shaderMode == 1 && shaderProgram) {
            shaderProgram->use();
            GLuint intensityLoc = glGetUniformLocation(shaderProgram->getProgram(), "u_bassIntensity");
            glUniform1f(intensityLoc, currentBassValue);
        }
    }

    ~BGPulsingNode() { 
        if (fftDSP) fftDSP->release();[cite: 1]
        if (shaderProgram) shaderProgram->release();
    }
};

class $modify(MyMenuLayer, MenuLayer) {[cite: 1]
    bool init() {[cite: 1]
        if (!MenuLayer::init()) return false;[cite: 1]

        g_zoomMax = Mod::get()->getSavedValue<float>("save_zoom", 1.0f);[cite: 1]
        g_bassIntensity = Mod::get()->getSavedValue<float>("save_intensity", 0.9f);[cite: 1]
        g_shakeIntensity = Mod::get()->getSavedValue<float>("save_shake", 12.0f);[cite: 1]
        g_shaderMode = Mod::get()->getSavedValue<int64_t>("save_shader_mode", 0);

        auto bg = static_cast<CCSprite*>(this->getChildByID("main-menu-bg"));[cite: 1]
        if (bg) this->addChild(BGPulsingNode::create(bg), -1);[cite: 1]

        if (auto bottomMenu = this->getChildByID("bottom-menu")) {[cite: 1]
            auto sprite = CCSprite::createWithSpriteFrameName("GJ_optionsBtn02_001.png");[cite: 1]
            auto btn = CCMenuItemSpriteExtra::create([cite: 1]
                sprite, this, menu_selector(MyMenuLayer::onCustomSettings)[cite: 1]
            );[cite: 1]
            bottomMenu->addChild(btn);[cite: 1]
            bottomMenu->updateLayout();[cite: 1]
        }
        return true;[cite: 1]
    }

    void onCustomSettings(CCObject* sender) {[cite: 1]
        MySettingsLayer::create()->show();[cite: 1]
    }
};
