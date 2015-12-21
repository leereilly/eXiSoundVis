// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "eXiSoundVisPrivatePCH.h"
#include "SoundVisualization.h"

/// De-/Constructurs ///

// Destructor to make sure the Buffer is freed again
USoundVisualization::~USoundVisualization()
{
	FMemory::Free(PCMSampleBuffer);
}


/// Functions to load Data from the HardDrive ///

// C++ Version of the LoadSoundFileFromHD function
bool USoundVisualization::LoadSoundFileFromHD(const FString& _FilePath)
{
	// Create new SoundWave Object
	USoundWave* SW = NewObject<USoundWave>(USoundWave::StaticClass());

	if (!SW)
	{
		return false;
	}

	//* If true the song was successfully loaded
	bool bLoaded = false;

	// Array for loaded song file (binary, encoded)
	TArray<uint8> RawFile;

	// Load file into RawFileArray
	bLoaded = FFileHelper::LoadFileToArray(RawFile, _FilePath.GetCharArray().GetData());

	if (bLoaded)
	{
		// Fill the Sound Data into the SoundWave object
		if (RawFile.Num() > 0)
			bLoaded = FillSoundWaveInfo(SW, &RawFile) == 0 ? true : false;
		else
			bLoaded = false;

		// Return Address to the OGG CompressedData part of this SW
		FByteBulkData* bulkData = &SW->CompressedFormatData.GetFormat(TEXT("OGG"));

		bulkData->Lock(LOCK_READ_WRITE);

		// Copy the RawFile Data into the SW CompressedFormatData
		FMemory::Memmove(bulkData->Realloc(RawFile.Num()), RawFile.GetData(), RawFile.Num());

		bulkData->Unlock();
	}
	
	if(!bLoaded)
	{
		return false;
	}

	// Get the PCMSampleBuffer filled
	GetPCMDataFromFile(SW, 0.0f, SW->Duration, true);

	// Return the pointer to the loaded SoundWave
	CurrentSoundWave = SW;

	return true;
}

// Called to get the Wave Info into the SoundWave*
int USoundVisualization::FillSoundWaveInfo(class USoundWave* _SW, TArray<uint8>* _RawFile)
{
	FSoundQualityInfo SQInfo;
	FVorbisAudioInfo VorbisAudioInfo = FVorbisAudioInfo();

	// Save the CompressedData in SQInfo
	if (!VorbisAudioInfo.ReadCompressedInfo(_RawFile->GetData(), _RawFile->Num(), &SQInfo))
	{
		return 1;
	}

	_SW->SoundGroup = ESoundGroup::SOUNDGROUP_Default;
	_SW->NumChannels = SQInfo.NumChannels;
	_SW->Duration = SQInfo.Duration;
	_SW->RawPCMDataSize = SQInfo.SampleDataSize;
	_SW->SampleRate = SQInfo.SampleRate;

	return 0;
}


/// Function to decompress the crompressed Data that comes with the .ogg file ///

void USoundVisualization::GetPCMDataFromFile(USoundWave* _SoundWave, float _StartTime, float _Duration, bool _Synchronous)
{
	check(_SoundWave);

	check(_SoundWave->NumChannels == 1 || _SoundWave->NumChannels == 2)

		if (!_SoundWave->RawPCMData || _SoundWave->RawPCMDataSize <= 0)
		{
			// Get the main audio device
			FAudioDevice* AudioDevice = GEngine->GetMainAudioDevice();

			if (AudioDevice)
			{
				_SoundWave->InitAudioResource(AudioDevice->GetRuntimeFormat(_SoundWave));

				float FBufferSize = _Duration * _SoundWave->SampleRate * _SoundWave->NumChannels;

				const uint32 BufferSize = FMath::FloorToInt(FBufferSize); // Duration * SampleRate * NumChannels

				if (DecompressWorker == NULL)
				{
					if (PCMSampleBuffer)
					{
						FMemory::Free(PCMSampleBuffer);
					}

					PCMSampleBuffer = (uint8*)FMemory::Malloc(BufferSize * 2);
					DecompressWorker = FAudioDecompressWorker::Initialize(_SoundWave, PCMSampleBuffer, _StartTime, _Duration);
				}
				else
				{
					if (DecompressWorker->IsFinished())
					{
						FMemory::Free(PCMSampleBuffer);

						PCMSampleBuffer = (uint8*)FMemory::Malloc(BufferSize * 2);

						DecompressWorker->Shutdown();

						DecompressWorker = FAudioDecompressWorker::Initialize(_SoundWave, PCMSampleBuffer, _StartTime, _Duration);
					}
				}
			}
		}
}


