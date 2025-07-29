// RSModule.cpp
#include "plugin.hpp"
#include <cmath>
#include <vector>
#include "filtres.hpp"

// === Fonctions auxiliaires ===

float Mean(const std::vector<float>& buf) {
    float sum = 0.f;
    for (float v : buf) sum += v;
    return buf.empty() ? 0.f : sum / buf.size();
}

float Max(const std::vector<float>& buf) {
    float mx = 0.f;
    for (float v : buf)
        if (fabs(v) > fabs(mx))
            mx = v;
    return mx;
}

// === Classe principale du module ===

struct RSModule : Module {
    // États internes
    float signal = 0.f, noise = 0.f, threshold = 1.f;
    float filtred_signal = 0.f;
    float XB = 1.f, tau = 1.f / 300.f, xi = -1.f;
    float dt = 0.01f;
    int current_filter = 0; // 1: Diode, 2: Diode2, 3: Bistable

    std::vector<float> buffer_y;
    std::vector<float> buffer_x;
    const size_t bufferSize = 512;

    // Paramètres du module
    enum ParamId {
        TIME_PARAM,
        GAIN_PARAM,
        STATIC_THRESHOLD,
        STATIC_MOD_PARAM,
        DYNAMIC_WHEEL_NUM,
        DYNAMIC_SYSTEM_TIME,
        DYNAMIC_SYSTEM_TIME_MOD_PARAM,
        DYNAMIC_WHEEL_POS,
        DYNAMIC_WHEEL_POS_MOD_PARAM,
        REVERB_PARAM,
        SWITCH_BISTABLE,
        SWITCH_DIODE1,
        SWITCH_DIODE2,
        PARAMS_LEN
    };

    enum InputId {
        INPUT_NOISE,
        INPUT_SIGNAL,
        STATIC_MOD_INPUT,
        DYNAMIC_WHEEL_POS_MOD_INPUT,
        DYNAMIC_SYSTEM_TIME_MOD_INPUT,
        INPUTS_LEN
    };

    enum OutputId {
        OUTPUT,
        OUTPUTS_LEN
    };

    enum LightId {
        BISTABLE_LIGHT,
        DIODE1_LIGHT,
        DIODE2_LIGHT,
        LIGHTS_LEN
    };

    RSModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

        configParam(TIME_PARAM, 2.f, 100.f, 18.f, "Time scaling");
        configParam(GAIN_PARAM, 0.1f, 20.f, 10.f, "Gain");
        configParam(STATIC_THRESHOLD, 0.f, 10.f, 1.f, "Static Threshold");
        configInput(STATIC_MOD_INPUT, "Static Modulation Input");
        configParam(STATIC_MOD_PARAM, 0.f, 1.f, 0.f, "Static Modulation Parameter");
        configParam(DYNAMIC_WHEEL_NUM, 1.f, 16.f, 1.f, "Dynamic Wheel Number");
        configParam(DYNAMIC_SYSTEM_TIME, 0.1f, 1000.f, 10.f, "Dynamic System Time");
        configInput(DYNAMIC_SYSTEM_TIME_MOD_INPUT, "Dynamic System Time Modulation Input");
        configParam(DYNAMIC_SYSTEM_TIME_MOD_PARAM, 0.f, 10.f, 1.f, "Dynamic System Time Modulation Parameter");
        configParam(DYNAMIC_WHEEL_POS, 0.1f, 10.f, 1.f, "Dynamic Wheel Position");
        configInput(DYNAMIC_WHEEL_POS_MOD_INPUT, "Dynamic Wheel Position Modulation Input");
        configParam(DYNAMIC_WHEEL_POS_MOD_PARAM, 0.f, 1.f, 0.f, "Dynamic Wheel Position Modulation Parameter");
        configParam(REVERB_PARAM, 0.f, 1.f, 0.f, "Reverb Parameter");
    }

    void updateSwitches() {
        bool bistable_enabled = params[SWITCH_BISTABLE].getValue() > 0.5f;
        bool diode1_enabled = params[SWITCH_DIODE1].getValue() > 0.5f;
        bool diode2_enabled = params[SWITCH_DIODE2].getValue() > 0.5f;

        if (bistable_enabled) {
            current_filter = 3;
        } else if (diode1_enabled) {
            current_filter = 1;
        } else if (diode2_enabled) {
            current_filter = 2;
        }
    }
    // Fonction de filtrage
    float getFilteredSignal() {
        int N = (int)params[DYNAMIC_WHEEL_NUM].getValue();
        float XB = params[DYNAMIC_WHEEL_POS].getValue();
        float threshold = params[STATIC_THRESHOLD].getValue();
        float signal = inputs[INPUT_SIGNAL].getVoltage();
        float noise = inputs[INPUT_NOISE].getVoltage();
        float tau = 1.f / params[DYNAMIC_SYSTEM_TIME].getValue();

        float dt = this->dt;
        float filtred_signal = 0.f;

        if (N < 1) N = 1;

        if (current_filter == 0) {
            filtred_signal = signal + noise;
        } else if (current_filter == 1) {
            filtred_signal = diode(signal + noise, threshold);
        } else if (current_filter == 2) {
            filtred_signal = rubber(signal + noise, threshold);
        } else if (current_filter == 3) {
            filtred_signal = multiWellFilter(xi, signal, noise, dt, tau, N, XB);
            xi = filtred_signal; // mise à jour de l'état interne
        }
        return filtred_signal;
    }

    void process(const ProcessArgs& args) override {
        // Lecture des entrées
        signal = inputs[INPUT_SIGNAL].getVoltage();
        noise = inputs[INPUT_NOISE].getVoltage();
        dt = args.sampleTime;
		filtred_signal = 0.1f;

        updateSwitches();

        filtred_signal = getFilteredSignal();

        outputs[OUTPUT].setVoltage(filtred_signal);

        // Mise à jour du buffer pour affichage
        buffer_y.push_back(filtred_signal);
        buffer_x.push_back(signal + noise);
        if (buffer_y.size() > bufferSize) {
            buffer_y.erase(buffer_y.begin());
            buffer_x.erase(buffer_x.begin());
        }

        // Mise à jour des lumières
        lights[BISTABLE_LIGHT].setBrightness(current_filter == 3 ? 1.f : 0.f);
        lights[DIODE1_LIGHT].setBrightness(current_filter == 1 ? 1.f : 0.f);	
        lights[DIODE2_LIGHT].setBrightness(current_filter == 2 ? 1.f : 0.f);
    }


    
};

