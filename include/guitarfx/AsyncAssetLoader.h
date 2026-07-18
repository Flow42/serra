#pragma once
#include <string>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <memory>
#include <vector>

namespace nam {
    class DSP;
}

// A real-time safe lock-free Single-Producer Single-Consumer queue for pointers
class LockFreePointerQueue {
public:
    LockFreePointerQueue() {
        for (int i = 0; i < CAPACITY; ++i) {
            mBuffer[i].store(nullptr, std::memory_order_relaxed);
        }
    }

    bool push(void* ptr) {
        int w = mWriteIdx.load(std::memory_order_relaxed);
        int r = mReadIdx.load(std::memory_order_acquire);
        if (w - r >= CAPACITY) {
            return false; // Queue full (should never happen in our scenario)
        }
        mBuffer[w % CAPACITY].store(ptr, std::memory_order_release);
        mWriteIdx.store(w + 1, std::memory_order_release);
        return true;
    }

    void* pop() {
        int r = mReadIdx.load(std::memory_order_relaxed);
        int w = mWriteIdx.load(std::memory_order_acquire);
        if (r == w) {
            return nullptr; // Empty
        }
        void* ptr = mBuffer[r % CAPACITY].load(std::memory_order_acquire);
        mReadIdx.store(r + 1, std::memory_order_release);
        return ptr;
    }

private:
    static constexpr int CAPACITY = 32;
    std::atomic<void*> mBuffer[CAPACITY];
    std::atomic<int> mWriteIdx{0};
    std::atomic<int> mReadIdx{0};
};

class AsyncAssetLoader {
public:
    AsyncAssetLoader();
    ~AsyncAssetLoader();

    void start();
    void stop();

    // Request asynchronous load of a NAM model.
    void requestModelLoad(const std::string& filepath, double sampleRate,
                          std::function<void(std::unique_ptr<nam::DSP>)> callback);

    // Request asynchronous load of an IR WAV file.
    void requestIRLoad(const std::string& filepath, double sampleRate,
                       std::function<void(std::vector<float>)> callback);

    // Queue pointers for deletion on the background thread (real-time safe).
    void decommissionModel(nam::DSP* model);
    void decommissionIRCoefficients(std::vector<float>* ir);

private:
    void run();

    struct LoadRequest {
        enum class Type { NAM_Model, Cabinet_IR };
        Type type;
        std::string filepath;
        double sampleRate;
        std::function<void(std::unique_ptr<nam::DSP>)> modelCallback;
        std::function<void(std::vector<float>)> irCallback;
    };

    std::thread mThread;
    std::atomic<bool> mShouldExit{false};

    std::mutex mMutex;
    std::condition_variable mCV;
    std::queue<LoadRequest> mQueue;

    // Lock-free queues for decommissioned resources
    LockFreePointerQueue mModelTrash;
    LockFreePointerQueue mIRTrash;
};