/// Blueprint Versions of the File Data Functions ///

bool USoundVisualization::SV_LoadSoundFileFromHD(const FString _FilePath)
{
	return LoadSoundFileFromHD(_FilePath);
}

bool USoundVisualization::SV_LoadAllSoundFileNamesFromHD(const FString _DirectoryPath, const bool _bAbsolutePath, const bool _bFullPath, const FString _FileExtension, TArray<FString>& _SoundFileNames)
{
	FString FinalPath = _DirectoryPath;

	if (!_bAbsolutePath)
	{
		FinalPath = FPaths::ConvertRelativePathToFull(FPaths::GameDir()) + _DirectoryPath;
	}

	TArray<FString> DirectoriesToSkip;

	IPlatformFile &PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	FLocalTimestampDirectoryVisitor Visitor(PlatformFile, DirectoriesToSkip, DirectoriesToSkip, false);
	PlatformFile.IterateDirectory(*FinalPath, Visitor);

	for (TMap<FString, FDateTime>::TIterator TimestampIt(Visitor.FileTimes); TimestampIt; ++TimestampIt)
	{
		const FString FilePath = TimestampIt.Key();
		const FString FileName = FPaths::GetCleanFilename(FilePath);

		bool bShouldAddFile = true;

		if (!_FileExtension.IsEmpty())
		{
			if (!(FPaths::GetExtension(FileName, false).Equals(_FileExtension, ESearchCase::IgnoreCase)))
			{
				bShouldAddFile = false;
			}
		}

		if (bShouldAddFile)
		{
			_SoundFileNames.Add(_bFullPath ? FilePath : FileName);
		}
	}

	return true;
}


/// Helper Functions ///

float USoundVisualization::GetFFTInValue(const int16 SampleValue, const int16 SampleIndex, const int16 SampleCount)
{
	float FFTValue = SampleValue;

	// Apply the Hann window
	FFTValue *= 0.5f * (1 - FMath::Cos(2 * PI * SampleIndex / (SampleCount - 1)));

	return FFTValue;
}


/// SOUND VIZ ///

/// C++ Versions of the Analyze Functions ///

