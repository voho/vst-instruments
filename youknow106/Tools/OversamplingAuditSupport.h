// Compile-time work-accounting support for the oversampling research tool.
//
// Shipping DSP never defines YOUKNOW106_WORK_AUDIT, so it neither includes
// this header nor pays for a counter branch.  The dedicated audit library
// points the thread-local sink at one stack-owned aggregate while it renders a
// deterministic window.  A null sink leaves pre-roll and unrelated renders
// unobserved; the parity fingerprint deliberately installs a live sink so it
// guards against active observation altering the signal.
#pragma once

#include <array>
#include <cstdint>
#include <string_view>
#include <utility>

namespace youknow106::oversampling_audit
{

struct DomainWorkCounters
{
    std::uint64_t hostFrames {};
    std::uint64_t internalFrames {};
    std::uint64_t scanPolls {};
    std::uint64_t converterPassStarts {};
    std::uint64_t converterWrites {};
    std::uint64_t extensionScanUpdates {};
    std::uint64_t holdVoiceUpdates {};
    std::uint64_t pulseComparatorUpdates {};
    std::uint64_t voiceAudioUpdates {};
    std::uint64_t cutoffMemoHits {};
    std::uint64_t cutoffMemoMisses {};
    std::uint64_t dcoFrames {};
    std::uint64_t dcoCycleWraps {};
    std::uint64_t dcoComparatorTransitions {};
    std::uint64_t dcoSubTransitions {};
    std::uint64_t vcfSteps {};
    std::uint64_t vcfIntegrationSubsteps {};
    std::uint64_t vcfRhsEvaluations {};
    std::uint64_t vcfStageEvaluations {};
    std::uint64_t vcfFeedbackEvaluations {};
    std::uint64_t vcfEarlyEvaluations {};
    std::uint64_t vcfInputReconstructions {};
    std::uint64_t vcfFractionalEventPeeks {};
    std::uint64_t vcfFractionalTargetCommits {};
    std::uint64_t vcfExactControlIntervals {};
    std::uint64_t vcfExactControlNodes {};
    std::uint64_t vcfExactControlMaps {};
    std::uint64_t vcfRecoveries {};
    std::uint64_t chorusFrames {};
    std::uint64_t bbdLineFrames {};
    std::uint64_t bbdLegacyInputSupportFrames {};
    std::uint64_t bbdExactInputSupportAdvances {};
    std::uint64_t bbdExactOutputSupportAdvances {};
    std::uint64_t bbdExactSupportCoordinateUpdates {};
    std::uint64_t bbdExactSupportMacs {};
    std::uint64_t bbdShifts {};
    std::uint64_t blepPastCorrectionVisits {};
    std::uint64_t blepFuturePredictionVisits {};
    std::uint64_t decimatorCalls {};
    std::uint64_t decimatorNonzeroTapVisits {};
    std::uint64_t decimatorStereoMacs {};
};

struct CounterDescriptor
{
    std::string_view name;
    std::uint64_t DomainWorkCounters::* member;
};

inline constexpr std::array counterDescriptors {
    CounterDescriptor { "hostFrames", &DomainWorkCounters::hostFrames },
    CounterDescriptor { "internalFrames", &DomainWorkCounters::internalFrames },
    CounterDescriptor { "scanPolls", &DomainWorkCounters::scanPolls },
    CounterDescriptor { "converterPassStarts", &DomainWorkCounters::converterPassStarts },
    CounterDescriptor { "converterWrites", &DomainWorkCounters::converterWrites },
    CounterDescriptor { "extensionScanUpdates", &DomainWorkCounters::extensionScanUpdates },
    CounterDescriptor { "holdVoiceUpdates", &DomainWorkCounters::holdVoiceUpdates },
    CounterDescriptor { "pulseComparatorUpdates", &DomainWorkCounters::pulseComparatorUpdates },
    CounterDescriptor { "voiceAudioUpdates", &DomainWorkCounters::voiceAudioUpdates },
    CounterDescriptor { "cutoffMemoHits", &DomainWorkCounters::cutoffMemoHits },
    CounterDescriptor { "cutoffMemoMisses", &DomainWorkCounters::cutoffMemoMisses },
    CounterDescriptor { "dcoFrames", &DomainWorkCounters::dcoFrames },
    CounterDescriptor { "dcoCycleWraps", &DomainWorkCounters::dcoCycleWraps },
    CounterDescriptor { "dcoComparatorTransitions", &DomainWorkCounters::dcoComparatorTransitions },
    CounterDescriptor { "dcoSubTransitions", &DomainWorkCounters::dcoSubTransitions },
    CounterDescriptor { "vcfSteps", &DomainWorkCounters::vcfSteps },
    CounterDescriptor { "vcfIntegrationSubsteps", &DomainWorkCounters::vcfIntegrationSubsteps },
    CounterDescriptor { "vcfRhsEvaluations", &DomainWorkCounters::vcfRhsEvaluations },
    CounterDescriptor { "vcfStageEvaluations", &DomainWorkCounters::vcfStageEvaluations },
    CounterDescriptor { "vcfFeedbackEvaluations", &DomainWorkCounters::vcfFeedbackEvaluations },
    CounterDescriptor { "vcfEarlyEvaluations", &DomainWorkCounters::vcfEarlyEvaluations },
    CounterDescriptor { "vcfInputReconstructions", &DomainWorkCounters::vcfInputReconstructions },
    CounterDescriptor { "vcfFractionalEventPeeks", &DomainWorkCounters::vcfFractionalEventPeeks },
    CounterDescriptor { "vcfFractionalTargetCommits", &DomainWorkCounters::vcfFractionalTargetCommits },
    CounterDescriptor { "vcfExactControlIntervals", &DomainWorkCounters::vcfExactControlIntervals },
    CounterDescriptor { "vcfExactControlNodes", &DomainWorkCounters::vcfExactControlNodes },
    CounterDescriptor { "vcfExactControlMaps", &DomainWorkCounters::vcfExactControlMaps },
    CounterDescriptor { "vcfRecoveries", &DomainWorkCounters::vcfRecoveries },
    CounterDescriptor { "chorusFrames", &DomainWorkCounters::chorusFrames },
    CounterDescriptor { "bbdLineFrames", &DomainWorkCounters::bbdLineFrames },
    CounterDescriptor { "bbdLegacyInputSupportFrames", &DomainWorkCounters::bbdLegacyInputSupportFrames },
    CounterDescriptor { "bbdExactInputSupportAdvances", &DomainWorkCounters::bbdExactInputSupportAdvances },
    CounterDescriptor { "bbdExactOutputSupportAdvances", &DomainWorkCounters::bbdExactOutputSupportAdvances },
    CounterDescriptor { "bbdExactSupportCoordinateUpdates", &DomainWorkCounters::bbdExactSupportCoordinateUpdates },
    CounterDescriptor { "bbdExactSupportMacs", &DomainWorkCounters::bbdExactSupportMacs },
    CounterDescriptor { "bbdShifts", &DomainWorkCounters::bbdShifts },
    CounterDescriptor { "blepPastCorrectionVisits", &DomainWorkCounters::blepPastCorrectionVisits },
    CounterDescriptor { "blepFuturePredictionVisits", &DomainWorkCounters::blepFuturePredictionVisits },
    CounterDescriptor { "decimatorCalls", &DomainWorkCounters::decimatorCalls },
    CounterDescriptor { "decimatorNonzeroTapVisits", &DomainWorkCounters::decimatorNonzeroTapVisits },
    CounterDescriptor { "decimatorStereoMacs", &DomainWorkCounters::decimatorStereoMacs },
};

inline thread_local DomainWorkCounters* activeDomainWorkCounters = nullptr;

class ScopedDomainWorkCounterSink
{
public:
    explicit ScopedDomainWorkCounterSink(DomainWorkCounters& counters) noexcept
        : previous_(std::exchange(activeDomainWorkCounters, &counters))
    {
    }

    ~ScopedDomainWorkCounterSink()
    {
        activeDomainWorkCounters = previous_;
    }

    ScopedDomainWorkCounterSink(const ScopedDomainWorkCounterSink&) = delete;
    ScopedDomainWorkCounterSink& operator=(const ScopedDomainWorkCounterSink&) = delete;

private:
    DomainWorkCounters* previous_ {};
};

} // namespace youknow106::oversampling_audit
