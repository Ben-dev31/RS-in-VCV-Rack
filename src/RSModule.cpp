// RSModule.cpp
#include "plugin.hpp"
#include <cmath>
#include <vector>
#include <iostream>
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

    int current_well_num = 1; // Numéro de puits 
    float delayBuffer[44100];
    int delayIndex = 0;
    
    float time = 0.f;
    float lastNoteTime = 0.2f;
    float noteInterval = .1f; // Intervalle entre les notes
    int closestWell = 0; // Index de la roue la plus proche

    std::vector<float> wellsPosition;
    // MIDI note list 
    std::vector<int> midiNotes = {60, 62, 72,64, 65, 67, 69, 71, 72, 74, 76, 77 };

    //MIDI note -> voltage conversion
    float midiToVolts(int note) {
        return 5.f * (note - 60) / 12.f;
    }

    std::vector<float> buffer_y;
    std::vector<float> buffer_x;
    const size_t bufferSize = 512;

    // Paramètres du module
    enum ParamId {
        TIME_PARAM,
        GAIN_PARAM,
        STATIC_THRESHOLD,
        STATIC_MOD_PARAM,
        DYNAMIC_well_NUM,
        DYNAMIC_SYSTEM_TIME,
        DYNAMIC_SYSTEM_TIME_MOD_PARAM,
        DYNAMIC_well_POS,
        DYNAMIC_well_POS_MOD_PARAM,
        NOTE_RATE,
        SWITCH_BISTABLE,
        SWITCH_DIODE1,
        SWITCH_DIODE2,
        MODE_PARAM,
        PARAMS_LEN
    };

    enum InputId {
        INPUT_NOISE,
        INPUT_SIGNAL,
        STATIC_MOD_INPUT,
        DYNAMIC_well_POS_MOD_INPUT,
        DYNAMIC_SYSTEM_TIME_MOD_INPUT,
        INPUT_GATE,
        INPUTS_LEN
    };

    enum OutputId {
        OUTPUT,
        GATE_OUTPUT,
        VOCT_OUTPUT,
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
        configParam(GAIN_PARAM, 10.f, 100.f, 20.f, "Gain");
        configParam(STATIC_THRESHOLD, 0.f, 10.f, 1.f, "Static Threshold");
        configParam(STATIC_MOD_PARAM, 0.f, 1.f, 0.f, "Static Modulation Parameter");
        configParam(DYNAMIC_well_NUM, 1.f, 16.f, 1.f, "Dynamic well Number");
        configParam(DYNAMIC_SYSTEM_TIME, 0.1f, 1000.f, 10.f, "Dynamic System Time");
        configInput(DYNAMIC_SYSTEM_TIME_MOD_INPUT, "Dynamic System Time Modulation Input");
        configParam(DYNAMIC_SYSTEM_TIME_MOD_PARAM, 0.f, 1e-5f, 0.f, "Dynamic System Time Modulation Parameter");
        configParam(DYNAMIC_well_POS, 0.1f, 10.f, 1.f, "Dynamic well Position");
        configInput(DYNAMIC_well_POS_MOD_INPUT, "Dynamic well Position Modulation Input");
        configParam(DYNAMIC_well_POS_MOD_PARAM, 0.f, 1.f, 0.f, "Dynamic well Position Modulation Parameter");
        configParam(NOTE_RATE, 0.f, 1.f, 0.2f, "Gate Frequency Parameter");
        configParam(SWITCH_BISTABLE, 0.f, 1.f, 0.f, "Bistable Switch");
        configParam(SWITCH_DIODE1, 0.f, 1.f, 0.f, "Diode 1 Switch");
        configParam(SWITCH_DIODE2, 0.f, 1.f, 0.f, "Diode 2 Switch");
        configParam(MODE_PARAM, 0.f, 1.f, 0.f, "Mode Switch (Normal/Rate) Mode");

        configOutput(GATE_OUTPUT, "Gate Output");
        configOutput(VOCT_OUTPUT, "V/oct Output");
        configOutput(OUTPUT, "Filtered Output");

        configInput(STATIC_MOD_INPUT, "Static Modulation Input");
        configInput(INPUT_NOISE, "Noise Input");
        configInput(INPUT_SIGNAL, "Signal Input");
        configInput(INPUT_GATE, "Gate Modulation Input");

    }

    void onReset() override {
        signal = 0.f;
        noise = 0.f;
        filtred_signal = 0.f;
        XB = 1.f;
        tau = 1.f / 300.f;
        xi = -1.f;
        buffer_y.clear();
        buffer_x.clear();
        time = 0.f;
        lastNoteTime = 0.2f;
       
        setwellsPositions();

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
        int N = (int)params[DYNAMIC_well_NUM].getValue();
        float XB = params[DYNAMIC_well_POS].getValue();
        float threshold = params[STATIC_THRESHOLD].getValue();
        float signal = inputs[INPUT_SIGNAL].getVoltage();
        float noise = inputs[INPUT_NOISE].getVoltage();
        float tau = 1.f / params[DYNAMIC_SYSTEM_TIME].getValue();

        if (inputs[STATIC_MOD_INPUT].isConnected()) {
            float static_mod = inputs[STATIC_MOD_INPUT].getVoltage();
            static_mod = fabs(static_mod) >= 1.f ? 1.f : static_mod;
            threshold += static_mod * params[STATIC_MOD_PARAM].getValue();
        }
        if (inputs[DYNAMIC_well_POS_MOD_INPUT].isConnected()) {
            float dynamic_mod = inputs[DYNAMIC_well_POS_MOD_INPUT].getVoltage();
            dynamic_mod = fabs(dynamic_mod) >= 1.f ? 1.f : dynamic_mod;
            XB += dynamic_mod * params[DYNAMIC_well_POS_MOD_PARAM].getValue();
        }
        if (inputs[DYNAMIC_SYSTEM_TIME_MOD_INPUT].isConnected()) {
            float dynamic_mod = inputs[DYNAMIC_SYSTEM_TIME_MOD_INPUT].getVoltage();
            dynamic_mod = fabs(dynamic_mod) >= 1.f ? 1.f : dynamic_mod;
            tau += dynamic_mod * params[DYNAMIC_SYSTEM_TIME_MOD_PARAM].getValue();
            
        }

        float dt = this->dt;
        float filtred_signal = 0.f;
        float xi = this->xi; // État interne du filtre

        // Vérification des valeurs
        //std::cout<< "dt: "<< dt << std::endl;

        if (N < 1) N = 1;

        if (current_filter == 0) {
            filtred_signal = 0.f; // Pas de filtrage
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

    // wells positions 
    void setwellsPositions() {
        int N = (int)params[DYNAMIC_well_NUM].getValue();
        float XB = params[DYNAMIC_well_POS].getValue();
        float L = 2.0f * XB;
        wellsPosition.resize(N+1);

        std::vector<float> x0_list(N);
        for (int i = 0; i < N; ++i)
            wellsPosition[i] = (i - (N - 1) / 2.0f) * L;
       
    }
    // well number
    int getCurrentwellNum(float v) {
        float XB = params[DYNAMIC_well_POS].getValue();
        float min_diff = std::numeric_limits<float>::max();

        for (int i = 0; i < (int)wellsPosition.size(); ++i) {
            float diff = fabs(wellsPosition[i] - v);
            if (diff <= XB && diff < min_diff) {
                min_diff = diff;
                return i;
            }
        }
        return 0;
    }
       

    void process(const ProcessArgs& args) override {
        // Lecture des entrées
        signal = inputs[INPUT_SIGNAL].getVoltage();
        noise = inputs[INPUT_NOISE].getVoltage();

        noteInterval = params[NOTE_RATE].getValue();
        dt = args.sampleTime;

        time += dt; // Mise à jour du temps écoulé

     
        float v_oct = 0.f;
        updateSwitches();
        setwellsPositions();

        filtred_signal = getFilteredSignal();

        
        if(filtred_signal > 5.f) {
            filtred_signal = 5.f; // Limite supérieure
        } else if (filtred_signal < -5.f) {
            filtred_signal = -5.f; // Limite inférieure
        }
        
        if(current_filter == 3){

            if((time - lastNoteTime) >= noteInterval){
                current_well_num = getCurrentwellNum(filtred_signal);
                if(current_well_num == closestWell) {
                    outputs[GATE_OUTPUT].setVoltage(10.f);
                } else {
                    outputs[GATE_OUTPUT].setVoltage(0.f);
                    closestWell = current_well_num;
                }
                
                lastNoteTime = time;
                //outputs[GATE_OUTPUT].setVoltage(0.f);
                
            }else{
                outputs[GATE_OUTPUT].setVoltage(10.f);
            }

       
        
        
            v_oct = midiToVolts(midiNotes[closestWell]);
            outputs[VOCT_OUTPUT].setVoltage(v_oct);
        }
        

        outputs[OUTPUT].setVoltage(filtred_signal);

        // Mise à jour du buffer pour affichage
        buffer_y.push_back(filtred_signal);
        buffer_x.push_back(signal + noise);
        if (buffer_y.size() > bufferSize) {
            buffer_y.erase(buffer_y.begin());
            buffer_x.erase(buffer_x.begin());
        }

        if(current_filter == 3 ){
            xi = filtred_signal; // Mise à jour de l'état interne pour le filtre bistable
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

    float rate = 1.f; // Taux de rafraîchissement du graphique
    float lastUpdateTime = 0.f;

    float lcy = 0.f; // Position y du cercle de la position max
    float lcx = 0.f; // Position x du cercle de la position max

    GraphDisplay(RSModule* module, Vec pos, Vec size)
        : module(module), position(pos), size(size) {
        this->box.pos = pos;
        this->box.size = size;
    }

    float getFiltreProfil(float x) {
        float threshold = module->params[RSModule::STATIC_THRESHOLD].getValue();
        float XB = module->params[RSModule::DYNAMIC_well_POS].getValue();
        int current_filter = module->current_filter;

        

        if (current_filter == 1) {
            return diode(x, threshold);
        } else if (current_filter == 2) {
            return rubber(x, threshold);
        } else if (current_filter == 3) {
            return multi_well_potential(x, module->params[RSModule::DYNAMIC_well_NUM].getValue(), XB);
        }
        return 0.f;
    }

    void draw(const DrawArgs& args) override {
        if (!module) return;

        float threshold = module->params[RSModule::STATIC_THRESHOLD].getValue();
        float XB = module->params[RSModule::DYNAMIC_well_POS].getValue();
        float gain = module->params[RSModule::GAIN_PARAM].getValue();
        float time = module->params[RSModule::TIME_PARAM].getValue();
        int Num = (int)module->params[RSModule::DYNAMIC_well_NUM].getValue();

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
        float domain = (module->current_filter != 3) ? threshold + 5 : 2*Num*XB + 5;
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
        
        float mode = module->params[RSModule::MODE_PARAM].getValue();
        if (mode > 0.5f && module->current_filter == 3) {
            if( (module->time - lastUpdateTime) >= module->params[RSModule::NOTE_RATE].getValue()) {
            
            
                bool bistable_enabled = (module->current_filter == 3);
                float cx = bistable_enabled? Max(module->buffer_y): Max(module->buffer_x);

                //std::cout <<bistable_enabled<< " cx: " << cx << "   bx: "<<Max(module->buffer_x)<<"  by: "<<Max(module->buffer_y)<< std::endl;

            
                float cy = getFiltreProfil(cx);
            
                lcx = x_center + cx * time;
                lcy = y_center - cy * gain;

                
                lastUpdateTime = module->time;
            }else{
                if ((lcy <= (box.pos[1] - H / 4 + H) && lcy >= box.pos[1] - H / 4) ||
                    (lcx <= (box.pos[0] - W / 4 + W) && lcx >= box.pos[0] - W / 4)) {
                    nvgBeginPath(args.vg);
                    nvgFillColor(args.vg, nvgRGB(0, 0, 255));
                    nvgCircle(args.vg, lcx, lcy, 4.0);
                    nvgFill(args.vg);
                }
            }
        } else {
            bool bistable_enabled = (module->current_filter == 3);
            float cx = bistable_enabled? Max(module->buffer_y): Max(module->buffer_x);

        
            float cy = getFiltreProfil(cx);
        
            lcx = x_center + cx * time;
            lcy = y_center - cy * gain;
                if ((lcy <= (box.pos[1] - H / 4 + H) && lcy >= box.pos[1] - H / 4) ||
                (lcx <= (box.pos[0] - W / 4 + W) && lcx >= box.pos[0] - W / 4)) {
                nvgBeginPath(args.vg);
                nvgFillColor(args.vg, nvgRGB(0, 0, 255));
                nvgCircle(args.vg, lcx, lcy, 4.0);
                nvgFill(args.vg);
            }
        }
    }
};

// === Widget de l'interface graphique du module ===

struct RSModuleWidget : ModuleWidget {
    RSModuleWidget(RSModule* module) {
        setModule(module);
        setPanel(createPanel(
            asset::plugin(pluginInstance, "res/RSModule-dark.svg"),
            // Si vous avez une version claire, vous pouvez l'utiliser comme suit :
            // 
            asset::plugin(pluginInstance, "res/RSModule.svg")
            
        ));

        // Vis de fixation
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // Affichage du graphe du potentiel
        addChild(new GraphDisplay(module, mm2px(Vec(0.1, 10.0)), mm2px(Vec(106.125, 41))));
        // Contrôles du graphe
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(7.92, 58.4675)), module, RSModule::TIME_PARAM));
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(21.92, 58.4675)), module, RSModule::GAIN_PARAM));

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
        addParam(createParamCentered<RoundHugeBlackKnob>(mm2px(Vec(70.801, 68.711)), module, RSModule::DYNAMIC_well_NUM));
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(49.68, 67.983)), module, RSModule::DYNAMIC_SYSTEM_TIME));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(49.68, 85.144)), module, RSModule::DYNAMIC_SYSTEM_TIME_MOD_INPUT));
        addParam(createParamCentered<Trimpot>(mm2px(Vec(49.68, 96.933)), module, RSModule::DYNAMIC_SYSTEM_TIME_MOD_PARAM));
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(94.201, 69.305)), module, RSModule::DYNAMIC_well_POS));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(94.201, 84.062)), module, RSModule::DYNAMIC_well_POS_MOD_INPUT));
        addParam(createParamCentered<Trimpot>(mm2px(Vec(94.201, 95.245)), module, RSModule::DYNAMIC_well_POS_MOD_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(72.585, 91.441)), module, RSModule::NOTE_RATE));
        addParam(createParamCentered<CKSS>(mm2px(Vec(35.816, 58.4675)), module, RSModule::MODE_PARAM));

        // Entrées
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.36, 114.64)), module, RSModule::INPUT_NOISE));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(26.4375, 114.64)), module, RSModule::INPUT_SIGNAL));

        // Sortie
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(55.8495, 114.64)), module, RSModule::GATE_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(79.0685, 114.64)), module, RSModule::OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(97.8245, 114.64)), module, RSModule::VOCT_OUTPUT));
    }
};

// Enregistrement du module auprès de VCV Rack
Model* modelRSModule = createModel<RSModule, RSModuleWidget>("RSModule");