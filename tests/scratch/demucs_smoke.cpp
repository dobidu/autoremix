// tests/scratch/demucs_smoke.cpp
//
// End-to-end smoke for plugin/include/dsp/NativeDemucsSeparator.h.
// Loads a WAV, runs separate_demucs, writes 4 stem WAVs next to the
// input. Used once before Phase 27-01 wire-up to confirm the C++
// ORT path actually produces musical-sounding stems.
//
// Usage:
//   demucs_smoke <input.wav> <htdemucs.onnx>
//
// Output:
//   <input>_vocals.wav  <input>_drums.wav
//   <input>_bass.wav    <input>_other.wav
//
// Constraints (same as the separator):
//   - input must be 44.1 kHz (mono or stereo)
//   - htdemucs.onnx must be the artifact produced by tools/export_demucs_onnx.py

#include <chrono>
#include <cstdio>
#include <cstdlib>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>

#include "dsp/NativeDemucsSeparator.h"

namespace {

juce::AudioBuffer<float>
read_wav(juce::AudioFormatManager& fmt, const juce::File& f, double& sr_out)
{
    juce::AudioBuffer<float> buf;
    std::unique_ptr<juce::AudioFormatReader> reader(fmt.createReaderFor(f));
    if (!reader) {
        std::fprintf(stderr, "cannot read %s\n",
                     f.getFullPathName().toRawUTF8());
        return buf;
    }
    sr_out = reader->sampleRate;
    buf.setSize(static_cast<int>(reader->numChannels),
                static_cast<int>(reader->lengthInSamples));
    reader->read(&buf, 0, buf.getNumSamples(), 0, true, true);
    return buf;
}

bool
write_wav(juce::AudioFormatManager& fmt,
          const juce::File&         dest,
          const juce::AudioBuffer<float>& buf,
          double sr)
{
    dest.deleteFile();
    std::unique_ptr<juce::FileOutputStream> os(dest.createOutputStream());
    if (!os) return false;
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer(
        wav.createWriterFor(os.get(), sr, static_cast<unsigned>(buf.getNumChannels()),
                            24, {}, 0));
    if (!writer) return false;
    os.release();   // writer owns it now
    return writer->writeFromAudioSampleBuffer(buf, 0, buf.getNumSamples());
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 3) {
        std::fprintf(stderr,
            "usage: %s <input.wav> <htdemucs.onnx>\n", argv[0]);
        return 2;
    }
    const juce::File in_file { juce::String(argv[1]) };
    const juce::File model   { juce::String(argv[2]) };

    if (!in_file.existsAsFile()) {
        std::fprintf(stderr, "input not found: %s\n", argv[1]);
        return 2;
    }
    if (!model.existsAsFile()) {
        std::fprintf(stderr, "model not found: %s\n", argv[2]);
        return 2;
    }

    juce::AudioFormatManager fmt;
    fmt.registerBasicFormats();

    double sr = 0.0;
    auto input = read_wav(fmt, in_file, sr);
    if (input.getNumSamples() == 0) return 3;

    std::printf("[smoke] input  = %s\n", in_file.getFullPathName().toRawUTF8());
    std::printf("[smoke] model  = %s\n", model.getFullPathName().toRawUTF8());
    std::printf("[smoke] sr     = %.0f Hz, channels=%d, samples=%d\n",
                sr, input.getNumChannels(), input.getNumSamples());

    if (std::abs(sr - 44100.0) > 0.5) {
        std::fprintf(stderr,
            "[smoke] input must be 44.1 kHz (got %.0f); resample first\n", sr);
        return 4;
    }

    const auto t0 = std::chrono::steady_clock::now();
    auto result = autoremix::dsp::separators::separate_demucs(
        input, sr, model,
        [](double p) { std::printf("\r[smoke] progress %5.1f%%", p * 100.0); std::fflush(stdout); });
    std::printf("\n");
    const auto t1 = std::chrono::steady_clock::now();
    const double elapsed_s =
        std::chrono::duration<double>(t1 - t0).count();

    if (!result.ok) {
        std::fprintf(stderr, "[smoke] separation FAILED: %s\n",
                     result.error.toRawUTF8());
        return 5;
    }

    std::printf("[smoke] separation OK in %.1f s "
                "(%.2fx realtime)\n",
                elapsed_s,
                static_cast<double>(input.getNumSamples())
                    / sr / elapsed_s);

    auto write_one = [&](const juce::AudioBuffer<float>& b, const char* tag) {
        const auto out =
            in_file.getParentDirectory().getChildFile(
                in_file.getFileNameWithoutExtension() + "_" + tag + ".wav");
        const bool ok = write_wav(fmt, out, b, sr);
        std::printf("[smoke] %-8s → %s (%s)\n",
                    tag,
                    out.getFullPathName().toRawUTF8(),
                    ok ? "ok" : "FAIL");
        return ok;
    };

    bool all_ok = true;
    all_ok &= write_one(result.stems.vocals, "vocals");
    all_ok &= write_one(result.stems.drums,  "drums");
    all_ok &= write_one(result.stems.bass,   "bass");
    all_ok &= write_one(result.stems.other,  "other");

    std::printf("[smoke] %s\n", all_ok ? "DONE" : "PARTIAL FAILURE");
    return all_ok ? 0 : 6;
}
