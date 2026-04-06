#include "prism/processor.h"

#ifdef PRISM_STANDALONE

#include <spdlog/spdlog.h>
#include <fstream>

enum {
    SHADER_0,
    SHADER_INPUT_1,
    SHADER_INPUT_2,
    SHADER_INPUT_3,
    SHADER_INPUT_4,
    SHADER_INPUT_5,
    SHADER_INPUT_6,
    SHADER_INPUT_7,
    SHADER_TEXEL0,
    SHADER_TEXEL0A,
    SHADER_TEXEL1,
    SHADER_TEXEL1A,
    SHADER_1,
    SHADER_COMBINED,
    SHADER_NOISE
};

extern "C" prism::ContextTypes* add_text(prism::ContextItems* _, prism::ContextTypes* arg1, prism::ContextTypes* arg2, prism::ContextTypes* arg3) {
    std::string items = "";
    for (int i = 0; i < 3; i++) {
        items += "add";
    }
    return new prism::ContextTypes{ items };
}

#define RAND_NOISE "((random(vec3(floor(gl_FragCoord.xy * noise_scale), float(frame_count))) + 1.0) / 2.0)"

static const char* shader_item_to_str(uint32_t item, bool with_alpha, bool only_alpha, bool inputs_have_alpha,
                                      bool first_cycle, bool hint_single_element) {
    if (!only_alpha) {
        switch (item) {
            case SHADER_0:
                return with_alpha ? "vec4(0.0, 0.0, 0.0, 0.0)" : "vec3(0.0, 0.0, 0.0)";
            case SHADER_1:
                return with_alpha ? "vec4(1.0, 1.0, 1.0, 1.0)" : "vec3(1.0, 1.0, 1.0)";
            case SHADER_INPUT_1:
                return with_alpha || !inputs_have_alpha ? "vInput1" : "vInput1.rgb";
            case SHADER_INPUT_2:
                return with_alpha || !inputs_have_alpha ? "vInput2" : "vInput2.rgb";
            case SHADER_INPUT_3:
                return with_alpha || !inputs_have_alpha ? "vInput3" : "vInput3.rgb";
            case SHADER_INPUT_4:
                return with_alpha || !inputs_have_alpha ? "vInput4" : "vInput4.rgb";
            case SHADER_TEXEL0:
                return first_cycle ? (with_alpha ? "texVal0" : "texVal0.rgb")
                                   : (with_alpha ? "texVal1" : "texVal1.rgb");
            case SHADER_TEXEL0A:
                return first_cycle
                           ? (hint_single_element ? "texVal0.a"
                                                  : (with_alpha ? "vec4(texVal0.a, texVal0.a, texVal0.a, texVal0.a)"
                                                                : "vec3(texVal0.a, texVal0.a, texVal0.a)"))
                           : (hint_single_element ? "texVal1.a"
                                                  : (with_alpha ? "vec4(texVal1.a, texVal1.a, texVal1.a, texVal1.a)"
                                                                : "vec3(texVal1.a, texVal1.a, texVal1.a)"));
            case SHADER_TEXEL1A:
                return first_cycle
                           ? (hint_single_element ? "texVal1.a"
                                                  : (with_alpha ? "vec4(texVal1.a, texVal1.a, texVal1.a, texVal1.a)"
                                                                : "vec3(texVal1.a, texVal1.a, texVal1.a)"))
                           : (hint_single_element ? "texVal0.a"
                                                  : (with_alpha ? "vec4(texVal0.a, texVal0.a, texVal0.a, texVal0.a)"
                                                                : "vec3(texVal0.a, texVal0.a, texVal0.a)"));
            case SHADER_TEXEL1:
                return first_cycle ? (with_alpha ? "texVal1" : "texVal1.rgb")
                                   : (with_alpha ? "texVal0" : "texVal0.rgb");
            case SHADER_COMBINED:
                return with_alpha ? "texel" : "texel.rgb";
            case SHADER_NOISE:
                return with_alpha ? "vec4(" RAND_NOISE ", " RAND_NOISE ", " RAND_NOISE ", " RAND_NOISE ")"
                                  : "vec3(" RAND_NOISE ", " RAND_NOISE ", " RAND_NOISE ")";
        }
    } else {
        switch (item) {
            case SHADER_0:
                return "0.0";
            case SHADER_1:
                return "1.0";
            case SHADER_INPUT_1:
                return "vInput1.a";
            case SHADER_INPUT_2:
                return "vInput2.a";
            case SHADER_INPUT_3:
                return "vInput3.a";
            case SHADER_INPUT_4:
                return "vInput4.a";
            case SHADER_TEXEL0:
                return first_cycle ? "texVal0.a" : "texVal1.a";
            case SHADER_TEXEL0A:
                return first_cycle ? "texVal0.a" : "texVal1.a";
            case SHADER_TEXEL1A:
                return first_cycle ? "texVal1.a" : "texVal0.a";
            case SHADER_TEXEL1:
                return first_cycle ? "texVal1.a" : "texVal0.a";
            case SHADER_COMBINED:
                return "texel.a";
            case SHADER_NOISE:
                return RAND_NOISE;
        }
    }
    return "";
}

