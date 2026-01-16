// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "AudioCaptureCore.h"

/**
 * Delegate called when audio data is captured from microphone
 * @param AudioData - Float buffer of captured audio samples (normalized -1.0 to 1.0)
 */
DECLARE_DELEGATE_OneParam(FOnAudioDataCaptured, const TArray<float>&);

/**
 * Background thread for capturing microphone audio
 * Uses Unreal's AudioCapture module for cross-platform compatibility
 *
 * Audio flows: Microphone → AudioCaptureSynth → Float Buffer → Delegate callback
 */
class FAudioCaptureRunnable : public FRunnable
{
public:
	/**
	 * Constructor
	 * @param InSampleRate - Target sample rate (default: 16000 Hz for speech)
	 */
	explicit FAudioCaptureRunnable(int32 InSampleRate = 16000);

	virtual ~FAudioCaptureRunnable();

	// FRunnable interface
	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;
	virtual void Exit() override;

	/**
	 * Start capturing audio from default microphone
	 * @return true if capture started successfully
	 */
	bool StartCapture();

	/**
	 * Stop capturing audio
	 */
	void StopCapture();

	/**
	 * Check if currently capturing
	 * @return true if capture is active
	 */
	bool IsCapturing() const { return bIsCapturing; }

	/**
	 * Delegate called when audio data is available
	 * Fired from background thread, so callbacks must be thread-safe
	 */
	FOnAudioDataCaptured OnAudioDataCaptured;

private:
	/** Audio capture device */
	TUniquePtr<Audio::FAudioCaptureSynth> AudioCapture;

	/** Target sample rate */
	int32 TargetSampleRate;

	/** Running flag (thread-safe) */
	FThreadSafeBool bIsRunning;

	/** Capturing flag (thread-safe) */
	FThreadSafeBool bIsCapturing;

	/** Thread handle */
	FRunnableThread* Thread;

	/** Captured audio buffer (accumulates samples between callbacks) */
	TArray<float> CapturedBuffer;

	/** Lock for buffer access */
	FCriticalSection BufferLock;

	/** Chunk size for callbacks (samples) */
	int32 ChunkSize;

	/**
	 * Convert stereo to mono if needed
	 * @param AudioData - Input audio buffer
	 * @param NumFrames - Number of frames
	 * @param NumChannels - Number of channels
	 * @return Mono audio buffer
	 */
	TArray<float> ConvertToMono(const float* AudioData, int32 NumFrames, int32 NumChannels);
};