void USoundVisualization::Old_CalculateFrequencySpectrum(USoundWave* SoundWave, const bool bSplitChannels, const float StartTime, const float TimeLength, const int32 SpectrumWidth, TArray< TArray<float> >& OutSpectrums)
{
	OutSpectrums.Empty();

	const int32 NumChannels = SoundWave->NumChannels;
	if (SpectrumWidth > 0 && NumChannels > 0)
	{
		// Setup the output data
		OutSpectrums.AddZeroed((bSplitChannels ? NumChannels : 1));

		for (int32 ChannelIndex = 0; ChannelIndex < OutSpectrums.Num(); ++ChannelIndex)
		{
			OutSpectrums[ChannelIndex].AddZeroed(SpectrumWidth);
		}

		if (PCMSampleBuffer != NULL)
		{
			float FBufferSize = TimeLength * SoundWave->SampleRate * SoundWave->NumChannels;

			const uint32 BufferSize = FMath::FloorToInt(FBufferSize) * 2; // Duration * SampleRate * NumChannels * 2

			int32 SampleCount = 0;
			int32 SampleCounts[10] = { 0 };

			int32 FirstSample = SoundWave->SampleRate * StartTime;
			int32 LastSample = SoundWave->SampleRate * (StartTime + TimeLength);

			if (NumChannels <= 2)
			{
				SampleCount = SoundWave->RawPCMDataSize / (2 * NumChannels);
			}

			FirstSample = FMath::Min(SampleCount, FirstSample);
			LastSample = FMath::Min(SampleCount, LastSample);

			int32 SamplesToRead = LastSample - FirstSample;

			if (SamplesToRead > 0)
			{
				// Shift the window enough so that we get a power of 2
				int32 PoT = 2;
				while (SamplesToRead > PoT) PoT *= 2;

				FirstSample = FMath::Max(0, FirstSample - (PoT - SamplesToRead) / 2);

				SamplesToRead = PoT;

				// With this equation, LastSample is either Equal or greater than SamplesToRead 
				LastSample = FirstSample + SamplesToRead;

				// If we have more samples than the SampleCount (due to PoT)
				if (LastSample > SampleCount)
				{
					// Regarding above comments, this will either 0 or >0, so FirstSample can't be negative
					FirstSample = LastSample - SamplesToRead;
				}
				// This makes 0 sense! We can't get a negative FirstSample
				if (FirstSample < 0)
				{
					// If we get to this point we can't create a reasonable window so just give up
					return;
				}

				// Create 2 complex number pointer arrays of size 10 and set their value to 0
				kiss_fft_cpx* buf[10] = { 0 };
				kiss_fft_cpx* out[10] = { 0 };

				// Create a one dimensional int Array with the SamplesToRead in it
				int32 Dims[1] = { SamplesToRead };

				// Struct of data kiss_fftnd_state
				kiss_fftnd_cfg stf = kiss_fftnd_alloc(Dims, 1, 0, NULL, NULL);

				// Save the Samples Data wie have to the SamplePtr
				int16* SamplePtr = reinterpret_cast<int16*>(PCMSampleBuffer);

				// STEREO/MONO
				if (NumChannels <= 2)
				{
					for (int32 ChannelIndex = 0; ChannelIndex < NumChannels; ++ChannelIndex)
					{
						// For each Channel, create room for SamplesToRead complex numbers
						buf[ChannelIndex] = (kiss_fft_cpx *)KISS_FFT_MALLOC(sizeof(kiss_fft_cpx) * SamplesToRead);
						out[ChannelIndex] = (kiss_fft_cpx *)KISS_FFT_MALLOC(sizeof(kiss_fft_cpx) * SamplesToRead);
					}

					SamplePtr += (FirstSample * NumChannels);

					for (int32 SampleIndex = 0; SampleIndex < SamplesToRead; ++SampleIndex)
					{
						for (int32 ChannelIndex = 0; ChannelIndex < NumChannels; ++ChannelIndex)
						{
							// Use Window function to get a better result for the Data (Hann Window)
							buf[ChannelIndex][SampleIndex].r = GetFFTInValue(*SamplePtr, SampleIndex, SamplesToRead);
							buf[ChannelIndex][SampleIndex].i = 0.f;

							SamplePtr++;
						}
					}
				}
				// Lost the thin line to what this does
				for (int32 ChannelIndex = 0; ChannelIndex < NumChannels; ++ChannelIndex)
				{
					if (buf[ChannelIndex])
					{
						kiss_fftnd(stf, buf[ChannelIndex], out[ChannelIndex]);
					}
				}


				int32 SamplesPerSpectrum = SamplesToRead / (2 * SpectrumWidth);
				int32 ExcessSamples = SamplesToRead % (2 * SpectrumWidth);

				int32 FirstSampleForSpectrum = 1;

				for (int32 SpectrumIndex = 0; SpectrumIndex < SpectrumWidth; ++SpectrumIndex)
				{
					int32 SamplesRead = 0;
					double SampleSum = 0;
					int32 SamplesForSpectrum = SamplesPerSpectrum + (ExcessSamples-- > 0 ? 1 : 0);

					float Test = 0.0;

					for (int32 ChannelIndex = 0; ChannelIndex < NumChannels; ++ChannelIndex)
					{
						if (out[ChannelIndex])
						{
							if (bSplitChannels)
							{
								SampleSum = 0;
							}

							for (int32 SampleIndex = 0; SampleIndex < SamplesForSpectrum; ++SampleIndex)
							{
								float PostScaledR = out[ChannelIndex][FirstSampleForSpectrum + SampleIndex].r * 2.f / SamplesToRead;
								float PostScaledI = out[ChannelIndex][FirstSampleForSpectrum + SampleIndex].i * 2.f / SamplesToRead;

								float Val = 10.f * FMath::LogX(10.f, FMath::Square(PostScaledR) + FMath::Square(PostScaledI));
								Test += FMath::Sqrt(FMath::Square(out[ChannelIndex][SampleIndex].r) + FMath::Square(out[ChannelIndex][SampleIndex].i));
								SampleSum += Val;
							}

							if (bSplitChannels)
							{
								OutSpectrums[ChannelIndex][SpectrumIndex] = (float)(SampleSum / SamplesForSpectrum);

							}
							SamplesRead += SamplesForSpectrum;
						}
					}

					if (!bSplitChannels)
					{
						//OutSpectrums[0][SpectrumIndex] = (float)(SampleSum / SamplesRead);
						OutSpectrums[0][SpectrumIndex] = Test;
					}

					FirstSampleForSpectrum += SamplesForSpectrum;
				}

				KISS_FFT_FREE(stf);
				for (int32 ChannelIndex = 0; ChannelIndex < NumChannels; ++ChannelIndex)
				{
					if (buf[ChannelIndex])
					{
						KISS_FFT_FREE(buf[ChannelIndex]);
						KISS_FFT_FREE(out[ChannelIndex]);
					}
				}
			}
		}
	}
}