bool get_bool(prism::ContextTypes* value) {
    if (std::holds_alternative<int>(*value)) {
        return std::get<int>(*value) == 1;
    }
    return false;
}

extern "C" prism::ContextTypes* append_formula(prism::ContextItems* items, prism::ContextTypes* a_arg, prism::ContextTypes* a_single,
                                    prism::ContextTypes* a_mult, prism::ContextTypes* a_mix,
                                    prism::ContextTypes* a_with_alpha, prism::ContextTypes* a_only_alpha,
                                    prism::ContextTypes* a_alpha, prism::ContextTypes* a_first_cycle) {
    if (!items->contains("local_var")) {
        items->insert({"local_var", prism::ContextTypes{0}});
    }
    // increase local_var by 1
    auto& local_var = std::get<int>(items->at("local_var"));
    local_var++;
    // uint8_t c[2][4] =
    auto c = std::get<prism::MTDArray<int>>(*a_arg);
    bool do_single = get_bool(a_single);
    bool do_multiply = get_bool(a_mult);
    bool do_mix = get_bool(a_mix);
    bool with_alpha = get_bool(a_with_alpha);
    bool only_alpha = get_bool(a_only_alpha);
    bool opt_alpha = get_bool(a_alpha);
    bool first_cycle = get_bool(a_first_cycle);
    std::string out = "";
    if (do_single) {
        out += shader_item_to_str(c.at(only_alpha, 3), with_alpha, only_alpha, opt_alpha, first_cycle, false);
    } else if (do_multiply) {
        out += shader_item_to_str(c.at(only_alpha, 0), with_alpha, only_alpha, opt_alpha, first_cycle, false);
        out += " * ";
        out += shader_item_to_str(c.at(only_alpha, 2), with_alpha, only_alpha, opt_alpha, first_cycle, true);
    } else if (do_mix) {
        out += "mix(";
        out += shader_item_to_str(c.at(only_alpha, 1), with_alpha, only_alpha, opt_alpha, first_cycle, false);
        out += ", ";
        out += shader_item_to_str(c.at(only_alpha, 0), with_alpha, only_alpha, opt_alpha, first_cycle, false);
        out += ", ";
        out += shader_item_to_str(c.at(only_alpha, 2), with_alpha, only_alpha, opt_alpha, first_cycle, true);
        out += ")";
    } else {
        out += "(";
        out += shader_item_to_str(c.at(only_alpha, 0), with_alpha, only_alpha, opt_alpha, first_cycle, false);
        out += " - ";
        out += shader_item_to_str(c.at(only_alpha, 1), with_alpha, only_alpha, opt_alpha, first_cycle, false);
        out += ") * ";
        out += shader_item_to_str(c.at(only_alpha, 2), with_alpha, only_alpha, opt_alpha, first_cycle, true);
        out += " + ";
        out += shader_item_to_str(c.at(only_alpha, 3), with_alpha, only_alpha, opt_alpha, first_cycle, false);
    }
    return new prism::ContextTypes{ out };
}

std::optional<std::string> include_fs(const std::string& path){
    std::ifstream input(path);
    if (!input.is_open()) {
        SPDLOG_ERROR("Failed to open file: {}", path);
        return std::nullopt;
    }

    std::vector<uint8_t> data = std::vector<uint8_t>(std::istreambuf_iterator(input), {});
    input.close();

    return std::string(data.begin(), data.end());
}

std::string to_string(const prism::ContextTypes& value) {
    if (std::holds_alternative<int>(value)) {
        return std::to_string(std::get<int>(value));
    } else if (std::holds_alternative<float>(value)) {
        return std::to_string(std::get<float>(value));
    } else if (std::holds_alternative<std::string>(value)) {
        return std::get<std::string>(value);
    } else if (std::holds_alternative<prism::MTDArray<bool>>(value)) {
        return "MTDArray<bool>";
    } else if (std::holds_alternative<prism::MTDArray<int>>(value)) {
        return "MTDArray<int>";
    } else if (std::holds_alternative<prism::MTDArray<float>>(value)) {
        return "MTDArray<float>";
    }
    return "Unknown type";
}

