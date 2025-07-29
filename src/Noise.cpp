#include "plugin.hpp"
#include <random>
#include <vector>
#include <cmath>
#include <cstdint>


// Générateur global pour le bruit blanc
static std::random_device rd;
static std::mt19937 gen(rd());
static std::normal_distribution<float> gauss(0.f, 1.f);

// Bruit blanc gaussien
float generateWhiteNoise(float /*sampleRate*/) {
    return gauss(gen);
}

// Bruit rouge (Brownian)
float generateRedNoise(float /*sampleRate*/) {
    static float last = 0.f;
    float white = gauss(gen) * 0.02f; // facteur pour éviter la dérive
    last += white;
    // Clamp pour éviter les débordements
    if (last > 5.f) last = 5.f;
    if (last < -5.f) last = -5.f;
    return last;
}

// Velvet noise (distribution impulsionnelle aléatoire)
float generateVelvetNoise(float sampleRate) {
    static float current = 0.f;
    static int count = 0;
    // Densité d'impulsions (ex: 1000/s)
    const float density = 1000.f;
    int interval = int(sampleRate / density);
    if (count++ >= interval) {
        count = 0;
        // Impulsion aléatoire +1 ou -1
        current = (gen() % 2 == 0) ? 1.f : -1.f;
    } else {
        current = 0.f;
    }
    return current;
}

// Bruit Perlin


class Perlin{
public:
    static constexpr int GRADIENT_SIZE = 65536;

    Perlin() {
        gradients.resize(GRADIENT_SIZE);
        std::mt19937 rng(std::random_device{}());
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

        for (int i = 0; i < GRADIENT_SIZE; ++i) {
            gradients[i] = dist(rng);
        }
    }

    float generateSampleAt(float time, float baseFreq, int octaves, float persistence, float lacunarity) const {
        float x = time * baseFreq;
        return fractalPerlin(x, octaves, persistence, lacunarity);
    }

private:
    std::vector<float> gradients;

    float fade(float t) const {
        return t * t * t * (t * (t * 6.f - 15.f) + 10.f);
    }

    float grad(float x) const {
        int idx = static_cast<int>(std::floor(x)) % GRADIENT_SIZE;
        if (idx < 0) idx += GRADIENT_SIZE;
        return gradients[idx];
    }

    float perlin(float x) const {
        float x0 = std::floor(x);
        float x1 = x0 + 1.0f;
        float t = x - x0;
        float fade_t = fade(t);
        float g0 = grad(x0);
        float g1 = grad(x1);
        float d0 = (x - x0) * g0;
        float d1 = (x - x1) * g1;
        return (1.0f - fade_t) * d0 + fade_t * d1;
    }

    float fractalPerlin(float x, int octaves, float persistence, float lacunarity) const {
        float total = 0.0f;
        float amplitude = 1.0f;
        float frequency = 1.0f;
        float maxAmplitude = 0.0f;

        for (int i = 0; i < octaves; ++i) {
            total += perlin(x * frequency) * amplitude;
            maxAmplitude += amplitude;
            amplitude *= persistence;
            frequency *= lacunarity;
        }

        return total / maxAmplitude;
    }
};



struct Noise : Module {
	Perlin perlinNoise = Perlin();
	float time = 0.f;


	enum ParamId {
		AMPL_PARAM,
		PERLIN_FREQ_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		INPUTS_LEN
	};
	enum OutputId {
		PERLIN,
		VELVET,
		WHITE,
		RED,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	Noise() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(AMPL_PARAM, 1.0f, 5.0f, 0.5f, "Amplitude");
		configParam(PERLIN_FREQ_PARAM, 0.1f, 300.0f, 10.0f, "Perlin Frequency");	
		configOutput(PERLIN, "Perlin Noise");
		configOutput(VELVET, "Velvet Noise");
		configOutput(WHITE, "White Noise");
		configOutput(RED, "Red Noise");	

	}


	void process(const ProcessArgs& args) override {

		float amplitude = params[AMPL_PARAM].getValue();
		float perlinFreq = params[PERLIN_FREQ_PARAM].getValue();

		time += args.sampleTime;

		// Generate noise signals
		float perlinNoise = generatePerlinNoise(perlinFreq, args.sampleRate) * amplitude;
		float velvetNoise = generateVelvetNoise(args.sampleRate) * amplitude;
		float whiteNoise = generateWhiteNoise(args.sampleRate) * amplitude;
		float redNoise = generateRedNoise(args.sampleRate) * amplitude;

        // Clamp the noise signals to avoid clipping
        if (perlinNoise > 5.f) perlinNoise = 5.f;
        if (perlinNoise < -5.f) perlinNoise = -5.f;
        if (velvetNoise > 5.f) velvetNoise = 5.f;
        if (velvetNoise < -5.f) velvetNoise = -5.f;
        if (whiteNoise > 5.f) whiteNoise = 5.f;
        if (whiteNoise < -5.f) whiteNoise = -5.f;
        if (redNoise > 5.f) redNoise = 5.f;
        if (redNoise < -5.f) redNoise = -5.f;

		// Output the noise signals
		outputs[PERLIN].setVoltage(perlinNoise);
		outputs[VELVET].setVoltage(velvetNoise);
		outputs[WHITE].setVoltage(whiteNoise);
		outputs[RED].setVoltage(redNoise);
	}

	float generatePerlinNoise(float frequency, float sampleRate) {
		float time = this->time; // Vous pouvez ajuster le temps selon vos besoins
		return perlinNoise.generateSampleAt(time, frequency, 5, 0.5f, 2.0f);
	}

	void onReset() override {
		time = 0.f;
		perlinNoise = Perlin(); // Réinitialiser le générateur de bruit Perlin
	}
};


struct NoiseWidget : ModuleWidget {
	NoiseWidget(Noise* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Noise.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));


		addParam(createParamCentered<RoundHugeBlackKnob>(mm2px(Vec(25.616, 21.795)), module, Noise::AMPL_PARAM));
        addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(11.1125, 48.8155)), module, Noise::PERLIN_FREQ_PARAM));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(25.4005, 48.5515)), module, Noise::PERLIN));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(25.4005, 68.6595)), module, Noise::VELVET));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(25.4005, 86.1215)), module, Noise::WHITE));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(25.4005, 108.3475)), module, Noise::RED));
	}
};


Model* modelNoise = createModel<Noise, NoiseWidget>("Noise");