void USoundVisualization::New_CalculateFrequencySpectrum(USoundWave* _SoundWave, const float _StartTime, const float _Duration, TArray<float>& _OutFrequencies)
{
	_OutFrequencies.Empty();

	const int32 NumChannels = _SoundWave->NumChannels;

	if (NumChannels > 0)
	{
		// For 0.1 seconds and 44100 samples per second, this would be 4410 samples, half of them will be returned by FFT, so 2205 array size
		/*
		int32 NumFreq = 0;

		int32 Temp = ((int32)(_SoundWave->SampleRate * _Duration)) % 2;

		if (Temp > 0)
		{
		NumFreq = 1;
		}

		NumFreq += (int32)((_SoundWave->SampleRate * _Duration) / 2);

		_OutFrequencies.AddZeroed(NumFreq);*/

		if (PCMSampleBuffer != NULL)
		{
			float FBufferSize = _Duration * _SoundWave->SampleRate * _SoundWave->NumChannels;

			const uint32 BufferSize = FMath::FloorToInt(FBufferSize) * 2; // Duration * SampleRate * NumChannels * 2

																		  // Get first and last sample
			int32 FirstSample = _SoundWave->SampleRate * _StartTime;
			int32 LastSample = _SoundWave->SampleRate * (_StartTime + _Duration);

			// Get Maximum amount of samples in this song
			int32 SampleCount = _SoundWave->RawPCMDataSize / (2 * NumChannels);

			FirstSample = FMath::Min(SampleCount, FirstSample);
			LastSample = FMath::Min(SampleCount, LastSample);

			// Actual samples we gonna read
			int32 SamplesToRead = LastSample - FirstSample;

			if (SamplesToRead > 0)
			{
				// Shift the window enough so that we get a power of 2
				int32 PoT = 2;
				while (SamplesToRead > PoT) PoT *= 2;

				FirstSample = FMath::Max(0, FirstSample - (PoT - SamplesToRead) / 2);

				SamplesToRead = PoT;

				// With this equation, LastSample is either Equal or greater than SamplesToRead 
				LastSample = FirstSample + SamplesToRead;

				// If we have more samples than the SampleCount (due to PoT)
				if (LastSample > SampleCount)
				{
					// Regarding above comments, this will either 0 or >0, so FirstSample can't be negative
					FirstSample = LastSample - SamplesToRead;
				}
				// This makes 0 sense! We can't get a negative FirstSample
				if (FirstSample < 0)
				{
					// If we get to this point we can't create a reasonable window so just give up
					return;
				}

				kiss_fft_cpx* buf[2] = { 0 };
				kiss_fft_cpx* out[2] = { 0 };

				// Create a one dimensional int Array with the SamplesToRead in it
				int32 Dims[1] = { SamplesToRead };

				// Struct of data kiss_fftnd_state
				kiss_fftnd_cfg stf = kiss_fftnd_alloc(Dims, 1, 0, NULL, NULL);

				// Save the Samples Data wie have to the SamplePtr
				int16* SamplePtr = reinterpret_cast<int16*>(PCMSampleBuffer);

				for (int32 ChannelIndex = 0; ChannelIndex < NumChannels; ChannelIndex++)
				{
					buf[ChannelIndex] = (kiss_fft_cpx *)KISS_FFT_MALLOC(sizeof(kiss_fft_cpx) * SamplesToRead);
					out[ChannelIndex] = (kiss_fft_cpx *)KISS_FFT_MALLOC(sizeof(kiss_fft_cpx) * SamplesToRead);
				}

				SamplePtr += (FirstSample * NumChannels);

				for (int32 SampleIndex = 0; SampleIndex < SamplesToRead; ++SampleIndex)
				{
					//if (SampleIndex + FirstSample <= SampleCount)
					//{
					for (int32 ChannelIndex = 0; ChannelIndex < NumChannels; ChannelIndex++)
					{
						// Use Window function to get a better result for the Data (Hann Window)
						buf[ChannelIndex][SampleIndex].r = GetFFTInValue(*SamplePtr, SampleIndex, SamplesToRead);
						buf[ChannelIndex][SampleIndex].i = 0.f;

						SamplePtr++;
					}
					//	}
					/*else
					{
					for (int32 ChannelIndex = 0; ChannelIndex < NumChannels; ChannelIndex++)
					{
					// Fill with zeros
					buf[ChannelIndex][SampleIndex].r = 0.f;
					buf[ChannelIndex][SampleIndex].i = 0.f;
					}
					}*/
				}

				for (int32 ChannelIndex = 0; ChannelIndex < NumChannels; ChannelIndex++)
				{
					if (buf[ChannelIndex])
					{
						kiss_fftnd(stf, buf[ChannelIndex], out[ChannelIndex]);
					}
				}

				_OutFrequencies.AddZeroed(SamplesToRead / 2);

				for (int32 SampleIndex = 0; SampleIndex < SamplesToRead / 2; ++SampleIndex)
				{
					float ChannelSum = 0.0f;

					for (int32 ChannelIndex = 0; ChannelIndex < NumChannels; ++ChannelIndex)
					{
						if (out[ChannelIndex])
						{
							ChannelSum += FMath::Sqrt(FMath::Square(out[ChannelIndex][SampleIndex].r) + FMath::Square(out[ChannelIndex][SampleIndex].i));
						}
					}

					_OutFrequencies[SampleIndex] = ChannelSum / NumChannels;
				}

				KISS_FFT_FREE(stf);
				for (int32 ChannelIndex = 0; ChannelIndex < NumChannels; ++ChannelIndex)
				{
					if (buf[ChannelIndex])
					{
						KISS_FFT_FREE(buf[ChannelIndex]);
						KISS_FFT_FREE(out[ChannelIndex]);
					}
				}
			}
		}
	}
}