int main(int argc, char** argv) {
    if (argc < 2) {
        SPDLOG_ERROR("Usage: {} <file>", argv[0]);
        return 1;
    }

    std::ifstream input(argv[1]);
    if (!input.is_open()) {
        SPDLOG_ERROR("Failed to open file: {}", argv[1]);
        return 1;
    }

    std::vector<uint8_t> data = std::vector<uint8_t>(std::istreambuf_iterator(input), {});
    input.close();

    int o_textures[2] = { 1, 1 };
    int o_clamp[2][2] = { { 1, 0 }, { 0, 1 } };
    float o_float[] = { 1.1f, 2.2f, 3.3f, 4.4f, 5.5f, 6.6f };
    int o_masks[2] = { 1, 2 };
    int o_blend[2] = { 1, 2 };
    int o_c[2][2][4] = { { { 1, 2, 3, 4 }, { 5, 6, 7, 8 } }, { { 9, 10, 11, 12 }, { 13, 14, 15, 16 } } };
    int o_color_alpha_same[3] = { 0, 0, 0 };
    int o_do_single[2][2] = { { 1, 2 }, { 3, 4 } };
    int o_do_multiply[2][2] = { { 1, 2 }, { 3, 4 } };
    int o_do_mix[2][2] = { { 1, 2 }, { 3, 4 } };

    auto test = M_ARRAY(o_c, int, 2, 2, 4);
    for(int i = 0; i < 2; i++) {
        for(int j = 0; j < 2; j++) {
            for(int k = 0; k < 4; k++) {
                assert(test.at(i, j, k) == o_c[i][j][k] || "Array access failed");
            }
        }
    }

    for(int i = 0; i < 2; i++){
        for(int j = 0; j < 2; j++){
            for(int k = 0; k < 4; k++){
                assert(test.get(i, j).at(k) == o_c[i][j][k] || "Array access failed");
            }
        }
        assert(test.get(i).at(0, 0) == o_c[i][0][0] || "Array access failed");
    }

    uintptr_t ptr = (uintptr_t)&o_c;
    uintptr_t ptr2 = (uintptr_t)test.ptr;
    for (int i = 0; i < 2; i++) {
        ptr = (uintptr_t)&o_c[i];
        ptr2 = (uintptr_t)test.get(i).ptr;
        assert(ptr == ptr2);
        auto c2 = test.get(i);
        for (int j = 0; j < 2; j++) {
            ptr = (uintptr_t)&o_c[i][j];
            ptr2 = (uintptr_t)c2.get(j).ptr;
            assert(ptr == ptr2);
            auto c3 = c2.get(j);
            for (int k = 0; k < 4; k++) {
                ptr = (uintptr_t)&o_c[i][j][k];
                ptr2 = (uintptr_t)&c3.at(k);
                assert(ptr == ptr2);
            }
        }
    }

    prism::ContextItems vars = {
        { "GLSL_VERSION", "#version 410 core" },
        { "attr", "in" },
        { "o_textures", M_ARRAY(o_textures, int, 2) },
        { "o_clamp", M_ARRAY(o_clamp, int, 2, 2) },
        { "o_float", M_ARRAY(o_float, float, 6) },
        { "o_fog", true },
        { "o_grayscale", true },
        { "o_noise", true },
        { "o_inputs", 4 },
        { "o_alpha", true },
        { "o_masks", M_ARRAY(o_masks, int, 2) },
        { "o_blend", M_ARRAY(o_blend, int, 2) },
        { "o_three_point_filter", true },
        { "o_2cyc", true },
        { "o_alpha_threshold", true },
        { "o_texture_edge", true },
        { "o_invisible", true },
        { "srgb_mode", true },
        { "core_opengl", false },
        { "opengles", false },
        { "vOutColor", "gl_Position" },
        { "o_current_filter", 0 },
        { "o_c", M_ARRAY(o_c, int, 2, 2, 4) },
        { "add_text", (InvokeFunc) add_text },
        { "o_color_alpha_same", M_ARRAY(o_color_alpha_same, int, 3) },
        { "FILTER_THREE_POINT", 3 },
        { "SHADER_0", SHADER_0 },
        { "SHADER_INPUT_1", SHADER_INPUT_1 },
        { "SHADER_INPUT_2", SHADER_INPUT_2 },
        { "SHADER_INPUT_3", SHADER_INPUT_3 },
        { "SHADER_INPUT_4", SHADER_INPUT_4 },
        { "SHADER_INPUT_5", SHADER_INPUT_5 },
        { "SHADER_INPUT_6", SHADER_INPUT_6 },
        { "SHADER_INPUT_7", SHADER_INPUT_7 },
        { "SHADER_TEXEL0", SHADER_TEXEL0 },
        { "SHADER_TEXEL0A", SHADER_TEXEL0A },
        { "SHADER_TEXEL1", SHADER_TEXEL1 },
        { "SHADER_TEXEL1A", SHADER_TEXEL1A },
        { "SHADER_1", SHADER_1 },
        { "SHADER_COMBINED", SHADER_COMBINED },
        { "SHADER_NOISE", SHADER_NOISE },
        { "append_formula", (InvokeFunc) append_formula },
        { "o_do_single", M_ARRAY(o_do_single, int, 2, 2) },
        { "o_do_multiply", M_ARRAY(o_do_multiply, int, 2, 2) },
        { "o_do_mix", M_ARRAY(o_do_mix, int, 2, 2) },
        { "texture", "texture2d" }
    };

    prism::Processor processor;
    processor.bind_include_loader(include_fs);
    processor.populate(vars);
    processor.load(std::string(data.begin(), data.end()));
    SPDLOG_INFO("Processed data: \n{}", processor.process());
    for (const auto& item : processor.getTypes()) {
        SPDLOG_INFO("{}: {}", item.first, to_string(item.second));
    }
    return 0;
}
#endif