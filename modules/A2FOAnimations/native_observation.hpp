#pragma once
#include <windows.h>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace a2fo::animations {
// Private POD boundary. This proves only native fallback when no custom clip
// classes exist; it never samples a clip, invokes a gateway or mutates playback.
struct NativePlaybackProof {
    HMODULE image=nullptr;
    void* target=nullptr;
    void* replacement=nullptr;
    void* gateway=nullptr;
    std::size_t length=0;
    std::uint64_t epoch=0;
};
using NativePlaybackSource=bool(*)(HMODULE,NativePlaybackProof&) noexcept;

class NativePlaybackCoverage {
    SRWLOCK lock_=SRWLOCK_INIT;
    NativePlaybackProof proof_{};
    std::uint64_t epoch_=1;
    std::uint32_t updates_=0,evaluators_=0;
    bool ready_=false,custom_=false,failed_=false;
    void tick() noexcept {
        if(epoch_==std::numeric_limits<std::uint64_t>::max())failed_=true;
        else ++epoch_;
    }
    bool begin(std::uint32_t& count) noexcept {
        AcquireSRWLockExclusive(&lock_);tick();
        const bool okay=!failed_&&count!=std::numeric_limits<std::uint32_t>::max();
        if(okay)++count;else failed_=true;
        ReleaseSRWLockExclusive(&lock_);return okay;
    }
public:
    NativePlaybackCoverage()=default;
    NativePlaybackCoverage(const NativePlaybackCoverage&)=delete;
    NativePlaybackCoverage& operator=(const NativePlaybackCoverage&)=delete;
    void configure(bool ready,const NativePlaybackProof& proof) noexcept {
        AcquireSRWLockExclusive(&lock_);tick();
        ready_=ready;proof_=proof;
        ReleaseSRWLockExclusive(&lock_);
    }
    bool begin_class_update() noexcept {return begin(updates_);}
    void end_class_update(bool custom) noexcept {
        AcquireSRWLockExclusive(&lock_);tick();custom_=custom;
        if(updates_)--updates_;else failed_=true;
        ReleaseSRWLockExclusive(&lock_);
    }
    bool begin_evaluate() noexcept {return begin(evaluators_);}
    void end_evaluate() noexcept {
        AcquireSRWLockExclusive(&lock_);tick();
        if(evaluators_)--evaluators_;else failed_=true;
        ReleaseSRWLockExclusive(&lock_);
    }
    bool snapshot(HMODULE image,NativePlaybackProof& output) noexcept {
        AcquireSRWLockShared(&lock_);
        const bool okay=image&&ready_&&!custom_&&!failed_&&!updates_&&!evaluators_&&
            proof_.image==image&&proof_.target&&proof_.replacement&&proof_.gateway&&proof_.length;
        if(okay){output=proof_;output.epoch=epoch_;}
        ReleaseSRWLockShared(&lock_);return okay;
    }
};
class NativeEvaluationScope {
    NativePlaybackCoverage& coverage_;bool entered_;
public:
    explicit NativeEvaluationScope(NativePlaybackCoverage& coverage) noexcept:
        coverage_(coverage),entered_(coverage.begin_evaluate()){}
    NativeEvaluationScope(const NativeEvaluationScope&)=delete;
    NativeEvaluationScope& operator=(const NativeEvaluationScope&)=delete;
    ~NativeEvaluationScope(){if(entered_)coverage_.end_evaluate();}
};
}