void USoundVisualization::Old_GetAmplitude(USoundWave* SoundWave, const bool bSplitChannels, const float StartTime, const float TimeLength, const int32 AmplitudeBuckets, TArray< TArray<float> >& OutAmplitudes)
{
	OutAmplitudes.Empty();

	const int32 NumChannels = SoundWave->NumChannels;
	if (AmplitudeBuckets > 0 && NumChannels > 0)
	{
		// Setup the output data
		OutAmplitudes.AddZeroed((bSplitChannels ? NumChannels : 1));
		for (int32 ChannelIndex = 0; ChannelIndex < OutAmplitudes.Num(); ++ChannelIndex)
		{
			OutAmplitudes[ChannelIndex].AddZeroed(AmplitudeBuckets);
		}

		// check if there is any raw sound data
		if (SoundWave->RawData.GetBulkDataSize() > 0)
		{
			// Lock raw wave data.
			uint8* RawWaveData = (uint8*)SoundWave->RawData.Lock(LOCK_READ_ONLY);
			int32 RawDataSize = SoundWave->RawData.GetBulkDataSize();

			GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Black, FString("RawDataSize = ") + FString::FromInt(RawDataSize));

			FWaveModInfo WaveInfo;

			// parse the wave data
			if (WaveInfo.ReadWaveHeader(RawWaveData, RawDataSize, 0))
			{
				uint32 SampleCount = 0;
				uint32 SampleCounts[10] = { 0 };

				uint32 FirstSample = *WaveInfo.pSamplesPerSec * StartTime;
				uint32 LastSample = *WaveInfo.pSamplesPerSec * (StartTime + TimeLength);

				if (NumChannels <= 2)
				{
					SampleCount = WaveInfo.SampleDataSize / (2 * NumChannels);
				}

				FirstSample = FMath::Min(SampleCount, FirstSample);
				LastSample = FMath::Min(SampleCount, LastSample);

				int16* SamplePtr = reinterpret_cast<int16*>(WaveInfo.SampleDataStart);
				if (NumChannels <= 2)
				{
					SamplePtr += FirstSample;
				}

				uint32 SamplesPerAmplitude = (LastSample - FirstSample) / AmplitudeBuckets;
				uint32 ExcessSamples = (LastSample - FirstSample) % AmplitudeBuckets;

				for (int32 AmplitudeIndex = 0; AmplitudeIndex < AmplitudeBuckets; ++AmplitudeIndex)
				{
					if (NumChannels <= 2)
					{
						int64 SampleSum[2] = { 0 };
						uint32 SamplesToRead = SamplesPerAmplitude + (ExcessSamples-- > 0 ? 1 : 0);
						for (uint32 SampleIndex = 0; SampleIndex < SamplesToRead; ++SampleIndex)
						{
							for (int32 ChannelIndex = 0; ChannelIndex < NumChannels; ++ChannelIndex)
							{
								SampleSum[ChannelIndex] += FMath::Abs(*SamplePtr);
								SamplePtr++;
							}
						}
						for (int32 ChannelIndex = 0; ChannelIndex < NumChannels; ++ChannelIndex)
						{
							OutAmplitudes[(bSplitChannels ? ChannelIndex : 0)][AmplitudeIndex] = SampleSum[ChannelIndex] / (float)SamplesToRead;
						}
					}
				}
			}

			SoundWave->RawData.Unlock();
		}
	}
}


/// Blueprint Versions of the Analyze Functions ///

