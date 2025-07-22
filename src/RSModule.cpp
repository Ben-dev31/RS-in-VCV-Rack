// RSModule.cpp
#include "plugin.hpp"
#include <cmath>
#include <vector>

// === Fonctions auxiliaires ===

// Filtre diode (dead zone au seuil)
float diode(float x, float th) {
	return (x >= th) ? (x - th) : 0.f;
}

// Fonction de type "caoutchouc" symétrique autour de 0 (non utilisée ici)
double rubber(float x, float th) {
	if (x >= th) return x - th;
	else if (x <= -th) return x + th;
	return 0.0;
}

// Filtre bistable basé sur un potentiel double puits
float bistableFilter(float xi, float si, float ni, float dt, float tau, float Xb) {
	return xi + dt / tau * (xi - 1.f / (Xb * Xb) * xi * xi * xi + si + ni);
}

// Potentiel bistable
float bistablePotential(float x, float th) {
	return -0.5f * x * x + (1.f / (4.f * th * th)) * x * x * x * x;
}

// Moyenne d'un vecteur
float Mean(const std::vector<float>& buf) {
	float sum = 0.f;
	for (float v : buf) sum += v;
	return buf.empty() ? 0.f : sum / buf.size();
}

// Valeur absolue maximale
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

	// Buffers pour l'affichage dynamique
	std::vector<float> buffer_y;
	std::vector<float> buffer_x;
	const size_t bufferSize = 512;

	// Paramètres du module
	enum ParamId {
		TIME_PARAM,
		GAIN_PARAM,
		THRESHOLD_PARAM,
		XB_PARAM,
		TAU_PARAM,
		L_PARAM,
		ENABLE_DIODE,
		ENABLE_BISTABLE,
		ENABLE_NONE,
		PARAMS_LEN
	};

	enum InputId {
		INPUT_NOISE,
		INPUT_SIGNAL,
		INPUTS_LEN
	};

	enum OutputId {
		OUTPUT,
		OUTPUTS_LEN
	};

	enum LightId {
		BISTABLE_LIGHT,
		DIODE_LIGHT,
		LIGHTS_LEN
	};

	RSModule() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		configParam(TIME_PARAM, 2.f, 100.f, 18.f, "Time scaling");
		configParam(GAIN_PARAM, 0.1f, 20.f, 10.f, "Gain");
		configParam(THRESHOLD_PARAM, 0.f, 5.f, 1.f, "Threshold");
		configParam(XB_PARAM, 0.1f, 5.f, 3.5f, "XB (potential width)");
		configParam(TAU_PARAM, 2, 2000, 10, "Bistable internal time");
		configParam(L_PARAM, 0.1f, 2.f, 0.1f, "Unused");
		configParam(ENABLE_BISTABLE, 0.f, 1.f, 0.f, "Enable bistable filter");
		configParam(ENABLE_DIODE, 0.f, 1.f, 0.f, "Enable diode filter");
		configParam(ENABLE_NONE, 0.f, 1.f, 0.f, "Bypass all filters");
	}

	void process(const ProcessArgs& args) override {
		dt = args.sampleTime;
		threshold = params[THRESHOLD_PARAM].getValue();
		XB = params[XB_PARAM].getValue();
		signal = inputs[INPUT_SIGNAL].getVoltage();
		noise = inputs[INPUT_NOISE].getVoltage();
		tau = 1.f/params[TAU_PARAM].getValue();

		bool useNone = params[ENABLE_NONE].getValue() > 0.5f;
		bool useBistable = params[ENABLE_BISTABLE].getValue() > 0.5f;
		bool useDiode = params[ENABLE_DIODE].getValue() > 0.5f;

		if (useNone) {
			filtred_signal = signal + noise;
		} else if (useBistable) {
			filtred_signal = bistableFilter(xi, signal, noise, dt, tau, XB);
			xi = filtred_signal;
		} else if (useDiode) {
			filtred_signal = diode(signal + noise, threshold);
		} else {
			filtred_signal = 0.f;
		}

		outputs[OUTPUT].setVoltage(filtred_signal);

		// LEDs de statut
		lights[BISTABLE_LIGHT].setBrightness(useBistable ? 1.f : 0.f);
		lights[DIODE_LIGHT].setBrightness(useDiode ? 1.f : 0.f);

		// Mise à jour du buffer pour affichage
		buffer_y.push_back(filtred_signal);
		buffer_x.push_back(signal + noise);
		if (buffer_y.size() > bufferSize) {
			buffer_y.erase(buffer_y.begin());
			buffer_x.erase(buffer_x.begin());
		}
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

	void draw(const DrawArgs& args) override {
		if (!module) return;

		float threshold = module->params[RSModule::THRESHOLD_PARAM].getValue();
		float XB = module->params[RSModule::XB_PARAM].getValue();
		float gain = module->params[RSModule::GAIN_PARAM].getValue();
		float time = module->params[RSModule::TIME_PARAM].getValue();

		bool bistable_enabled = module->params[RSModule::ENABLE_BISTABLE].getValue() > 0.5f;
		bool diode_enabled = module->params[RSModule::ENABLE_DIODE].getValue() > 0.5f;

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
		float domain = diode_enabled ? threshold + 5 : XB + 5;
		float x1,x2,y1,y2;

		for (int i = 0; i < N; ++i) {
			x1 = -domain + 2.f * domain * i / N;
			x2 = -domain + 2.f * domain * (i + 1) / N;
			y1 = bistable_enabled ? bistablePotential(x1, XB) : diode(x1, threshold);
			y2 = bistable_enabled ? bistablePotential(x2, XB) : diode(x2, threshold);
			
			x1 = x_center + x1 * time;
			x2 = x_center + x2 * time;
			y1 = y_center - y1 * gain;
			y2 = y_center - y2 * gain;

			if((y1 <= (box.pos[1]-H/4 + H) && y1 >= box.pos[1]-H/4) || (y2 <=(box.pos[1]-H/4 + H) && y2 >= box.pos[1]-H/4 ))
			{
				nvgMoveTo(args.vg, x1, y1);
            	nvgLineTo(args.vg, x2, y2);
			}
			
		}
		nvgStroke(args.vg);

		// Cercle pour la position max
		//float cx = Max(module->buffer_x);
		float cx = Max(module->buffer_y);
		float cy = bistable_enabled ? bistablePotential(cx, XB) : diode(cx, threshold);
		
		cx = x_center + cx * time;
		cy = y_center - cy * gain;

		if((cy <= (box.pos[1]-H/4 + H) && cy >= box.pos[1]-H/4) || (cx <=(box.pos[0]-W/4 + W) && cx >= box.pos[0]-W/4 ))
		{
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
		addChild(new GraphDisplay(module, mm2px(Vec(0.1, 10.0)), mm2px(Vec(81, 41))));

		// Commutateurs (switchs)
		addParam(createParamCentered<CKSS>(mm2px(Vec(7.92, 73)), module, RSModule::ENABLE_DIODE));
		addParam(createParamCentered<CKSS>(mm2px(Vec(33, 73)), module, RSModule::ENABLE_BISTABLE));
		addParam(createParamCentered<CKSS>(mm2px(Vec(60, 73)), module, RSModule::ENABLE_BISTABLE));

		// Potentiomètres de contrôle
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(33.92, 83.78)), module, RSModule::XB_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(46.92, 83.78)), module, RSModule::TAU_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(60.92, 83.78)), module, RSModule::L_PARAM));

		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(7.92, 58.78)), module, RSModule::TIME_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(21.92, 58.78)), module, RSModule::GAIN_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(7.92, 83.78)), module, RSModule::THRESHOLD_PARAM));

		// Entrées
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.92, 113.78)), module, RSModule::INPUT_NOISE));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(38.92, 113.78)), module, RSModule::INPUT_SIGNAL));

		// Sortie
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(65.92, 113.78)), module, RSModule::OUTPUT));
		
		// Voyants (LEDs)
		addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(3, 73)), module, RSModule::DIODE_LIGHT));
		addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(28, 73)), module, RSModule::BISTABLE_LIGHT));
	}
};

// Enregistrement du module auprès de VCV Rack
Model* modelRSModule = createModel<RSModule, RSModuleWidget>("RSModule");