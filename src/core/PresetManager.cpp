#include "guitarfx/PresetManager.h"
#include "guitarfx/DSPEngine.h"
#include "guitarfx/Processor.h"
#include "nlohmann/json.hpp"
#include <iostream>

using json = nlohmann::json;

std::string PresetManager::savePreset(const DSPEngine& engine) {
    json root;
    root["version"] = 1;
    
    json processors = json::array();
    const auto& chain = engine.getChain();
    for (const auto& proc : chain) {
        json item;
        item["id"] = proc->getIdentifier();
        item["bypassed"] = proc->isBypassed();
        
        // Save parameters
        json params = json::object();
        for (int i = 0; i < proc->getNumParameters(); ++i) {
            params[proc->getParameterName(i)] = proc->getParameter(i);
        }
        item["params"] = params;
        
        // Save custom settings
        if (proc->getIdentifier() == "nam") {
            item["model_path"] = proc->getSetting("model_path");
        } else if (proc->getIdentifier() == "cabinet") {
            item["ir_path"] = proc->getSetting("ir_path");
        }
        
        processors.push_back(item);
    }
    root["processors"] = processors;
    
    // Save OutputProcessor settings
    json outputProc = json::object();
    const auto& out = engine.getOutputProcessor();
    outputProc["bypassed"] = out.isBypassed();
    json outParams = json::object();
    for (int i = 0; i < out.getNumParameters(); ++i) {
        outParams[out.getParameterName(i)] = out.getParameter(i);
    }
    outputProc["params"] = outParams;
    root["output_processor"] = outputProc;

    return root.dump(4);
}

bool PresetManager::loadPreset(DSPEngine& engine, const std::string& presetJson) {
    try {
        json root = json::parse(presetJson);
        if (!root.contains("version") || root["version"] != 1) {
            return false;
        }
        
        std::vector<std::string> order;
        if (root.contains("processors") && root["processors"].is_array()) {
            for (const auto& item : root["processors"]) {
                std::string id = item["id"];
                order.push_back(id);
                
                auto proc = engine.getProcessor(id);
                if (proc) {
                    proc->setBypass(item["bypassed"]);
                    if (item.contains("params") && item["params"].is_object()) {
                        for (auto& [name, val] : item["params"].items()) {
                            for (int i = 0; i < proc->getNumParameters(); ++i) {
                                if (proc->getParameterName(i) == name) {
                                    proc->setParameter(i, val.get<float>());
                                    break;
                                }
                            }
                        }
                    }
                    if (id == "nam" && item.contains("model_path")) {
                        proc->setSetting("model_path", item["model_path"].get<std::string>());
                    } else if (id == "cabinet" && item.contains("ir_path")) {
                        proc->setSetting("ir_path", item["ir_path"].get<std::string>());
                    }
                }
            }
            
            // Reorder the chain in DSPEngine
            engine.reorderChain(order);
        }
        
        // Load OutputProcessor
        if (root.contains("output_processor") && root["output_processor"].is_object()) {
            auto proc = engine.getProcessor("output");
            if (proc) {
                const auto& item = root["output_processor"];
                proc->setBypass(item["bypassed"]);
                if (item.contains("params") && item["params"].is_object()) {
                    for (auto& [name, val] : item["params"].items()) {
                        for (int i = 0; i < proc->getNumParameters(); ++i) {
                            if (proc->getParameterName(i) == name) {
                                proc->setParameter(i, val.get<float>());
                                break;
                            }
                        }
                    }
                }
            }
        }
        
        return true;
    } catch (...) {
        return false;
    }
}