void USoundVisualization::SV_Old_CalculateFrequencySpectrum(USoundWave* SoundWave, int32 Channel, float StartTime, float TimeLength, int32 SpectrumWidth, TArray<float>& OutSpectrum)
{
	OutSpectrum.Empty();

	if (SoundWave)
	{
		if (SpectrumWidth <= 0)
		{
			//UE_LOG(LogSoundVisualization, Warning, TEXT("Invalid SpectrumWidth (%d)"), SpectrumWidth);
		}
		else if (Channel < 0)
		{
			//UE_LOG(LogSoundVisualization, Warning, TEXT("Invalid Channel (%d)"), Channel);
		}
		else
		{
			TArray< TArray<float> > Spectrums;

			Old_CalculateFrequencySpectrum(SoundWave, (Channel != 0), StartTime, TimeLength, SpectrumWidth, Spectrums);

			if (Channel == 0)
			{
				OutSpectrum = Spectrums[0];
			}
			else if (Channel <= Spectrums.Num())
			{
				OutSpectrum = Spectrums[Channel - 1];
			}
			else
			{
				//UE_LOG(LogSoundVisualization, Warning, TEXT("Requested channel %d, sound only has %d channels"), SoundWave->NumChannels);
			}
		}
	}
}

void USoundVisualization::SV_New_CalculateFrequencySpectrum(USoundWave* _SoundWave, const float _StartTime, const float _Duration, TArray<float>& _OutFrequencies)
{
	_OutFrequencies.Empty();

	if (_SoundWave)
	{
		New_CalculateFrequencySpectrum(_SoundWave, _StartTime, _Duration, _OutFrequencies);
	}

}

void USoundVisualization::SV_Old_GetAmplitude(USoundWave* SoundWave, int32 Channel, float StartTime, float TimeLength, int32 AmplitudeBuckets, TArray<float>& OutAmplitudes)
{
	OutAmplitudes.Empty();

	if (SoundWave)
	{
		if (Channel >= 0)
		{
			TArray< TArray<float> > Amplitudes;

			Old_GetAmplitude(SoundWave, (Channel != 0), StartTime, TimeLength, AmplitudeBuckets, Amplitudes);

			if (Channel == 0)
			{
				OutAmplitudes = Amplitudes[0];
			}
			else if (Channel <= Amplitudes.Num())
			{
				OutAmplitudes = Amplitudes[Channel - 1];
			}
		}
	}
}


/// NOT WORKING! DON'T TOUCH!  ///

float Energie(int16* _Data, int32 _Offset, int32 _Window, int32 _SongLength)
{
	float Energy = 0.f;

	for (int32 i = _Offset; (i < _Offset + _Window) && (i < _SongLength); i++)
	{
		Energy = Energy + _Data[i] * _Data[i] / _Window;
	}

	return Energy;
}
/*
void USoundVisualization::GetBPMOfSong(USoundWave* _SoundWave, int32 &BMPOfSong)
{
	// Start: Setup Values
	int32 SongLength = _SoundWave->RawPCMDataSize / (2 * _SoundWave->NumChannels);;

	float* Energy1024 = new float[SongLength / 1024];
	float* Energy44100 = new float[SongLength / 1024];
	float* Conv = new float[SongLength / 1024];
	float* Beat = new float[SongLength / 1024];
	float* Energie_Peak = new float[SongLength / 1024 + 21];
	for (int32 i = 0; i < SongLength / 1024 + 21; i++)
		Energie_Peak[i] = 0;

	if (PCMSampleBuffer == NULL)
		return;

	int16* SamplePtr = reinterpret_cast<int16*>(PCMSampleBuffer);
	// End: Setup Values

	for (int32 i = 0; i < SongLength / 1024; i++)
	{
		Energy1024[i] = Energie(SamplePtr, i * 1024, 4096, SongLength);
	}

	////

	Energy44100[0] = 0;

	float Sum = 0.f;

	for (int32 i = 0; i < 43; i++)
	{
		Sum += Energy1024[i];
	}

	Energy44100[0] = Sum / 43;

	for (int32 i = 1; i < SongLength / 1024; i++)
	{
		Sum = Sum - Energy1024[i - 1] + Energy1024[i + 42];
		Energy44100[i] = Sum / 43;
	}

	////

	for (int32 i = 21; i < SongLength / 1024; i++)
	{
		if (Energy1024[i] > 1.3 * Energy44100[i - 21])
		{
			Energie_Peak[i] = 1;
		}
	}

	///

	TArray<int32> T;
	int32 I_Prec = 0;

	for (int32 i = 1; i < SongLength / 1024; i++)
	{
		if ((Energie_Peak[i] == 1) && (Energie_Peak[i - 1] == 0))
		{
			int32 Di = i - I_Prec;
			if (Di > 5)
			{
				T.Add(Di);
				I_Prec = i;
			}
		}
	}

	int32 T_Occ_Max = 0;
	float T_Occ_Moy = 0.f;

	int32 Occurences_T[86];

	for (int32 i = 0; i < 86; i++)
		Occurences_T[i] = 0;

	for (int32 i = 1; i < T.Num(); i++)
	{
		if (T[i] <= 86)
		{
			Occurences_T[T[i]]++;
		}
	}

	int Occ_Max = 0;

	for (int32 i = 1; i < 86; i++)
	{
		if (Occurences_T[i] > Occ_Max)
		{
			T_Occ_Max = i;
			Occ_Max = Occurences_T[i];
		}
	}

	int Voisin = T_Occ_Max - 1;
	if (Occurences_T[T_Occ_Max + 1] > Occurences_T[Voisin])
		Voisin = T_Occ_Max + 1;

	float Div = Occurences_T[T_Occ_Max] + Occurences_T[Voisin];

	if (Div == 0)
		T_Occ_Moy = 0;
	else
		T_Occ_Moy = (float)(T_Occ_Max * Occurences_T[T_Occ_Max] + (Voisin)* Occurences_T[Voisin]) / Div;

	int32 Tempo = (int32) 60.f / (T_Occ_Moy* (1024.f / 44100.f));

	GEngine->AddOnScreenDebugMessage(-1, 100.f, FColor::Red, FString::FromInt(Tempo));

	BMPOfSong = Tempo;
}
*/


