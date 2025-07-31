#include "plugin.hpp"


struct Compressor : Module {

	float signal = 0.f;
	float gain = 0.1f;
	float ratio = 40.f;
	float threshold = 0.5f;  // 

	enum ParamId {
		AMPL_PARAM,
		RATO_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	Compressor() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(AMPL_PARAM, 0.f, 10.f, 0.1f, "Amplitude");
		configParam(RATO_PARAM, 0.f, 100.f, 40.f, "Ratio");
		configInput(INPUT, "Input");
		configOutput(OUTPUT, "Output");
	}

	void process(const ProcessArgs& args) override {
		signal = inputs[INPUT].getVoltage();
		gain = params[AMPL_PARAM].getValue();
		ratio = params[RATO_PARAM].getValue();

		// Simple compression algorithm

		if(fabs(signal) > threshold) {
			signal = threshold + (signal - threshold) / (ratio/10.f);
		}
 

		outputs[OUTPUT].setVoltage(signal * gain);
	}
};


struct CompressorWidget : ModuleWidget {
	CompressorWidget(Compressor* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Compressor.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(25.3995, 30.4665)), module, Compressor::AMPL_PARAM));
		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(25.3995, 68.4665)), module, Compressor::RATO_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.9685, 103.9685)), module, Compressor::INPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(38.9685, 103.9685)), module, Compressor::OUTPUT));

	}

	
};


Model* modelCompressor = createModel<Compressor, CompressorWidget>("Compressor");