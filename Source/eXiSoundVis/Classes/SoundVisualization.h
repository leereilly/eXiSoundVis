// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Object.h"

#include "Components/AudioComponent.h"
#include "AudioDecompress.h"
#include "AudioDevice.h"
#include "ActiveSound.h"
#include "Audio.h"
#include "Developer/TargetPlatform/Public/Interfaces/IAudioFormat.h"
#include "Runtime/Engine/Public/VorbisAudioInfo.h"
#include "Sound/SoundWave.h"
#include "Runtime/Core/Public/Async/AsyncWork.h"

#include "SoundVisualization.generated.h"

/**
	Most of this Multithread stuff is from Ramas Wiki Entry.
	Go check it out and credit him for it! (:
*/
class FAudioDecompressWorker : public FRunnable
{

public:

	// Buffer that holds the decompressed data
	uint8* PCMOutBuffer;

	// Bool to check if the Worker finished
	bool bIsFinished;

	// Time Variables
	float CurrentTime;
	float DecompressDuration;

	// Pointer to a SoundWave Object
	class USoundWave* Wave;

	// Some Compressed Audio Information
	ICompressedAudioInfo* AudioInfo;

	static FAudioDecompressWorker* Runnable;

	FRunnableThread* Thread;

	FThreadSafeCounter StopTaskCounter;

	// Function to check if the Worker is finished
	bool IsFinished() const
	{
		return bIsFinished;
	}

	FAudioDecompressWorker(USoundWave* _InWave, uint8* _PCMBuffer, float _StartTime, float _Duration);
	virtual ~FAudioDecompressWorker();

	virtual bool Init();
	virtual uint32 Run();
	virtual void Stop();
	virtual void Exit();

	void EnsureCompletion();

	static FAudioDecompressWorker* Initialize(USoundWave* _InWave, uint8* _PCMBuffer, float _StartTime, float _Duration);

	static void Shutdown();

	static bool IsThreadFinished();
};

/**
 * Example of declaring a UObject in a plugin module
 */
UCLASS(Blueprintable, BlueprintType)
class USoundVisualization : public UObject
{
	GENERATED_BODY()
		
		/// VARIABLES ///

public:

	FAudioDecompressWorker* DecompressWorker = NULL;

	// Holds the Data of our Current Song
	uint8* PCMSampleBuffer = NULL;

	// This is the Current Song
	UPROPERTY(BlueprintReadOnly, Category = "SoundVis | Song Data")
	USoundWave* CurrentSoundWave;

	/// FUNCTIONS ///

public:

	/// De-/Constructurs ///

	~USoundVisualization();

	/// Functions to load Data from the HardDrive ///

	// Function to load a sound file from the HD
	bool LoadSoundFileFromHD(const FString& _FilePath);

	// Function to fill in the RawFile sound data into the USoundWave object
	int FillSoundWaveInfo(class USoundWave* _SW, TArray<uint8>* _RawFile);

	/// Function to decompress the crompressed Data that comes with the .ogg file ///

	void GetPCMDataFromFile(USoundWave* _SoundWave, float _StartTime, float _Duration, bool _Synchronous = false);

	/// Functions to Analyze the current _SoundWave ///

	// Old function to calculate the frequency specturm. The returned values are a bit weird. Don't know what they should mean
	void Old_CalculateFrequencySpectrum(USoundWave* _SoundWave, const bool _bSplitChannels, const float _StartTime, const float _TimeLength, const int32 _SpectrumWidth, TArray< TArray<float> >& _OutSpectrums);

	// My new function to calculate the frequency spectrum. Returns an array of frequencies from 0 to 22000. Amount of different frequencies depends on samplerate of song and Duration of the TimeWindow
	void New_CalculateFrequencySpectrum(USoundWave* _SoundWave, const float _StartTime, const float _Duration, TArray<float>& _OutFrequencies);

	// Old function to calculate the Amplitudes of a song. No new one currently
	void Old_GetAmplitude(USoundWave* _SoundWave, const bool _bSplitChannels, const float _StartTime, const float _TimeLength, const int32 _AmplitudeBuckets, TArray< TArray<float> >& _OutAmplitudes);