/// Multithreading Functions (Check Ramas Wiki Entry if you don't understand that stuff :X) ///

FAudioDecompressWorker* FAudioDecompressWorker::Runnable = NULL;

FAudioDecompressWorker::FAudioDecompressWorker(USoundWave* _InWave, uint8* _PCMBuffer, float _StartTime, float _Duration)
	: Wave(_InWave)
	, CurrentTime(_StartTime)
	, DecompressDuration(_Duration)
	, PCMOutBuffer(_PCMBuffer)
	, AudioInfo(NULL)
{
	if (GEngine && GEngine->GetMainAudioDevice())
	{
		AudioInfo = GEngine->GetMainAudioDevice()->CreateCompressedAudioInfo(Wave);
	}

	Thread = FRunnableThread::Create(this, TEXT("FAudioDecompressWorker"), 0, TPri_BelowNormal); //windows default = 8mb for thread, could specify more
}

FAudioDecompressWorker::~FAudioDecompressWorker()
{
	delete Thread;
	Thread = NULL;
}

bool FAudioDecompressWorker::Init()
{
	return true;
}

uint32 FAudioDecompressWorker::Run()
{
	FPlatformProcess::Sleep(0.03);

	bIsFinished = false;

	if (!Wave)
	{
		bIsFinished = true;

		return 0;
	}

	if (AudioInfo != NULL)
	{
		FSoundQualityInfo QualityInfo = { 0 };

		// Parse the audio header for the relevant information
		if (!(Wave->ResourceData))
		{
			bIsFinished = true;

			return 0;
		}

		if (AudioInfo->ReadCompressedInfo(Wave->ResourceData, Wave->ResourceSize, &QualityInfo))
		{
			FScopeCycleCounterUObject WaveObject(Wave);

			// Extract the data
			Wave->SampleRate = QualityInfo.SampleRate;
			Wave->NumChannels = QualityInfo.NumChannels;

			if (QualityInfo.Duration > 0.0f)
			{
				Wave->Duration = QualityInfo.Duration;
			}

			const uint32 PCMBufferSize = DecompressDuration * Wave->SampleRate * Wave->NumChannels;

			AudioInfo->SeekToTime(CurrentTime);
			AudioInfo->ReadCompressedData(PCMOutBuffer, false, PCMBufferSize * 2);
		}
		else if (Wave->DecompressionType == DTYPE_RealTime || Wave->DecompressionType == DTYPE_Native)
		{
			Wave->RemoveAudioResource();
		}

		delete AudioInfo;
	}

	bIsFinished = true;

	return 0;
}

void FAudioDecompressWorker::Stop()
{
	StopTaskCounter.Increment();
}

FAudioDecompressWorker* FAudioDecompressWorker::Initialize(USoundWave* _InWave, uint8* _PCMBuffer, float _StartTime, float _Duration)
{
	if (!Runnable && FPlatformProcess::SupportsMultithreading())
	{
		Runnable = new FAudioDecompressWorker(_InWave, _PCMBuffer, _StartTime, _Duration);
	}

	return Runnable;
}

void FAudioDecompressWorker::EnsureCompletion()
{
	Stop();
	Thread->WaitForCompletion();
}

