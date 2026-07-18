#include "guitarfx/AsyncAssetLoader.h"
#include "guitarfx/WavReader.h"
#include "NAM/get_dsp.h"
#include "NAM/dsp.h"
#include <chrono>
#include <filesystem>

AsyncAssetLoader::AsyncAssetLoader() {
}

AsyncAssetLoader::~AsyncAssetLoader() {
    stop();
}

void AsyncAssetLoader::start() {
    mShouldExit = false;
    mThread = std::thread(&AsyncAssetLoader::run, this);
}

void AsyncAssetLoader::stop() {
    mShouldExit = true;
    mCV.notify_all();
    if (mThread.joinable()) {
        mThread.join();
    }
}

void AsyncAssetLoader::requestModelLoad(const std::string& filepath, double sampleRate,
                                       std::function<void(std::unique_ptr<nam::DSP>)> callback) {
    std::lock_guard<std::mutex> lock(mMutex);
    LoadRequest req;
    req.type = LoadRequest::Type::NAM_Model;
    req.filepath = filepath;
    req.sampleRate = sampleRate;
    req.modelCallback = callback;
    mQueue.push(req);
    mCV.notify_all();
}

void AsyncAssetLoader::requestIRLoad(const std::string& filepath, double sampleRate,
                                     std::function<void(std::vector<float>)> callback) {
    std::lock_guard<std::mutex> lock(mMutex);
    LoadRequest req;
    req.type = LoadRequest::Type::Cabinet_IR;
    req.filepath = filepath;
    req.sampleRate = sampleRate;
    req.irCallback = callback;
    mQueue.push(req);
    mCV.notify_all();
}

void AsyncAssetLoader::decommissionModel(nam::DSP* model) {
    if (model != nullptr) {
        mModelTrash.push(model);
    }
}

void AsyncAssetLoader::decommissionIRCoefficients(std::vector<float>* ir) {
    if (ir != nullptr) {
        mIRTrash.push(ir);
    }
}

template<typename T>
void cleanTrash(LockFreePointerQueue& queue) {
    while (void* ptr = queue.pop()) {
        T* typedPtr = static_cast<T*>(ptr);
        delete typedPtr;
    }
}

void AsyncAssetLoader::run() {
    while (!mShouldExit) {
        LoadRequest req;
        bool hasRequest = false;

        {
            std::unique_lock<std::mutex> lock(mMutex);
            mCV.wait_for(lock, std::chrono::milliseconds(10), [this] {
                return !mQueue.empty() || mShouldExit.load();
            });

            if (mShouldExit) {
                break;
            }

            if (!mQueue.empty()) {
                req = mQueue.front();
                mQueue.pop();
                hasRequest = true;
            }
        }

        // Clean trash on background thread
        cleanTrash<nam::DSP>(mModelTrash);
        cleanTrash<std::vector<float>>(mIRTrash);

        if (hasRequest) {
            try {
                if (req.type == LoadRequest::Type::NAM_Model) {
                    // Load the model from filesystem path
                    auto model = nam::get_dsp(std::filesystem::path(req.filepath));
                    if (model != nullptr) {
                        // Pre-warm/Reset the model to prepare it for processing
                        model->Reset(req.sampleRate, 512);
                        
                        // Warm with a small block of silence
                        std::vector<float> warmInput(512, 0.0f);
                        std::vector<float> warmOutput(512, 0.0f);
                        float* inputPtrs[1] = { warmInput.data() };
                        float* outputPtrs[1] = { warmOutput.data() };
                        model->process(inputPtrs, outputPtrs, 512);

                        if (req.modelCallback) {
                            req.modelCallback(std::move(model));
                        }
                    }
                } else if (req.type == LoadRequest::Type::Cabinet_IR) {
                    auto ir = WavReader::loadAndResample(req.filepath, req.sampleRate);
                    if (!ir.empty()) {
                        if (req.irCallback) {
                            req.irCallback(std::move(ir));
                        }
                    }
                }
            } catch (...) {
                // Ignore load failures (e.g. invalid file format)
            }
        }
    }

    cleanTrash<nam::DSP>(mModelTrash);
    cleanTrash<std::vector<float>>(mIRTrash);
}