	/// Helper Functions ///

	// Function used to get a better value for the FFT. Uses Hann Window
	float GetFFTInValue(const int16 _SampleValue, const int16 _SampleIndex, const int16 _SampleCount);

	// Not working, better not touch! :D
	//UFUNCTION(BlueprintCallable, Category = "Test SV")
	//void GetBPMOfSong(USoundWave* _SoundWave, int32 &BMPOfSong);

	/// Blueprint Versions of the File Data Functions ///

	/**
	* Will load a file (currently .ogg) from your Harddrive and save it in a USoundWave variable
	*
	* @param	_FilePath	Absolute path to the File. E.g.: "C:/Songs/File.ogg"
	*
	*/
	UFUNCTION(BlueprintCallable, Category = "SoundVis | SoundFile")
		bool SV_LoadSoundFileFromHD(const FString _FilePath);

	/**
	* Will get an Array of Names of the Found SoundFiles
	*
	* @param	_DirectoryPath	Path to the Directory in which the Files are (absolute/relative)
	* @param	_bAbsolutePath	Tells if the DirectoryPath is absolute (C:/..) or relative to the GameDirectory
	* @param	_bFullPath		Tells if the returned Names should contain the full Path to the File
	* @param	_FileExtension	This is the Extension the Function should look for. For the Plugin it should be .ogg
	* @param	_SoundFileNames	The Array of found SoundFileNames (Either only the Name.Extension or the full Path/Name.Extension)
	*
	*/
	UFUNCTION(BlueprintCallable, Category = "SoundVis | SoundFile")
		bool SV_LoadAllSoundFileNamesFromHD(const FString _DirectoryPath, const bool _bAbsolutePath, const bool _bFullPath, const FString _FileExtension, TArray<FString>& _SoundFileNames);

	/// Blueprint Versions of the Analyze Functions ///

	/**
	* Will call the OLD CalculateFrequencySpectrum function from BP Side
	*
	* @param	_SoundWave		SoundWave that get's analyzed
	* @param	_Channel		Channel number, 0 is combining them. Only supports up to 2 channels (stereo)
	* @param	_StartTime		StartTime that frames the part of the song that you want to analyize
	* @param	_TimeLength		How long the part is you want to analyze
	* @param	_SpectrumWidth	In how many parts we want to cut the samples
	* @param	_OutSpectrum	Used to return the spectrum data. TArray is SpectrumWidth long
	*
	*/
	UFUNCTION(BlueprintCallable, Category = "SoundVis | Frequency")
		void SV_Old_CalculateFrequencySpectrum(USoundWave* _SoundWave, int32 _Channel, float _StartTime, float _TimeLength, int32 _SpectrumWidth, TArray<float>& _OutSpectrum);

	/**
	* Will call the NEW CalculateFrequencySpectrum function from BP Side
	*
	* @param	_SoundWave		SoundWave that gts analyzed
	* @param	_StartTime		The StartPoint of the TimeWindow we want to analyze
	* @param	_Duration		The length of the TimeWindow we want to analyze
	* @param	_OutFrequencies	Array of float values for x Frequencies from 0 to 22000
	* 
	*/

	UFUNCTION(BlueprintCallable, Category = "SoundVis | Frequency")
		void SV_New_CalculateFrequencySpectrum(USoundWave* _SoundWave, const float _StartTime, const float _Duration, TArray<float>& _OutFrequencies);

	/**
	* Will call the OLD GetAmplitude function from BP Side (no new one right now)
	*
	* @param	_SoundWave			SoundWave that get's analyzed
	* @param	_Channel			Channel number, 0 is combining them. Only supports up to 2 channels (stereo)
	* @param	_StartTime			StartTime that frames the part of the song that you want to analyize
	* @param	_TimeLength			How long the part is you want to analyze
	* @param	_AmplitudeBuckets	AmplitudeBuckets
	* @param	_OutAmplitudes		Used to return the amplitude data. TArray is AmplitudeBuckets long
	*
	*/
	UFUNCTION(BlueprintCallable, Category = "SoundVis | Amplitude")
		void SV_Old_GetAmplitude(USoundWave* _SoundWave, int32 _Channel, float _StartTime, float _TimeLength, int32 _AmplitudeBuckets, TArray<float>& _OutAmplitudes);