void FAudioDecompressWorker::Shutdown()
{
	if (Runnable)
	{
		Runnable->EnsureCompletion();
		delete Runnable;
		Runnable = NULL;
	}
}

bool FAudioDecompressWorker::IsThreadFinished()
{
	if (Runnable)
	{
		return Runnable->IsFinished();
	}

	return true;
}

void FAudioDecompressWorker::Exit()
{
	Thread->Kill();
}


/// Frequency Data Functions ///

// Function to return the most commen frequencies
void USoundVisualization::SV_GetFrequencyValues(USoundWave* _SoundWave, TArray<float> _Frequencies, float& F16, float& F32, float& F64, float& F128, float& F256, float& F512, float& F1000, float& F2000, float& F4000, float& F8000, float& F16000)
{
	if (_SoundWave && _Frequencies.Num() > 0)
	{
		F16 = _Frequencies[(int32)(16 * _Frequencies.Num() * 2 / _SoundWave->SampleRate)];
		F32 = _Frequencies[(int32)(32 * _Frequencies.Num() * 2 / _SoundWave->SampleRate)];
		F64 = _Frequencies[(int32)(64 * _Frequencies.Num() * 2 / _SoundWave->SampleRate)];
		F128 = _Frequencies[(int32)(128 * _Frequencies.Num() * 2 / _SoundWave->SampleRate)];
		F256 = _Frequencies[(int32)(256 * _Frequencies.Num() * 2 / _SoundWave->SampleRate)];
		F512 = _Frequencies[(int32)(512 * _Frequencies.Num() * 2 / _SoundWave->SampleRate)];
		F1000 = _Frequencies[(int32)(1000 * _Frequencies.Num() * 2 / _SoundWave->SampleRate)];
		F2000 = _Frequencies[(int32)(2000 * _Frequencies.Num() * 2 / _SoundWave->SampleRate)];
		F4000 = _Frequencies[(int32)(4000 * _Frequencies.Num() * 2 / _SoundWave->SampleRate)];
		F8000 = _Frequencies[(int32)(8000 * _Frequencies.Num() * 2 / _SoundWave->SampleRate)];
		F16000 = _Frequencies[(int32)(16000 * _Frequencies.Num() * 2 / _SoundWave->SampleRate)];
	}
	else
	{
		F16 = 0.0f;
		F32 = 0.0f;
		F64 = 0.0f;
		F128 = 0.0f;
		F256 = 0.0f;
		F512 = 0.0f;
		F1000 = 0.0f;
		F2000 = 0.0f;
		F4000 = 0.0f;
		F8000 = 0.0f;
		F16000 = 0.0f;
	}
}

// Function to get the nearly exact value of a given frequency
void USoundVisualization::SV_GetSpecificFrequencyValue(USoundWave* _SoundWave, TArray<float> _Frequencies, int32 _WantedFrequency, float& _FrequencyValue)
{
	if (_SoundWave && _Frequencies.Num() > 0)
	{
		_FrequencyValue = _Frequencies[(int32)(_WantedFrequency * _Frequencies.Num() * 2 / _SoundWave->SampleRate)];
	}
}

// Function to get the average value of the subbass frequencies
void USoundVisualization::SV_GetAverageSubBassValue(USoundWave* _SoundWave, TArray<float> _Frequencies, float& _AverageSubBass)
{
	SV_GetAverageFrequencyValueInRange(_SoundWave, _Frequencies, 20, 60, _AverageSubBass);
}

// Function to get the average value of the bass frequencies
void USoundVisualization::SV_GetAverageBassValue(USoundWave* _SoundWave, TArray<float> _Frequencies, float& _AverageBass)
{
	SV_GetAverageFrequencyValueInRange(_SoundWave, _Frequencies, 60, 250, _AverageBass);
}

// Function to calculate the average frequency value of a given interval
void USoundVisualization::SV_GetAverageFrequencyValueInRange(USoundWave* _SoundWave, TArray<float> _Frequencies, int32 _StartFrequence, int32 _EndFrequence, float& _AverageFrequency)
{
	if (_StartFrequence >= _EndFrequence || _StartFrequence < 20 || _EndFrequence > 22000)
		return;

	int32 FStart = (int32)(_StartFrequence  * _Frequencies.Num() * 2 / _SoundWave->SampleRate);
	int32 FEnd = (int32)(_EndFrequence * _Frequencies.Num() * 2 / _SoundWave->SampleRate);

	int32 NumberOfFrequencies = 0;

	float ValueSum = 0.0f;

	for (int i = FStart; i <= FEnd; i++)
	{
		NumberOfFrequencies++;

		ValueSum += _Frequencies[i];
	}

	_AverageFrequency = ValueSum / NumberOfFrequencies;
}