// === Affichage graphique du potentiel ===

struct GraphDisplay : Widget {
    RSModule* module;
    Vec position;
    Vec size;

    GraphDisplay(RSModule* module, Vec pos, Vec size)
        : module(module), position(pos), size(size) {
        this->box.pos = pos;
        this->box.size = size;
    }

    float getFiltreProfil(float x) {
        float threshold = module->params[RSModule::STATIC_THRESHOLD].getValue();
        float XB = module->params[RSModule::DYNAMIC_WHEEL_POS].getValue();
        int current_filter = module->current_filter;

        if (current_filter == 1) {
            return diode(x, threshold);
        } else if (current_filter == 2) {
            return rubber(x, threshold);
        } else if (current_filter == 3) {
            return multi_well_potential(x, module->params[RSModule::DYNAMIC_WHEEL_NUM].getValue(), XB);
        }
        return 0.f;
    }

    void draw(const DrawArgs& args) override {
        if (!module) return;

        float threshold = module->params[RSModule::STATIC_THRESHOLD].getValue();
        float XB = module->params[RSModule::DYNAMIC_WHEEL_POS].getValue();
        float gain = module->params[RSModule::GAIN_PARAM].getValue();
        float time = module->params[RSModule::TIME_PARAM].getValue();
        bool diode_enabled = module->params[RSModule::SWITCH_DIODE1].getValue() > 0.5f;
        int Num = (int)module->params[RSModule::DYNAMIC_WHEEL_NUM].getValue();

        float W = size.x;
        float H = size.y;
        float x_center = W / 2.f;
        float y_center = H / 2.f;

        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, 0, y_center);
        nvgLineTo(args.vg, W, y_center);
        nvgMoveTo(args.vg, x_center, 0);
        nvgLineTo(args.vg, x_center, H);
        nvgStrokeColor(args.vg, nvgRGB(180, 180, 180));
        nvgStrokeWidth(args.vg, 1.0);
        nvgStroke(args.vg);

        nvgBeginPath(args.vg);
        nvgStrokeColor(args.vg, nvgRGB(0x00, 0xff, 0x00));
        nvgStrokeWidth(args.vg, 1.5);

        int N = 1000;
        float domain = diode_enabled ? threshold + 5 : 2*Num*XB + 5;
        float x1, x2, y1, y2;