	/// Frequency Data Functions ///

	/**
	* This function will return all values for the most common frequencies. It's needs a Frequency Array from the "BP_New_CalculateFrequencySpectrum" function and the matching SoundWave
	*
	* @param	_SoundWave		SoundWave to get specific data from (SampleRate)
	* @param	_Frequencies	Array of float values for different frequencies from 0 to 22000. Can be get by using the "BP_New_CalculateFrequencySpectrum" function
	* @param	F16 to F16000	Different values for the named fequencies (Hz)
	*
	*/
	UFUNCTION(BlueprintCallable, Category = "SoundVis | Frequency Values")
		void SV_GetFrequencyValues(USoundWave* _SoundWave, TArray<float> _Frequencies, float& F16, float& F32, float& F64, float& F128, float& F256, float& F512, float& F1000, float& F2000, float& F4000, float& F8000, float& F16000);

	/**
	* This function will return the value of a specific frequency. It's needs a Frequency Array from the "BP_New_CalculateFrequencySpectrum" function and the matching SoundWave
	*
	* @param	_SoundWave		SoundWave to get specific data from (SampleRate)
	* @param	_Frequencies	Array of float values for different frequencies from 0 to 22000. Can be get by using the "BP_New_CalculateFrequencySpectrum" function
	* @param	_FrequencyValue Float value of the requested frequency
	*
	*/
	UFUNCTION(BlueprintCallable, Category = "SoundVis | Frequency Values")
		void SV_GetSpecificFrequencyValue(USoundWave* _SoundWave, TArray<float> _Frequencies, int32 _WantedFrequency, float& _FrequencyValue);

	/**
	* This function will return the average value for SubBass (20 to 60hz)
	*
	* @param	_SoundWave		SoundWave to get specific data from (SampleRate)
	* @param	_Frequencies	Array of float values for different frequencies from 0 to 22000. Can be get by using the "BP_New_CalculateFrequencySpectrum" function
	* @param	_AverageSubBass Average value of the frequencies from 20 to 60
	*
	*/
	UFUNCTION(BlueprintCallable, Category = "SoundVis | Frequency Values")
		void SV_GetAverageSubBassValue(USoundWave* _SoundWave, TArray<float> _Frequencies, float& _AverageSubBass);

	/**
	* This function will return the average value for Bass (60 to 250hz)
	*
	* @param	_SoundWave		SoundWave to get specific data from (SampleRate)
	* @param	_Frequencies	Array of float values for different frequencies from 0 to 22000. Can be get by using the "BP_New_CalculateFrequencySpectrum" function
	* @param	_AverageBass	Average value of the frequencies from 60 to 250
	*
	*/
	UFUNCTION(BlueprintCallable, Category = "SoundVis | Frequency Values")
		void SV_GetAverageBassValue(USoundWave* _SoundWave, TArray<float> _Frequencies, float& _AverageBass);

	/**
	* This function will return the average value for a given frequency interval e.g.: 20 to 60 (SubBass)
	*
	* @param	_SoundWave			SoundWave to get specific data from (SampleRate)
	* @param	_Frequencies		Array of float values for different frequencies from 0 to 22000. Can be get by using the "BP_New_CalculateFrequencySpectrum" function
	* @param	_StartFrequency		Start Frequency of the Frequency interval
	* @param	_EndFrequency		End Frequency of the Frequency interval
	* @param	_AverageFrequency	Average value of the requested frequency interval
	*
	*/
	UFUNCTION(BlueprintCallable, Category = "SoundVis | Frequency Values")
		void SV_GetAverageFrequencyValueInRange(USoundWave* _SoundWave, TArray<float> _Frequencies, int32 _StartFrequence, int32 _EndFrequence, float& _AverageFrequency);

};