        for (int i = 0; i < N; ++i) {
            x1 = -domain + 2.f * domain * i / N;
            x2 = -domain + 2.f * domain * (i + 1) / N;
            y1 = getFiltreProfil(x1);
            y2 = getFiltreProfil(x2);

            x1 = x_center + x1 * time;
            x2 = x_center + x2 * time;
            y1 = y_center - y1 * gain;
            y2 = y_center - y2 * gain;

            if ((y1 <= (box.pos[1] - H / 4 + H) && y1 >= box.pos[1] - H / 4) ||
                (y2 <= (box.pos[1] - H / 4 + H) && y2 >= box.pos[1] - H / 4)) {
                nvgMoveTo(args.vg, x1, y1);
                nvgLineTo(args.vg, x2, y2);
            }
        }
        nvgStroke(args.vg);

        // Cercle pour la position max
        float cx = Max(module->buffer_y);
        float cy = getFiltreProfil(cx);

        cx = x_center + cx * time;
        cy = y_center - cy * gain;

        if ((cy <= (box.pos[1] - H / 4 + H) && cy >= box.pos[1] - H / 4) ||
            (cx <= (box.pos[0] - W / 4 + W) && cx >= box.pos[0] - W / 4)) {
            nvgBeginPath(args.vg);
            nvgFillColor(args.vg, nvgRGB(0, 0, 255));
            nvgCircle(args.vg, cx, cy, 4.0);
            nvgFill(args.vg);
        }
    }
};

// === Widget de l'interface graphique du module ===

struct RSModuleWidget : ModuleWidget {
    RSModuleWidget(RSModule* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/RSModule.svg")));

        // Vis de fixation
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // Affichage du graphe du potentiel
        addChild(new GraphDisplay(module, mm2px(Vec(0.1, 10.0)), mm2px(Vec(101, 41))));
        // Contrôles du graphe
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(7.92, 58.78)), module, RSModule::TIME_PARAM));
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(21.92, 58.78)), module, RSModule::GAIN_PARAM));

        // Boutons de contrôle
        addParam(createParamCentered<LEDButton>(mm2px(Vec(8.1995, 71.0915)), module, RSModule::SWITCH_DIODE1));
        addParam(createParamCentered<LEDButton>(mm2px(Vec(18.4695, 71.0915)), module, RSModule::SWITCH_DIODE2));
        addParam(createParamCentered<LEDButton>(mm2px(Vec(32.0175, 71.0915)), module, RSModule::SWITCH_BISTABLE));
        addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(8.1995, 71.0915)), module, RSModule::DIODE1_LIGHT));
        addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(18.4695, 71.0915)), module, RSModule::DIODE2_LIGHT));	
        addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(32.0175, 71.0915)), module, RSModule::BISTABLE_LIGHT));

        // Potentiomètres de contrôle (section Diodes)
        addParam(createParamCentered<RoundHugeBlackKnob>(mm2px(Vec(16.097, 88.879)), module, RSModule::STATIC_THRESHOLD));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(34.300, 95.822)), module, RSModule::STATIC_MOD_INPUT));
        addParam(createParamCentered<Trimpot>(mm2px(Vec(34.300, 84.738)), module, RSModule::STATIC_MOD_PARAM));
        
        // Potentiomètres de contrôle (section Bistable)
        addParam(createParamCentered<RoundHugeBlackKnob>(mm2px(Vec(70.801, 68.711)), module, RSModule::DYNAMIC_WHEEL_NUM));
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(49.68, 67.983)), module, RSModule::DYNAMIC_SYSTEM_TIME));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(49.68, 85.144)), module, RSModule::DYNAMIC_SYSTEM_TIME_MOD_INPUT));
        addParam(createParamCentered<Trimpot>(mm2px(Vec(49.68, 96.933)), module, RSModule::DYNAMIC_SYSTEM_TIME_MOD_PARAM));
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(94.201, 69.305)), module, RSModule::DYNAMIC_WHEEL_POS));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(94.201, 84.062)), module, RSModule::DYNAMIC_WHEEL_POS_MOD_INPUT));
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(94.201, 95.245)), module, RSModule::DYNAMIC_WHEEL_POS_MOD_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(72.585, 104.8)), module, RSModule::REVERB_PARAM));

        // Entrées
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.36, 114.64)), module, RSModule::INPUT_NOISE));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(43.992, 114.64)), module, RSModule::INPUT_SIGNAL));

        // Sortie
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(84.472, 114.64)), module, RSModule::OUTPUT));
    }
};

// Enregistrement du module auprès de VCV Rack
Model* modelRSModule = createModel<RSModule, RSModuleWidget>("RSModule");